// This file defines a C64 disc drive interface. Apart from the "real deal"
// implementation, it can also be implemented on top of disc images such as
// the .d64 format. Note that while the physical sectors of a C64 disc are
// identified by track / sector numbers, this interface abstracts from that
// addressing scheme by assigning a linear sector number to all sectors,
// which are internally translated to a (track, sector) pair if necessary.

#ifndef DRIVE_INTERFACE_H
#define DRIVE_INTERFACE_H

#include "utils.h"

class DriveInterface {
public:
  virtual ~DriveInterface() {}

  // Physically formats the disc. Note that depending on the implementation,
  // this method won't put logical structure such as a valid BAM or directory
  // onto the disc. The num_sectors value determines how many sectors should
  // be formatted. C64 floppy discs may contain up to 41 tracks, but the
  // standard is 35 tracks. Returns true if successful, sets status otherwise.
  virtual bool FormatDiscLowLevel(size_t num_sectors, IECStatus *status) = 0;

  // Determine and return the number of sectors available on the current disc.
  virtual size_t GetNumSectors() = 0;

  // Read the sector specified by sector_number into *content. Returns true if
  // successful, sets status otherwise.
  virtual bool ReadSector(size_t sector_number, std::string *content,
                          IECStatus *status) = 0;

  // Write content to the sector specified by sector_number. Returns true if
  // successful, sets status otherwise.
  virtual bool WriteSector(size_t sector_number, const std::string &content,
                           IECStatus *status) = 0;
};

#endif // DRIVE_INTERFACE_H
