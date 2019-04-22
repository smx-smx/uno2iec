#include <boost/filesystem.hpp>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "image_drive_d64.h"

#include "boost/format.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

const size_t kTestImageNumSectors = 768;

class ImageDriveD64Test : public ::testing::Test {
public:
  void SetUp() {
    // Create a temp image we'll be working it and populate it.
    image_path_ =
        (boost::filesystem::temp_directory_path() / "image_XXXXXX").string();
    // Requires C++11.
    int fd = mkstemp(&image_path_[0]);
    for (size_t s = 0; s < kTestImageNumSectors; ++s) {
      unsigned char buffer[DriveInterface::kNumBytesPerSector];
      FillTestBuffer(buffer, s);
      EXPECT_TRUE(write(fd, buffer, sizeof(buffer)) == sizeof(buffer));
    }
    EXPECT_TRUE(close(fd) == 0);
  }

  void TearDown() {
    // Remove image.
    EXPECT_TRUE(unlink(image_path_.c_str()) == 0);
  }

protected:
  void FillTestBuffer(unsigned char *buffer, size_t sector_number) {
    for (size_t c = 0; c < DriveInterface::kNumBytesPerSector; ++c) {
      // Fill buffer with a counter starting at sector number for each
      // sector and wrapping over as required.
      buffer[c] = (sector_number + c) % 256;
    }
  }

  // Path to a generated test image.
  std::string image_path_;
};

TEST_F(ImageDriveD64Test, ReadSectorTest) {
  ImageDriveD64 drive(image_path_, /*read_only=*/true);

  IECStatus status;
  size_t num_sectors = 0;
  EXPECT_TRUE(drive.GetNumSectors(&num_sectors, &status));
  EXPECT_EQ(num_sectors, kTestImageNumSectors);

  // Read all the sectors on the disc, in reverse order to make sure
  // we're seeking to the requested sector.
  for (int s = num_sectors - 1; s >= 0; --s) {
    std::string content;
    EXPECT_TRUE(drive.ReadSector(s, &content, &status)) << status.message;

    // We expect our sector content to match.
    std::string golden;
    golden.resize(DriveInterface::kNumBytesPerSector);
    FillTestBuffer(reinterpret_cast<unsigned char *>(&golden[0]), s);
    EXPECT_EQ(golden, content);
  }
}
