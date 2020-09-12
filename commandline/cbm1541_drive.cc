// DriveInterface implementation on top of a physical CBM 1541 disk drive.

#include "cbm1541_drive.h"

#include "assembly/format_h.h"
#include "assembly/rw_block_h.h"
#include "boost/format.hpp"

// Logical OK response.
static const char kOKResponse[] = "00, OK,00,00\r";

// Max amount of data for a single M-W command is 35 bytes.
static const size_t kMaxMWSize = 35;

// We skip the first three bytes, because they're a jmp into the read/write job.
static const size_t kReadWriteBlockEntryPoint = 0x503;

// The third parameter to the read/write job. If zero, read. If non-zero, write.
static const size_t kReadBlockOption = 0x00;
static const size_t kWriteBlockOption = 0x01;

// We skip the first three bytes, because they're a jmp into the format job.
static const size_t kFormatEntryPoint = 0x503;

static const size_t kNumBytesPerSector = 0x100;

// The direct access channels to use.
static const int kWriteDirectAccessChannel = 2;
static const int kReadDirectAccessChannel = 3;

// We won't allow trying to access a track higher than this as it might damage
// the hardware.
static const int kMaxTrackNumber = 41;

const std::map<CBM1541Drive::FirmwareState,
               CBM1541Drive::CustomFirmwareFragment>
    CBM1541Drive::fw_fragment_map_ = {
        {FW_CUSTOM_FORMATTING_CODE, {format_bin, sizeof(format_bin), 0x500}},
        {FW_CUSTOM_READ_WRITE_CODE,
         {rw_block_bin, sizeof(rw_block_bin), 0x500}}};

CBM1541Drive::CBM1541Drive(IECBusConnection *bus_conn, char device_number)
    : bus_conn_(bus_conn), device_number_(device_number),
      fw_state_(FW_NO_CUSTOM_CODE) {}

CBM1541Drive::~CBM1541Drive() {
  // Close direct access channels that have been initialized.
  // Ignore the result of this operation. It shouldn't fail, but if it
  // does, there's nothing we can (and should) beyond destroying ourselves.
  // TODO(aeckleder): Log any error that occurs here.
  if (write_da_chan_ != -1) {
    IECStatus status;
    bus_conn_->CloseChannel(device_number_, write_da_chan_, &status);
    write_da_chan_ = -1;
  }
  if (read_da_chan_ != -1) {
    IECStatus status;
    bus_conn_->CloseChannel(device_number_, read_da_chan_, &status);
    read_da_chan_ = -1;
  }
}

bool CBM1541Drive::FormatDiscLowLevel(size_t num_tracks, IECStatus *status) {
  if (!SetFirmwareState(FW_CUSTOM_FORMATTING_CODE, status))
    return false;

  // TODO(aeckleder): Pass num_tracks to format code.
  std::string request = "M-E";
  request.append(1, char(kFormatEntryPoint & 0xff));
  request.append(1, char(kFormatEntryPoint >> 8));
  if (!bus_conn_->WriteToChannel(device_number_, 15, request, status)) {
    return false;
  }
  // Get the result for the disc format.
  std::string response;
  if (!bus_conn_->ReadFromChannel(device_number_, 15, &response, status)) {
    return false;
  }
  if (response != kOKResponse) {
    SetError(IECStatus::DRIVE_ERROR, response, status);
    return false;
  }
  return true;
}

bool CBM1541Drive::GetNumSectors(size_t *num_sectors, IECStatus *status) {
  // Hardcoded 35 tracks disc.
  // TODO(aeckleder): 40 track discs have 768 blocks.
  *num_sectors = 683;
  return true;
}

bool CBM1541Drive::ReadSector(size_t sector_number, std::string *content,
                              IECStatus *status) {
  unsigned int track = 1;
  unsigned int sector = 0;
  GetTrackSector(sector_number, &track, &sector);
  if (track > kMaxTrackNumber) {
    SetError(
        IECStatus::INVALID_ARGUMENT,
        (boost::format("not trying to read from track %u as it might cause "
                       "hardware damage") %
         track)
            .str(),
        status);
    return false;
  }

  if (!SetFirmwareState(FW_CUSTOM_READ_WRITE_CODE, status))
    return false;
  if (!InitDirectAccessChannel(status))
    return false;

  // Read from disc.
  std::string request = "M-E";
  request.append(1, char(kReadWriteBlockEntryPoint & 0xff));
  request.append(1, char(kReadWriteBlockEntryPoint >> 8));
  request.append(1, char(track));
  request.append(1, char(sector));
  request.append(1, char(kReadBlockOption));
  if (!bus_conn_->WriteToChannel(device_number_, 15, request, status)) {
    return false;
  }

  // Reposition buffer pointer
  // TODO(aeckleder): Fix block read code to no longer require this.
  request = (boost::format("B-P:%u 0") % read_da_chan_).str();
  if (!bus_conn_->WriteToChannel(device_number_, 15, request, status)) {
    return false;
  }

  // Read sector content.
  if (!bus_conn_->ReadFromChannel(device_number_, read_da_chan_, content,
                                  status)) {
    return false;
  }

  // Get the result for the read command.
  std::string response;
  if (!bus_conn_->ReadFromChannel(device_number_, 15, &response, status)) {
    return false;
  }
  if (response != kOKResponse) {
    SetError(IECStatus::DRIVE_ERROR, response, status);
    return false;
  }
  return true;
}

