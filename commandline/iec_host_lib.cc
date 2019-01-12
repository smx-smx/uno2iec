#include <fcntl.h>
#include <iostream>
#include <memory>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "boost/format.hpp"
#include "iec_host_lib.h"

// Maximum chars to read looking for '\r'. This is intentionally larger than
// kConnectionStringPrefix, because it is followed by a version number.
static const size_t kMaxLength = 256;

static const std::string kConnectionStringPrefix = "connect_arduino:";

// Needs to support host mode.
static const int kMinProtocolVersion = 3;

// Number of tries for successfully reading the connection string prefix.
static const int kNumRetries = 5;

// Config values. These are hardcoded for now and match the defaults of
// the Arduino implementation. We request to be the host, so we specify
// a device number of zero here (which is special cased on the Arduino).
// Device zero (the C64 keyboard) is normally not addressed through the
// IEC bus, so this special casing should be OK.
static const int kDeviceNumber = 0;

static const int kAtnPin = 5;
static const int kDataPin = 3;
static const int kClockPin = 4;
static const int kSrqInPin = 6;
static const int kResetPin = 7;

// Commands supported by the Arduino's serial interface. All of these are
// single character strings.
static const std::string kCmdReset = "r"; // Reset the IEC bus.
static const std::string kCmdOpen = "o";  // Open a channel on a device.
static const std::string kCmdClose = "c"; // Close a channel on a device.
static const std::string kCmdGetData =
    "g"; // Get data from a channel on a device.

IECBusConnection::IECBusConnection(int arduino_fd, LogCallback log_callback)
    : arduino_fd_(arduino_fd),
      arduino_writer_(std::make_unique<BufferedReadWriter>(arduino_fd)),
      log_callback_(log_callback) {}

IECBusConnection::~IECBusConnection() {
  if (response_thread_.joinable()) {
    // Step response processing.
    response_thread_.join();
  }

  if (arduino_fd_ != -1) {
    close(arduino_fd_);
    arduino_fd_ = -1;
  }
}

bool IECBusConnection::Reset(IECStatus *status) {
  if (!arduino_writer_->WriteString(kCmdReset, status)) {
    return false;
  }
  return true;
}

bool IECBusConnection::OpenChannel(char device_number, char channel,
                                   const std::string &cmd_string,
                                   IECStatus *status) {
  std::string request_string = kCmdOpen + device_number + channel +
                               static_cast<char>(cmd_string.size()) +
                               cmd_string;
  if (!arduino_writer_->WriteString(request_string, status)) {
    return false;
  }
  return true;
}

bool IECBusConnection::ReadFromChannel(char device_number, char channel,
                                       std::string *result, IECStatus *status) {
  std::string request_string = kCmdGetData + device_number + channel;
  if (!arduino_writer_->WriteString(request_string, status)) {
    return false;
  }
  // TODO(aeckleder): Actually return the data rather than printing it
  // to the log.
  return true;
}

bool IECBusConnection::CloseChannel(char device_number, char channel,
                                    IECStatus *status) {
  std::string request_string = kCmdClose + device_number + channel;
  if (!arduino_writer_->WriteString(request_string, status)) {
    return false;
  }
  return true;
}

bool IECBusConnection::Initialize(IECStatus *status) {
  std::string connection_string;
  for (int i = 0; i < kNumRetries; ++i) {
    if (!arduino_writer_->ReadTerminatedString('\r', kMaxLength,
                                               &connection_string, status)) {
      return false;
    }
    if (connection_string.size() >= kConnectionStringPrefix.size() &&
        connection_string.substr(0, kConnectionStringPrefix.size()) ==
            kConnectionStringPrefix) {
      break;
    } else if (i >= (kNumRetries - 1)) {
      SetError(IECStatus::CONNECTION_FAILURE,
               std::string("Unknown protocol response: '") + connection_string +
                   "'",
               status);
      return false;
    } else {
      log_callback_('W', "CLIENT",
                    (boost::format("Malformed connection string '%s'") %
                     connection_string)
                        .str());
    }
  }
  int protocol_version = 0;
  if (sscanf(connection_string.substr(kConnectionStringPrefix.size()).c_str(),
             "%i", &protocol_version) <= 0 ||
      protocol_version < kMinProtocolVersion) {
    SetError(IECStatus::CONNECTION_FAILURE,
             std::string("Unsupported protocol: '") + connection_string + "'",
             status);
    return false;
  }
  time_t unix_time = time(nullptr);
  struct tm local_time;
  localtime_r(&unix_time, &local_time);

  // Now talk back to the Arduino, communicating our configuration.
  auto config_string =
      boost::format("OK>%u|%u|%u|%u|%u|%u|%u-%u-%u.%u:%u:%u\r") %
      kDeviceNumber % kAtnPin % kClockPin % kDataPin % kResetPin % kSrqInPin %
      (local_time.tm_year + 1900) % (local_time.tm_mon + 1) %
      local_time.tm_mday % local_time.tm_hour % local_time.tm_min %
      local_time.tm_sec;
  if (!arduino_writer_->WriteString(config_string.str(), status)) {
    return false;
  }

  // Start our response thread.
  response_thread_ = std::thread(&IECBusConnection::ProcessResponses, this);

  return true;
}

