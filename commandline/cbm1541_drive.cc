// DriveInterface implementation on top of a physical CBM 1541 disk drive.

#include "cbm1541_drive.h"

bool CBM1541Drive::FormatDiscLowLevel(size_t num_sectors, IECStatus *status) {
  return false;
}

size_t CBM1541Drive::GetNumSectors() { return 0; }

bool CBM1541Drive::ReadSector(size_t sector_number, std::string *content,
                              IECStatus *status) {
  return false;
}

bool CBM1541Drive::WriteSector(size_t sector_number, std::string *content,
                               IECStatus *status) {
  return false;
}