bool CBM1541Drive::WriteSector(size_t sector_number, const std::string &content,
                               IECStatus *status) {
  if (content.size() != kNumBytesPerSector) {
    SetError(IECStatus::INVALID_ARGUMENT,
             (boost::format("content.size(%u) != kNumBytesPerSector(%u)") %
              content.size() % kNumBytesPerSector)
                 .str(),
             status);
    return false;
  }
  unsigned int track = 1;
  unsigned int sector = 0;
  GetTrackSector(sector_number, &track, &sector);
  if (track > kMaxTrackNumber) {
    SetError(IECStatus::INVALID_ARGUMENT,
             (boost::format("not trying to write to track %u as it might cause "
                            "hardware damage") %
              track)
                 .str(),
             status);
    return false;
  }

  if (!SetFirmwareState(FW_CUSTOM_READ_WRITE_CODE, status))
    return false;
  if (!InitDirectAccessChannel(status))
    return false;

  // Write sector content to the buffer.
  if (!bus_conn_->WriteToChannel(device_number_, write_da_chan_, content,
                                 status)) {
    return false;
  }

  // Write the buffer to disc.
  std::string request = "M-E";
  request.append(1, char(kReadWriteBlockEntryPoint & 0xff));
  request.append(1, char(kReadWriteBlockEntryPoint >> 8));
  request.append(1, char(track));
  request.append(1, char(sector));
  request.append(1, char(kWriteBlockOption));
  if (!bus_conn_->WriteToChannel(device_number_, 15, request, status)) {
    return false;
  }

  // Get the result for the write command.
  std::string response;
  if (!bus_conn_->ReadFromChannel(device_number_, 15, &response, status)) {
    return false;
  }
  if (response != kOKResponse) {
    SetError(IECStatus::DRIVE_ERROR, response, status);
    return false;
  }
  return true;
}

bool CBM1541Drive::ReadCommandChannel(std::string *response,
                                      IECStatus *status) {
  // Accessing the command channel is always ok, no open call necessary.
  return bus_conn_->ReadFromChannel(device_number_, 15, response, status);
}

void CBM1541Drive::GetTrackSector(unsigned int s, unsigned int *track,
                                  unsigned int *sector) {
  const unsigned int area1_sectors = 357;
  const unsigned int area12_sectors = area1_sectors + 133;
  const unsigned int area123_sectors = area12_sectors + 108;
  if (s >= area123_sectors) {
    *sector = s - area123_sectors;
    *track = 31 + (*sector / 17);
    *sector = *sector % 17;
    return;
  }
  if (s >= area12_sectors) {
    *sector = s - area12_sectors;
    *track = 25 + (*sector / 18);
    *sector = *sector % 18;
    return;
  }
  if (s >= area1_sectors) {
    *sector = s - area1_sectors;
    *track = 18 + (*sector / 19);
    *sector = *sector % 19;
    return;
  }
  *sector = s;
  *track = (*sector / 21) + 1;
  *sector = *sector % 21;
}

bool CBM1541Drive::SetFirmwareState(CBM1541Drive::FirmwareState firmware_state,
                                    IECStatus *status) {
  // Exit early if we're already in the desired state.
  if (fw_state_ == firmware_state)
    return true;
  fw_state_ = firmware_state;

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

bool CBM1541Drive::InitDirectAccessChannel(IECStatus *status) {
  if (write_da_chan_ == -1) {
    if (!OpenChannelWithBuffer(kWriteDirectAccessChannel, 1, status)) {
      return false;
    }
    write_da_chan_ = kWriteDirectAccessChannel;
  }
  if (read_da_chan_ == -1) {
    if (!OpenChannelWithBuffer(kReadDirectAccessChannel, 3, status)) {
      return false;
    }
    read_da_chan_ = kReadDirectAccessChannel;
  }
  return true;
}

bool CBM1541Drive::OpenChannelWithBuffer(int channel, int buffer,
                                         IECStatus *status) {
  if (!bus_conn_->OpenChannel(device_number_, channel,
                              (boost::format("#%u") % buffer).str(), status)) {
    return false;
  }
  // Set buffer pointer to beginning of buffer.
  auto cmd = boost::format("B-P:%u 0") % channel;
  if (!bus_conn_->WriteToChannel(device_number_, 15, cmd.str(), status)) {
    return false;
  }
  return true;
}
