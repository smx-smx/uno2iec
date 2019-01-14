#include <chrono>
#include <iostream>
#include <thread>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "boost/format.hpp"
#include "boost/program_options/cmdline.hpp"
#include "boost/program_options/options_description.hpp"
#include "boost/program_options/parsers.hpp"
#include "boost/program_options/variables_map.hpp"
#include "iec_host_lib.h"
#include "utils.h"

namespace po = boost::program_options;

using namespace std::chrono_literals;

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

int main(int argc, char *argv[]) {
  std::cout << "IEC Bus disc copy utility." << std::endl
            << "Copyright (c) 2018 Andreas Eckleder" << std::endl
            << std::endl;

  std::string arduino_device;
  int serial_speed = 0;

  po::options_description desc("Options");
  desc.add_options()("help", "usage overview")(
      "serial",
      po::value<std::string>(&arduino_device)->default_value("/dev/ttyUSB0"),
      "serial interface to use")(
      "speed", po::value<int>(&serial_speed)->default_value(57600),
      "baud rate");

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
  if (!connection->ReadFromChannel(9, 15, &response, &status)) {
    std::cout << "ReadFromChannel: " << status.message << std::endl;
    return 1;
  }
  std::cout << "Initial drive status: " << response << std::endl;
  std::cout << "Formatting disc..." << std::endl;

  if (!connection->WriteToChannel(9, 15, "N:MYDISC" /*"N:MYDISC,ID"*/,
                                  &status)) {
    std::cout << "WriteToChannel: " << status.message << std::endl;
    return 1;
  }

  // Get the result for the disc format.
  if (!connection->ReadFromChannel(9, 15, &response, &status)) {
    std::cout << "ReadFromChannel: " << status.message << std::endl;
    return 1;
  }
  std::cout << "Formatting status: " << response << std::endl;

  std::cout << "Opening source..." << std::endl;
  int fd = open("/home/weirdsoul/coding/uno2iec/commandline/2ndReality_S1.D64",
                O_RDONLY);
  if (fd == -1) {
    std::cout << "Error opening file: " << strerror(errno) << std::endl;
    return 1;
  }

  unsigned int da_chan = 2;
  // Open a direct access channel.
  if (!connection->OpenChannel(9, da_chan, "#", &status)) {
    std::cout << "OpenChannel: " << status.message << std::endl;
  }

  BufferedReadWriter reader(fd);

  // Copy the entire disc (683 sectors).
  for (unsigned int s = 0; s < 683; ++s) {
    std::string current_sector;
    if (!reader.ReadUpTo(256, 256, &current_sector, &status)) {
      std::cout << "Failed reading from file: " << status.message << std::endl;
      return 1;
    }

    // Write the current sector to the buffer.
    if (!connection->WriteToChannel(9, da_chan, current_sector, &status)) {
      std::cout << "WriteToChannel: " << status.message << std::endl;
      return 1;
    }

    unsigned int track = 1;
    unsigned int sector = 0;
    GetTrackSector(s, &track, &sector);

    // Write the buffer to disc.
    std::string cmd =
        (boost::format("U2 %u 0 %u %u") % da_chan % track % sector).str();
    std::cout << "cmd=" << cmd << std::endl;
    if (!connection->WriteToChannel(9, 15, cmd, &status)) {
      std::cout << "WriteToChannel: " << status.message << std::endl;
      return 1;
    }
  }

  close(fd);

  if (!connection->CloseChannel(9, da_chan, &status)) {
    std::cout << "CloseChannel: " << status.message << std::endl;
    return 1;
  }

  // Get the final result.
  if (!connection->ReadFromChannel(9, 15, &response, &status)) {
    std::cout << "ReadFromChannel: " << status.message << std::endl;
    return 1;
  }
  std::cout << "Copying status: " << response << std::endl;

  return 0;
}
