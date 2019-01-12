#include <chrono>
#include <iostream>
#include <thread>

#include "boost/program_options/cmdline.hpp"
#include "boost/program_options/options_description.hpp"
#include "boost/program_options/parsers.hpp"
#include "boost/program_options/variables_map.hpp"
#include "iec_host_lib.h"

namespace po = boost::program_options;

using namespace std::chrono_literals;

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
      "speed", po::value<int>(&serial_speed)->default_value(115200),
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

  std::this_thread::sleep_for(4s);

  // Read from the command channel. This is always OK, the open
  // call is completely optional.
  std::string response;
  if (!connection->ReadFromChannel(9, 15, &response, &status)) {
    std::cout << "ReadFromChannel: " << status.message << std::endl;
    return 1;
  }
  std::cout << "ReadFromChannel: response=" << response << std::endl;

  // TODO(aeckleder): Commands do not currently block, so we have to
  // wait a bit until they're done.
  std::this_thread::sleep_for(1s);

  // Perform a full disk format, just to do something.
  if (!connection->OpenChannel(9, 15, "N:MYDISC" /*"N:MYDISC,ID"*/, &status)) {
    std::cout << "OpenChannel: " << status.message << std::endl;
    return 1;
  }

  // TODO(aeckleder): Commands do not currently block, so we have to
  // wait a bit until they're done.
  std::this_thread::sleep_for(1s);

  // Get the result for the disc format.
  if (!connection->ReadFromChannel(9, 15, &response, &status)) {
    std::cout << "ReadFromChannel: " << status.message << std::endl;
    return 1;
  }
  std::cout << "ReadFromChannel: response=" << response << std::endl;

  // TODO(aeckleder): Commands do not currently block, so we have to
  // wait a bit until they're done.
  std::this_thread::sleep_for(1s);

  if (!connection->CloseChannel(9, 15, &status)) {
    std::cout << "CloseChannel: " << status.message << std::endl;
    return 1;
  }

  std::this_thread::sleep_for(2s);

  return 0;
}
