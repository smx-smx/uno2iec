#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

#include "iec_host_lib.h"

static void SetError(IECStatus::IECStatusCode status_code,
                     const std::string& context, IECStatus* status) {
  status->status_code = status_code;
  switch (status_code) {
    case IECStatus::OK:
      status->message = "OK";
      break;
    case IECStatus::UNIMPLEMENTED:
      status->message = "Unimplemented";
      break;
    case IECStatus::CONNECTION_FAILURE:
      status->message = "Connection failure";
      break;
  }
  if (!context.empty()) {
    status->message = context + ": " + status->message;
  }
}

static void SetErrorFromErrno(IECStatus::IECStatusCode status_code, const std::string& context,
                              IECStatus* status) {
  SetError(status_code, context, status);
  status->message = status->message + ": " + strerror(errno);
}

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

IECBusConnection* IECBusConnection::Create(int arduino_fd, IECStatus* status) {
  if (arduino_fd == -1) {
    return nullptr;
  }
  return new IECBusConnection(arduino_fd);
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
