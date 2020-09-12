// DriveInterface implementation on d64 drive images.

#include "image_drive_d64.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <iostream>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "boost/format.hpp"

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

bool ImageDriveD64::GetNumSectors(size_t *num_sectors, IECStatus *status) {
  if (!OpenDiscImage(status))
    return false;
  assert(image_fd_ != -1);

  struct stat stat_buf;
  if (fstat(image_fd_, &stat_buf) != 0) {
    SetErrorFromErrno(IECStatus::DRIVE_ERROR, "GetNumSectors", status);
    return false;
  }

  if (stat_buf.st_size % kNumBytesPerSector > 0) {
    SetError(IECStatus::DRIVE_ERROR,
             "GetNumSectors: File size not a multiple of sector size.", status);
    return false;
  }

  *num_sectors = stat_buf.st_size / kNumBytesPerSector;
  return true;
}

bool ImageDriveD64::ReadSector(size_t sector_number, std::string *content,
                               IECStatus *status) {
  if (!OpenDiscImage(status))
    return false;
  assert(image_fd_ != -1);
  if (!SeekToSector(sector_number, status))
    return false;

  content->resize(kNumBytesPerSector);
  ssize_t res = read(image_fd_, &(*content)[0], kNumBytesPerSector);
  if (res != kNumBytesPerSector) {
    // We're reading from a regular file. If it is a properly formatted
    // disc image, we should always get the expected number of bytes back.
    SetErrorFromErrno(
        IECStatus::DRIVE_ERROR,
        (boost::format("ReadSector: read returned %u") % res).str(), status);
    return false;
  }
  return true;
}

bool ImageDriveD64::WriteSector(size_t sector_number,
                                const std::string &content, IECStatus *status) {
  SetError(IECStatus::UNIMPLEMENTED, "ImageDriveD64::WriteSector", status);
  return false;
}

bool ImageDriveD64::ReadCommandChannel(std::string *response,
                                       IECStatus *status) {
  bool result = OpenDiscImage(status);
  if (result) {
    *response = "Accessing image '" + image_path_ + "'";
  }
  return result;
}

bool ImageDriveD64::SeekToSector(size_t sector_number, IECStatus *status) {
  off_t seek_pos = sector_number * kNumBytesPerSector;
  if (lseek(image_fd_, seek_pos, SEEK_SET) != seek_pos) {
    SetErrorFromErrno(IECStatus::DRIVE_ERROR, "SeekToSector", status);
    return false;
  }
  return true;
}

bool ImageDriveD64::OpenDiscImage(IECStatus *status) {
  if (image_fd_ != -1) {
    return true;
  }

  image_fd_ = open(image_path_.c_str(),
                   read_only_ ? O_RDONLY : O_RDWR | O_CREAT, S_IRWXU | S_IRWXG);
  if (image_fd_ == -1) {
    SetErrorFromErrno(IECStatus::DRIVE_ERROR, "OpenDiscImage", status);
    return false;
  }
  return true;
}
