#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <iostream>
#include <memory>

#include "boost/format.hpp"
#include "iec_host_lib.h"

// Maximum chars to read looking for '\r'. This is intentionally larger than
// kConnectionStringPrefix, because it is followed by a version number.
static const size_t kMaxLength = 256;

static const std::string kConnectionStringPrefix = "connect_arduino:";

// TODO(aeckleder): We'll need to increase the version as soon as we start
// supporting custom features.
static const int kMinProtocolVersion = 2;

// Number of tries for successfully reading the connection string prefix.
static const int kNumRetries = 5;

// Config values. These are hardcoded for now and match the defaults of
// the Arduino implementation.
// TODO(aeckleder): We'll be playing the host, so we shouldn't specify a device
// number.
static const int kDeviceNumber = 8;

static const int kAtnPin = 5;
static const int kDataPin = 3;
static const int kClockPin = 4;
static const int kSrqInPin = 6;
static const int kResetPin = 7;

IECBusConnection::IECBusConnection(int arduino_fd) : arduino_fd_(arduino_fd) {}

IECBusConnection::~IECBusConnection() {
  if (arduino_fd_ != -1) {
    close(arduino_fd_);
    arduino_fd_ = -1;
  }
}

bool IECBusConnection::Reset(IECStatus* status) {
  SetError(IECStatus::UNIMPLEMENTED, "Reset", status);
  return false;
}

bool IECBusConnection::SendCommand(int device_number,
                                   const std::string& cmd_string,
                                   IECStatus* status) {
  SetError(IECStatus::UNIMPLEMENTED, "SendCommand", status);
  return false;
}

bool IECBusConnection::Initialize(IECStatus* status) {
  std::string connection_string;
  for (int i = 0; i < kNumRetries; ++i) {
    if (!ReadTerminatedString(arduino_fd_, '\r', kMaxLength, &connection_string,
                              status)) {
      return false;
    }
    if (connection_string.size() >= kConnectionStringPrefix.size() &&
        connection_string.substr(0, kConnectionStringPrefix.size()) ==
            kConnectionStringPrefix) {
      break;
    } else if (i >= (kNumRetries - 1)) {
      SetError(
          IECStatus::CONNECTION_FAILURE,
          std::string("Unknown protocol response: '") + connection_string + "'",
          status);
      return false;
    }
  }
  int protocol_version = 0;
  if (sscanf(connection_string.substr(kConnectionStringPrefix.size()).c_str(),
             "%i", &protocol_version) <= 0 ||
      protocol_version < kMinProtocolVersion) {
    SetError(
        IECStatus::CONNECTION_FAILURE,
        std::string("Unknown protocol response: '") + connection_string + "'",
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
  if (!WriteString(arduino_fd_, config_string.str(), status)) return false;

  // TODO(aeckleder): We won't have to do this in the future. But right now,
  // we're trying to understand the existing protocol.
  for (int i = 0; i < 6; ++i) {
    std::string log_string;
    if (!ReadTerminatedString(arduino_fd_, '\r', kMaxLength, &log_string,
                              status)) {
      return false;
    }
    std::cout << "LOG LINE: " << log_string << std::endl;
  }
  return true;
}

IECBusConnection* IECBusConnection::Create(int arduino_fd, IECStatus* status) {
  if (arduino_fd == -1) {
    return nullptr;
  }
  auto conn = std::make_unique<IECBusConnection>(arduino_fd);
  if (!conn->Initialize(status)) {
    return nullptr;
  }
  return conn.release();
}

IECBusConnection* IECBusConnection::Create(const std::string& device_file,
                                           int speed, IECStatus* status) {
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
  return Create(fd, status);
}
