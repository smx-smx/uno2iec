// DriveInterface implementation on top of a physical CBM 1541 disk drive.

#ifndef CBM1541_DRIVE_H
#define CBM1541_DRIVE_H

#include <map>

#include "drive_interface.h"
#include "iec_host_lib.h"

class CBM1541Drive : public DriveInterface {
public:
  // Instantiate a CBM1541 drive using the specified connection object
  // and device_number. Ownership of the object pointed to by bus_conn
  // is not transferred. The object must stay alive during the lifetime
  // of this drive instance. It is the caller's responsibility to make sure
  // the device specified by device_number is managed exclusively by this
  // CBM1541Drive instance.
  CBM1541Drive(IECBusConnection *bus_conn, char device_number);

  ~CBM1541Drive();

  bool FormatDiscLowLevel(size_t num_tracks, IECStatus *status) override;
  bool GetNumSectors(size_t *num_sectors, IECStatus *status) override;
  bool ReadSector(size_t sector_number, std::string *content,
                  IECStatus *status) override;
  bool WriteSector(size_t sector_number, const std::string &content,
                   IECStatus *status) override;

  // GetTrackSector translates from a sector index to corresponding
  // track and (track local) sector number according to a hardcoded
  // schema matching the 1541's sectors / track configuration.
  static void GetTrackSector(unsigned int s, unsigned int *track,
                             unsigned int *sector);

private:
  // FirmareState represents the different custom firmware code fragments
  // we use to operate the drive.
  enum FirmwareState {
    FW_NO_CUSTOM_CODE, // The drive doesn't have any custom firmware code.
    FW_CUSTOM_FORMATTING_CODE, // Drive holds formatting code.
    FW_CUSTOM_READ_WRITE_CODE, // Drive holds custom read/write routines.
  };

  // Switch firmware state to firmware_state. After this method returns,
  // any custom firmware code associated with this state will have been
  // uploaded. In case of error, returns false and sets status.
  bool SetFirmwareState(FirmwareState firmware_state, IECStatus *status);

  // Write num_bytes of the content pointed to by source to target_address
  // on the drive. Returns true if successful, sets status otherwise.
  bool WriteMemory(unsigned short int target_address, size_t num_bytes,
                   const unsigned char *source, IECStatus *status);

  // Initialize direct access channel if it hasn't been initialized yet.
  bool InitDirectAccessChannel(IECStatus *status);

  // Open the specified channel, associate it with buffer and set the buffer
  // pointer to zero. Returns true if successful, sets status otherwise.
  bool OpenChannelWithBuffer(int channel, int buffer, IECStatus *status);

  // A pointer to the bus we'll be using to talk to the physical device.
  IECBusConnection *bus_conn_;

  // The device number of the physical device we're talking to.
  char device_number_;

  FirmwareState fw_state_;

  struct CustomFirmwareFragment {
    const unsigned char *binary; // Pointer to the actual binary.
    size_t binary_size;          // Size of the binary in bytes.
    size_t loading_address;      // Loading address of the binary.
  };
  static const std::map<FirmwareState, CustomFirmwareFragment> fw_fragment_map_;

  // Direct access channel to use for writing sector content.
  // Initialized lazily by InitDirectAccessChannel().
  int write_da_chan_ = -1;
  // Direct access channel to use for reading sector content.
  // Initialized lazily by InitDirectAccessChannel().
  int read_da_chan_ = -1;
};

#endif // CBM1541_DRIVE_H
