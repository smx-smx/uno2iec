// DriveInterface implementation on top of a physical CBM 1541 disk drive.

#ifndef CBM1541_DRIVE_H
#define CBM1541_DRIVE_H

#include "drive_interface.h"

class CBM1541Drive : public DriveInterface {
public:
  bool FormatDiscLowLevel(size_t num_sectors, IECStatus *status) override;
  size_t GetNumSectors() override;
  bool ReadSector(size_t sector_number, std::string *content,
                  IECStatus *status) override;
  bool WriteSector(size_t sector_number, std::string *content,
                   IECStatus *status) override;
};

#endif // CBM1541_DRIVE_H
