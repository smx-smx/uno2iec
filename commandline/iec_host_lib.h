// The IEC host library provides functionality to talk to
// devices on an IEC bus connected via Arduino.
// The host library assumes the function of a host, hence the name.

#ifndef IEC_HOST_LIB_H
#define IEC_HOST_LIB_H

#include <string>

struct IECStatus {
  std::string message;  // A status message describing the status.
};

// Send a command via the command channel. Returns true on success. In case of
// an error, status will be set to an appropriate error status.
bool SendCommand(const std::string& cmd_string, IECStatus* status);

#endif  // IEC_HOST_LIB_H
