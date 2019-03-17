// DriveInterface implementation on d64 drive images.

#include "image_drive_d64.h"

#include <errno.h>
#include <iostream>
#include <string.h>

ImageDriveD64::ImageDriveD64(const std::string &image_path, bool read_only)
    : image_path_(image_path), read_only_(read_only) {}

ImageDriveD64::~ImageDriveD64() {
  if (image_fd_ != -1) {
    // If we have a valid file descriptor, try to close it.
    // Ignore failures, we can't do anything about them here.
    if (close(image_fd_) != 0) {
      std::cerr << "ImageDriveD64: close() failed: " << strerror(errno)
                << std::endl;
    }
    image_fd_ = -1;
  }
}

bool ImageDriveD64::FormatDiscLowLevel(size_t num_tracks, IECStatus *status) {
  SetError(IECStatus::UNIMPLEMENTED, "ImageDriveD64::FormatDiscLowLevel",
           status);
  return false;
}

size_t ImageDriveD64::GetNumSectors() {
  // TODO(aeckleder): For existing images, calculate based on number of sectors.
  return 0;
}

bool ImageDriveD64::ReadSector(size_t sector_number, std::string *content,
                               IECStatus *status) {
  SetError(IECStatus::UNIMPLEMENTED, "ImageDriveD64::ReadSector", status);
  return false;
}

bool ImageDriveD64::WriteSector(size_t sector_number,
                                const std::string &content, IECStatus *status) {
  SetError(IECStatus::UNIMPLEMENTED, "ImageDriveD64::WriteSector", status);
  return false;
}

bool ImageDriveD64::OpenDiscImage(IECStatus *status) {
  SetError(IECStatus::UNIMPLEMENTED, "ImageDriveD64::OpenDiscImage", status);
  return false;
}
