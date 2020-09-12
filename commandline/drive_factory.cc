// DriveInterface factory implementation

#include "drive_factory.h"

#include <boost/lexical_cast.hpp>

#include "cbm1541_drive.h"
#include "image_drive_d64.h"

std::unique_ptr<DriveInterface> CreateDriveObject(const std::string &file_or_id,
                                                  IECBusConnection *bus_conn,
                                                  bool read_only,
                                                  IECStatus *status) {
  std::unique_ptr<DriveInterface> result;
  try {
    // Try to interpret file_or_id as a device number. If this fails,
    // we end up in the exception handler below.
    int device_number = boost::lexical_cast<int>(file_or_id);

    result = std::make_unique<CBM1541Drive>(bus_conn, device_number);
  } catch (const boost::bad_lexical_cast &) {
    result = std::make_unique<ImageDriveD64>(file_or_id, read_only);
  }
  return result;
}
