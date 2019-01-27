#include <chrono>
#include <iostream>
#include <thread>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "assembly/format_h.h"
#include "assembly/write_block_h.h"
#include "boost/format.hpp"
#include "boost/program_options/cmdline.hpp"
#include "boost/program_options/options_description.hpp"
#include "boost/program_options/parsers.hpp"
#include "boost/program_options/variables_map.hpp"
#include "iec_host_lib.h"
#include "utils.h"

namespace po = boost::program_options;

using namespace std::chrono_literals;

// Max amount of data for a single M-W command is 35 bytes.
static const size_t kMaxMWSize = 35;

// Memory start location in the 1541's memory for our format routine.
static const size_t kFormatCodeStart = 0x500;
// We skip the first three bytes, because they're a jmp into the format job.
static const size_t kFormatEntryPoint = 0x503;

// Memory start location in the 1541's memory for write block.
static const size_t kWriteBlockCodeStart = 0x500;
// We skip the first three bytes, because they're a jmp into the write job.
static const size_t kWriteBlockEntryPoint = 0x503;

// Convert input to a string of BCD hex numbers.
static std::string BytesToHex(const std::string &input) {
  std::string result;
  for (auto &c : input) {
    result += (boost::format("%02x") %
               static_cast<unsigned int>(static_cast<unsigned char>(c)))
                  .str();
  }
  return result;
}

// GetTrackSector translates from a sector index to corresponding
// track and (track local) sector number according to a hardcoded
// schema matching the 1541's sectors / track configuration.
static void GetTrackSector(unsigned int s, unsigned int *track,
                           unsigned int *sector) {
  const unsigned int area1_sectors = 357;
  const unsigned int area12_sectors = area1_sectors + 133;
  const unsigned int area123_sectors = area12_sectors + 108;
  if (s >= area123_sectors) {
    *sector = s - area123_sectors;
    *track = 31 + (*sector / 17);
    *sector = *sector % 17;
    return;
  }
  if (s >= area12_sectors) {
    *sector = s - area12_sectors;
    *track = 25 + (*sector / 18);
    *sector = *sector % 18;
    return;
  }
  if (s >= area1_sectors) {
    *sector = s - area1_sectors;
    *track = 18 + (*sector / 19);
    *sector = *sector % 19;
    return;
  }
  *sector = s;
  *track = (*sector / 21) + 1;
  *sector = *sector % 21;
}

static bool WriteMemory(IECBusConnection *connection, int target,
                        unsigned short int target_address, size_t num_bytes,
                        const unsigned char *source, IECStatus *status) {
  std::cout << "WriteMemory, num_bytes = " << num_bytes << std::endl;

  size_t bytes_written = 0;
  while (num_bytes - bytes_written > 0) {
    std::string request = "M-W";
    unsigned short int mem_pos = target_address + bytes_written;
    request.append(1, mem_pos & 0xff);
    request.append(1, mem_pos >> 8);
    size_t num_data_bytes = std::min(kMaxMWSize, num_bytes - bytes_written);
    request.append(1, num_data_bytes);
    for (size_t i = 0; i < num_data_bytes; ++i) {
      request.append(1, source[bytes_written + i]);
    }
    if (!connection->WriteToChannel(target, 15, request, status)) {
      std::cout << "WriteToChannel: " << status->message << std::endl;
      return false;
    }
    std::string response;
    if (!connection->ReadFromChannel(target, 15, &response, status)) {
      std::cout << "ReadFromChannel: " << status->message << std::endl;
      return false;
    }
    if (response != "00, OK,00,00\r") {
      SetError(IECStatus::DRIVE_ERROR, response, status);
      return false;
    }
    bytes_written += num_data_bytes;
  }
  return true;
}

