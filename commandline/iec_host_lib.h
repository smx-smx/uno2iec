// The IEC host library provides functionality to talk to
// devices on an IEC bus connected via Arduino.
// The host library assumes the function of a host, hence the name.

#ifndef IEC_HOST_LIB_H
#define IEC_HOST_LIB_H

#include <string>

#include "utils.h"

class IECBusConnection {
 public:
  // Instantiate an IECBusConnection object. The arduino_fd parameter is
  // used to specify a file descriptor that will be used for bidirectional
  // communication with an arduino connected to the IEC bus and speaking the
  // uno2iec protocol. Use Create() methods below instead of instantiating
  // directly!
  IECBusConnection(int arduino_fd);

  // Reset the IEC bus by pulling the reset line to low. Returns true on
  // success. In case of an error, status will be set to an appropriate error
  // status.
  bool Reset(IECStatus* status);

  // Send a command via the command channel. to the device specified by
  // device_number. Returns true on success. In case of an error, status will be
  // set to an appropriate error status.
  bool SendCommand(int device_number, const std::string& cmd_string,
                   IECStatus* status);

  // Create IECBusConnection instance using the specified device_file and serial
  // port speed. Returns nullptr in case of a problem and sets status.
  // Otherwise, ownership of the IECBusConnection instance is transferred to the
  // caller.
  static IECBusConnection* Create(const std::string& device_file, int speed,
                                  IECStatus* status);

  // Create IECBusConnection instance based on the specified arduino_fd, which
  // must me ready to use. Ownership of the file description is passed to the
  // IECBusConnection instance. Returns nullptr in case of a problem and sets
  // status. Otherwise, ownership of the IECBusConnection instance is
  // transferred to the caller.
  static IECBusConnection* Create(int arduino_fd, IECStatus* status);

  // Free resources such as any owned file descriptors.
  virtual ~IECBusConnection();

  // Initialize the bus connection. To be called immediately after construction.
  // Returns true if successful. In case of error, returns false and sets
  // status.
  bool Initialize(IECStatus* status);

 private:

  // File descriptor used for communication.
  int arduino_fd_;
};

#endif  // IEC_HOST_LIB_H
