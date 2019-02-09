// DriveInterface implementation on top of a physical CBM 1541 disk drive.

#include "cbm1541_drive.h"

#include "assembly/format_h.h"
#include "assembly/write_block_h.h"

// Logical OK response.
static const char kOKResponse[] = "00, OK,00,00\r";

// Max amount of data for a single M-W command is 35 bytes.
static const size_t kMaxMWSize = 35;

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

bool CBM1541Drive::SetFirmwareState(CBM1541Drive::FirmwareState firmware_state,
                                    IECStatus *status) {
  // Exit early if we're already in the desired state.
  if (fw_state_ == firmware_state)
    return true;
  auto fw_it = fw_fragment_map_.find(firmware_state);
  // No specific firmware requirements for this state. We're done.
  if (fw_it == fw_fragment_map_.end())
    return true;
  return WriteMemory(fw_it->second.loading_address, fw_it->second.binary_size,
                     fw_it->second.binary, status);
}

bool CBM1541Drive::WriteMemory(unsigned short int target_address,
                               size_t num_bytes, const unsigned char *source,
                               IECStatus *status) {
  size_t bytes_written = 0;
  while (num_bytes - bytes_written > 0) {
    std::string request = "M-W";
    unsigned short int mem_pos = target_address + bytes_written;
    request.append(1, mem_pos & 0xff);
    request.append(1, mem_pos >> 8);
    size_t num_data_bytes = std::min(kMaxMWSize, num_bytes - bytes_written);
    request.append(1, num_data_bytes);
    for (size_t i = 0; i < num_data_bytes; ++i) {
      request.append(1, source[bytes_written + i]);
    }
    if (!bus_conn_->WriteToChannel(device_number_, 15, request, status)) {
      return false;
    }
    std::string response;
    if (!bus_conn_->ReadFromChannel(device_number_, 15, &response, status)) {
      return false;
    }
    if (response != kOKResponse) {
      SetError(IECStatus::DRIVE_ERROR, response, status);
      return false;
    }
    bytes_written += num_data_bytes;
  }
  return true;
}
