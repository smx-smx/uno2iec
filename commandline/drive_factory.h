// Defines a factory function to create DriveInterface instances.

#ifndef DRIVE_FACTORY_H
#define DRIVE_FACTORY_H

#include <memory>

#include "drive_interface.h"
#include "iec_host_lib.h"

// Factory for creating a drive instance from the specified file_or_id.
// file_or_id can be either a IEC bus id or a path to a disc image.
// If file_or_id specifies a IEC bus id, bus_conn must be a pointer to
// and IECBusConnection instance used to talk to the drive.
// if read_only is true, expect the drive to reject attempts to write to it.
// If successful, returns a drive instance, nullptr otherwise.
// The string pointed to by drive_status receives the current drive status
// (may be empty if none). In case of failure, status will receive the
// corresponding error message.
std::unique_ptr<DriveInterface> CreateDriveObject(const std::string &file_or_id,
                                                  IECBusConnection *bus_conn,
                                                  bool read_only,
                                                  IECStatus *status);

#endif // DRIVE_FACTORY_H