int main(int argc, char *argv[]) {
  std::cout << "IEC Bus disc copy utility." << std::endl
            << "Copyright (c) 2018 Andreas Eckleder" << std::endl
            << std::endl;

  std::string arduino_device;
  int serial_speed = 0;
  bool verify = false;
  std::string source;
  int target = 9;
  bool format = false;

  po::options_description desc("Options");
  desc.add_options()("help", "usage overview")(
      "serial",
      po::value<std::string>(&arduino_device)->default_value("/dev/ttyUSB0"),
      "serial interface to use")(
      "speed", po::value<int>(&serial_speed)->default_value(57600),
      "baud rate")("verify", po::value<bool>(&verify)->default_value(false),
                   "verify copy")(
      "source", po::value<std::string>(&source)->default_value(""),
      "disk image to copy from")(
      "target", po::value<int>(&target)->default_value(9), "device to copy to")(
      "format", po::value<bool>(&format)->default_value(false),
      "format disc prior to copying");

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  if (vm.count("help")) {
    std::cout << desc << std::endl;
    return 1;
  }

  IECStatus status;
  std::unique_ptr<IECBusConnection> connection(IECBusConnection::Create(
      arduino_device, serial_speed,
      [](char level, const std::string &channel, const std::string &message) {
        std::cout << level << ":" << channel << ": " << message << std::endl;
      },
      &status));
  if (!connection) {
    std::cout << status.message << std::endl;
    return 1;
  }

  if (!connection->Reset(&status)) {
    std::cout << "Reset: " << status.message << std::endl;
    return 1;
  }

  // Accessing the command channel is always ok, no open call necessary.
  std::string response;
  if (!connection->ReadFromChannel(target, 15, &response, &status)) {
    std::cout << "ReadFromChannel: " << status.message << std::endl;
    return 1;
  }
  std::cout << "Initial drive status: " << response << std::endl;

  if (format) {
    std::cout << "Formatting disc..." << std::endl;

    if (!WriteMemory(connection.get(), target, kFormatCodeStart,
                     sizeof(format_bin), format_bin, &status)) {
      std::cout << "WriteMemory: " << status.message << std::endl;
      return 1;
    }

    std::string request = "M-E";
    request.append(1, char(kFormatEntryPoint & 0xff));
    request.append(1, char(kFormatEntryPoint >> 8));
    std::cout << "Sending M-E" << std::endl;
    if (!connection->WriteToChannel(target, 15, request, &status)) {
      std::cout << "WriteToChannel: " << status.message << std::endl;
      return 1;
    }

    std::cout << "Waiting for formatting to complete..." << std::endl;

    // Get the result for the disc format.
    if (!connection->ReadFromChannel(target, 15, &response, &status)) {
      std::cout << "ReadFromChannel: " << status.message << std::endl;
      return 1;
    }
    std::cout << "Formatting status: " << response << std::endl;
  }

  if (!WriteMemory(connection.get(), target, kWriteBlockCodeStart,
                   sizeof(write_block_bin), write_block_bin, &status)) {
    std::cout << "WriteMemory: " << status.message << std::endl;
    return 1;
  }

  std::cout << "Opening source '" << source << "'." << std::endl;
  int fd = open(source.c_str(), O_RDONLY);
  if (fd == -1) {
    std::cout << "Error opening file: " << strerror(errno) << std::endl;
    return 1;
  }

  std::cout << "Opened file." << std::endl;

  unsigned int da_chan = 2;
  // Open a direct access channel associated with buffer 1 (0x400-0x4ff).
  if (!connection->OpenChannel(target, da_chan, "#1", &status)) {
    std::cout << "OpenChannel: " << status.message << std::endl;
  }

  std::cout << "Opened direct access channel." << std::endl;

  // Perform a dummy read operation to initialize the disc id.
  auto cmd = boost::format("U1:%u 0 %u %u") % da_chan % 1 % 0;
  std::cout << "cmd=" << cmd << std::endl;
  if (!connection->WriteToChannel(target, 15, cmd.str(), &status)) {
    std::cout << "WriteToChannel: " << status.message << std::endl;
    return 1;
  }
  // Get the result for the initial read command.
  if (!connection->ReadFromChannel(target, 15, &response, &status)) {
    std::cout << "ReadFromChannel: " << status.message << std::endl;
    return 1;
  }
  if (response != "00, OK,00,00\r") {
    std::cout << "Initial read status: " << response << std::endl;
  }

  cmd = boost::format("B-P:%u 0") % da_chan;
  std::cout << "cmd=" << cmd << std::endl;
  if (!connection->WriteToChannel(target, 15, cmd.str(), &status)) {
    std::cout << "WriteToChannel: " << status.message << std::endl;
    return 1;
  }

  std::cout << "Reset buffer offset." << std::endl;

  BufferedReadWriter reader(fd);

  // Copy the entire disc.
  for (unsigned int s = 0;; ++s) {
    std::string current_sector;
    if (!reader.ReadUpTo(256, 256, &current_sector, &status)) {
      if (status.status_code == IECStatus::END_OF_FILE) {
        // We're done reading this image.
        break;
      }
      std::cout << "Failed reading from file: " << status.message << std::endl;
      return 1;
    }

    // Write the current sector to the buffer.
    if (!connection->WriteToChannel(target, da_chan, current_sector, &status)) {
      std::cout << "WriteToChannel: " << status.message << std::endl;
      return 1;
    }

    unsigned int track = 1;
    unsigned int sector = 0;
    GetTrackSector(s, &track, &sector);

    // Write the buffer to disc.
    std::string request = "M-E";
    request.append(1, char(kWriteBlockEntryPoint & 0xff));
    request.append(1, char(kWriteBlockEntryPoint >> 8));
    request.append(1, char(track));
    request.append(1, char(sector));
    std::cout << "Writing track " << track << " sector " << sector << std::endl;
    if (!connection->WriteToChannel(target, 15, request, &status)) {
      std::cout << "WriteToChannel: " << status.message << std::endl;
      return 1;
    }

    // Get the result for the write command.
    if (!connection->ReadFromChannel(target, 15, &response, &status)) {
      std::cout << "ReadFromChannel: " << status.message << std::endl;
      return 1;
    }
    if (response != "00, OK,00,00\r") {
      std::cout << "Status: " << response << std::endl;
    }

    if (verify) {
      // Verify buffer content.
      auto cmd = boost::format("U1:%u 0 %u %u") % da_chan % track % sector;
      std::cout << "cmd=" << cmd << std::endl;
      if (!connection->WriteToChannel(target, 15, cmd.str(), &status)) {
        std::cout << "WriteToChannel: " << status.message << std::endl;
        return 1;
      }
      // Read the current sector from the buffer.
      std::string verify_content;
      if (!connection->ReadFromChannel(target, da_chan, &verify_content,
                                       &status)) {
        std::cout << "ReadFromChannel: " << status.message << std::endl;
        return 1;
      }

      if (current_sector != verify_content) {
        std::cout << "Verification failed:" << std::endl;
        std::cout << "Original sector (" << current_sector.size()
                  << " bytes):" << std::endl;
        std::cout << BytesToHex(current_sector) << std::endl;
        std::cout << "Read sector (" << verify_content.size()
                  << " bytes):" << std::endl;
        std::cout << BytesToHex(verify_content) << std::endl;
      }

      // Position the buffer pointer in preparation for the next write
      // (reading from the same buffer produces an off-by-one error).
      cmd = boost::format("B-P:%u 0") % da_chan;
      std::cout << "cmd=" << cmd << std::endl;
      if (!connection->WriteToChannel(target, 15, cmd.str(), &status)) {
        std::cout << "WriteToChannel: " << status.message << std::endl;
        return 1;
      }
    }
  }

  close(fd);

  if (!connection->CloseChannel(target, da_chan, &status)) {
    std::cout << "CloseChannel: " << status.message << std::endl;
    return 1;
  }

  // Get the final result.
  if (!connection->ReadFromChannel(target, 15, &response, &status)) {
    std::cout << "ReadFromChannel: " << status.message << std::endl;
    return 1;
  }
  std::cout << "Copying status: " << response << std::endl;
  return 0;
}
