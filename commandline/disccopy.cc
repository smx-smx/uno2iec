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
#include "cbm1541_drive.h"
#include "iec_host_lib.h"
#include "utils.h"

namespace po = boost::program_options;

using namespace std::chrono_literals;

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
  std::unique_ptr<CBM1541Drive> drive =
      std::make_unique<CBM1541Drive>(connection.get(), target);

  if (format) {
    std::cout << "Formatting disc..." << std::endl;
    if (!drive->FormatDiscLowLevel(40, &status)) {
      std::cout << "FormatDiscLowLevel: " << status.message << std::endl;
      return 1;
    }
    std::cout << "Formatting complete." << std::endl;
  }

  std::cout << "Opening source '" << source << "'." << std::endl;
  int fd = open(source.c_str(), O_RDONLY);
  if (fd == -1) {
    std::cout << "Error opening file: " << strerror(errno) << std::endl;
    return 1;
  }

  std::cout << "Opened file." << std::endl;

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

    if (!drive->WriteSector(s, current_sector, &status)) {
      std::cout << "WriteSector: " << status.message << std::endl;
      return 1;
    }

    if (verify) {
      // TODO(aeckleder): Remove this and read using the drive interface.
      const int da_chan = 2;
      unsigned int track = 1;
      unsigned int sector = 0;
      CBM1541Drive::GetTrackSector(s, &track, &sector);

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

  // Get the final result.
  if (!connection->ReadFromChannel(target, 15, &response, &status)) {
    std::cout << "ReadFromChannel: " << status.message << std::endl;
    return 1;
  }
  std::cout << "Copying status: " << response << std::endl;
  return 0;
}
