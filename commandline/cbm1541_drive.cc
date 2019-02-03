// DriveInterface implementation on top of a physical CBM 1541 disk drive.

#include "cbm1541_drive.h"

#include "assembly/format_h.h"
#include "assembly/write_block_h.h"

const std::map<CBM1541Drive::FirmwareState,
               CBM1541Drive::CustomFirmwareFragment>
    CBM1541Drive::fw_fragment_map_ = {
        {FW_CUSTOM_FORMATTING_CODE, {format_bin, sizeof(format_bin), 0x500}},
        {FW_CUSTOM_READ_WRITE_CODE,
         {write_block_bin, sizeof(write_block_bin), 0x500}}};

CBM1541Drive::CBM1541Drive(IECBusConnection *bus_conn, char device_number)
    : bus_conn_(bus_conn), device_number_(device_number),
      fw_state_(FW_NO_CUSTOM_CODE) {}

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
