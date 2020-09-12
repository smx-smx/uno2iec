#include <chrono>
#include <iostream>
#include <thread>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "assembly/format_h.h"
#include "boost/format.hpp"
#include "boost/program_options/cmdline.hpp"
#include "boost/program_options/options_description.hpp"
#include "boost/program_options/parsers.hpp"
#include "boost/program_options/variables_map.hpp"
#include "drive_factory.h"
#include "drive_interface.h"
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
            << "Copyright (c) 2020 Andreas Eckleder" << std::endl
            << std::endl;

  std::string arduino_device;
  int serial_speed = 0;
  bool verify = false;
  std::string source;
  std::string target;
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
      "device (e.g. 8, 9) or image to copy from")(
      "target", po::value<std::string>(&target)->default_value(""),
      "device (e.g. 8, 9) or image file to copy to")(
      "format", po::value<bool>(&format)->default_value(false),
      "format disc prior to copying");

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  if (vm.count("help")) {
    std::cout << desc << std::endl;
    return 1;
  }

  if (source.empty()) {
    std::cout << desc << std::endl
              << "Required argument --source must be non-empty." << std::endl;
    return 2;
  }
  if (target.empty()) {
    std::cout << desc << std::endl
              << "Required argument --target must be non-empty." << std::endl;
    return 2;
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

  std::unique_ptr<DriveInterface> source_drive =
      CreateDriveObject(source, connection.get(), /*read_only=*/true, &status);
  if (!source_drive) {
    std::cout << "Failed to access specified source: " << status.message
              << std::endl;
    return 1;
  } else {
    std::string drive_status;
    if (!source_drive->ReadCommandChannel(&drive_status, &status)) {
      std::cout << "Failed to read source status:" << status.message
                << std::endl;
      return 1;
    }
    std::cout << "Initial source status: " << drive_status << std::endl;
  }

  std::unique_ptr<DriveInterface> target_drive =
      CreateDriveObject(target, connection.get(), /*read_only=*/false, &status);
  if (!target_drive) {
    std::cout << "Failed to access specified target: " << status.message
              << std::endl;
    return 1;
  } else {
    std::string drive_status;
    if (!target_drive->ReadCommandChannel(&drive_status, &status)) {
      std::cout << "Failed to read target status:" << status.message
                << std::endl;
      return 1;
    }
    std::cout << "Initial target status: " << drive_status << std::endl;
  }

  if (format) {
    std::cout << "Formatting disc..." << std::endl;
    if (!target_drive->FormatDiscLowLevel(40, &status)) {
      std::cout << "FormatDiscLowLevel: " << status.message << std::endl;
      return 1;
    }
    std::cout << "Formatting complete." << std::endl;
  }

  // Copy the entire disc.
  size_t num_sectors = 0;
  if (!source_drive->GetNumSectors(&num_sectors, &status)) {
    std::cout << "Failed to retrieve number of sectors: " << status.message
              << std::endl;
    return 1;
  }
  for (unsigned int s = 0; s < num_sectors; ++s) {
    std::string current_sector;
    if (!source_drive->ReadSector(s, &current_sector, &status)) {
      std::cout << "ReadSector: " << status.message << std::endl;
      return 1;
    }

    if (!target_drive->WriteSector(s, current_sector, &status)) {
      std::cout << "WriteSector: " << status.message << std::endl;
      return 1;
    }

    if (verify) {
      std::string verify_content;
      if (!target_drive->ReadSector(s, &verify_content, &status)) {
        std::cout << "ReadSector: " << status.message << std::endl;
        return 1;
      }

      if (current_sector != verify_content) {
        std::cout << "Verification failed (sector " << s << "):" << std::endl;
        std::cout << "Original sector (" << current_sector.size()
                  << " bytes):" << std::endl;
        std::cout << BytesToHex(current_sector) << std::endl;
        std::cout << "Read sector (" << verify_content.size()
                  << " bytes):" << std::endl;
        std::cout << BytesToHex(verify_content) << std::endl;
      }
    }
  }

  // Get the final result.
  std::string drive_status;
  if (!target_drive->ReadCommandChannel(&drive_status, &status)) {
    std::cout << "Failed to read target status:" << status.message << std::endl;
    return 1;
  }
  std::cout << "Copying status: " << drive_status << std::endl;
  return 0;
}