void IECBusConnection::ProcessResponses() {
  // TODO(aeckleder): Infinithread. Don't block on first character reads and
  // introduce quit conditional to shut down the background thread.
  while (true) {
    std::string read_string;
    IECStatus status;

    if (!arduino_writer_->ReadUpTo(1, 1, &read_string, &status)) {
      log_callback_('E', "CLIENT", status.message);
      return;
    }
    switch (read_string[0]) {
    case '!':
      // Debug channel configuration.
      if (!arduino_writer_->ReadTerminatedString('\r', kMaxLength, &read_string,
                                                 &status)) {
        log_callback_('E', "CLIENT", status.message);
        return;
      }
      if (read_string.size() < 2) {
        log_callback_(
            'E', "CLIENT",
            (boost::format("Malformed channel configuration string '%s'") %
             read_string)
                .str());
      }
      debug_channel_map_[read_string[0]] = read_string.substr(1);
      break;
    case 'D':
      // Standard debug message.
      if (!arduino_writer_->ReadTerminatedString('\r', kMaxLength, &read_string,
                                                 &status)) {
        log_callback_('E', "CLIENT", status.message);
        return;
      }
      if (read_string.size() < 3 ||
          debug_channel_map_.count(read_string[1]) == 0) {
        // Print the malformed message, but don't terminate execution.
        log_callback_(
            'E', "CLIENT",
            (boost::format("Malformed debug message '%s'") % read_string)
                .str());
      }
      log_callback_(read_string[0], debug_channel_map_[read_string[1]],
                    read_string.substr(2));
      break;
    case 'r':
      // Standard data response message.
      if (!arduino_writer_->ReadTerminatedString('\r', kMaxLength, &read_string,
                                                 &status)) {
        log_callback_('E', "CLIENT", status.message);
        return;
      }
      log_callback_('I', "RESPONSE", std::string("") +
                                         std::to_string(read_string.size()) +
                                         ": \"" + read_string + "\"");
      break;
    default:
      // Ignore all other messages.
      break;
    }
  }
}

IECBusConnection *IECBusConnection::Create(int arduino_fd,
                                           LogCallback log_callback,
                                           IECStatus *status) {
  if (arduino_fd == -1) {
    return nullptr;
  }
  auto conn = std::make_unique<IECBusConnection>(arduino_fd, log_callback);
  if (!conn->Initialize(status)) {
    return nullptr;
  }
  return conn.release();
}

IECBusConnection *IECBusConnection::Create(const std::string &device_file,
                                           int speed, LogCallback log_callback,
                                           IECStatus *status) {
  int fd = open(device_file.c_str(), O_RDWR | O_NONBLOCK);
  if (fd == -1) {
    SetErrorFromErrno(IECStatus::CONNECTION_FAILURE,
                      "open(\"" + device_file + "\")", status);
    return nullptr;
  }
  struct termios tty;
  if (tcgetattr(fd, &tty) == -1) {
    SetErrorFromErrno(IECStatus::CONNECTION_FAILURE, "tcgetattr", status);
    return nullptr;
  }
  cfsetospeed(&tty, (speed_t)speed);
  cfsetispeed(&tty, (speed_t)speed);

  tty.c_cflag |= (CLOCAL | CREAD); /* ignore modem controls */
  tty.c_cflag &= ~CSIZE;
  tty.c_cflag |= CS8;      /* 8-bit characters */
  tty.c_cflag &= ~PARENB;  /* no parity bit */
  tty.c_cflag &= ~CSTOPB;  /* only need 1 stop bit */
  tty.c_cflag &= ~CRTSCTS; /* no hardware flowcontrol */

  /* setup for non-canonical mode */
  tty.c_iflag &=
      ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
  tty.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
  tty.c_oflag &= ~OPOST;

  /* fetch bytes as they become available */
  tty.c_cc[VMIN] = 1;
  tty.c_cc[VTIME] = 1;

  if (tcsetattr(fd, TCSANOW, &tty) == -1) {
    SetErrorFromErrno(IECStatus::CONNECTION_FAILURE, "tcsetattr", status);
    return nullptr;
  }
  return Create(fd, log_callback, status);
}
