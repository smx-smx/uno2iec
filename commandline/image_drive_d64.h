// DriveInterface implementation on d64 drive images.

#ifndef IMAGE_DRIVE_D64_H
#define IMAGE_DRIVE_D64_H

#include <string>

#include "drive_interface.h"

class ImageDriveD64 : public DriveInterface {
public:
  // Instantiate a image drive object based on image_path. If read_only
  // is true, the image file is expected to exist and will be opened
  // in readonly mode. Attempts to write to the image will fail.
  ImageDriveD64(const std::string &image_path, bool read_only);

  ~ImageDriveD64();

  bool FormatDiscLowLevel(size_t num_tracks, IECStatus *status) override;
  bool GetNumSectors(size_t *num_sectors, IECStatus *status) override;
  bool ReadSector(size_t sector_number, std::string *content,
                  IECStatus *status) override;
  bool WriteSector(size_t sector_number, const std::string &content,
                   IECStatus *status) override;

private:
  // Open the disc image if it isn't already open. In case of an error,
  // returns false and sets status.
  bool OpenDiscImage(IECStatus *status);

  // Seek to the specified sector, which is expected to exist.
  bool SeekToSector(size_t sector_number, IECStatus *status);

  // Path to the disc image we're operating on.
  std::string image_path_;

  // True if image should be opened read-only.
  bool read_only_;

  // If the image is opened, contains the file descriptor used to
  // access it.
  int image_fd_ = -1;
};

#endif // IMAGE_DRIVE_D64_H
