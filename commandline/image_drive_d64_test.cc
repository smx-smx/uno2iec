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
const size_t kNumBytesPerSector = 256;

class ImageDriveD64Test : public ::testing::Test {
public:
  void SetUp() {
    // Create a temp image we'll be working it and populate it.
    image_path_ =
        (boost::filesystem::temp_directory_path() / "image_XXXXXX").string();
    // Requires C++11.
    int fd = mkstemp(&image_path_[0]);
    for (size_t s = 0; s < kTestImageNumSectors; ++s) {
      unsigned char buffer[kNumBytesPerSector];
      for (size_t c = 0; c < kNumBytesPerSector; ++c) {
        // Fill buffer with a counter starting at sector number for each
        // sector and wrapping over as required.
        buffer[c] = (s + c) % 256;
      }
      EXPECT_TRUE(write(fd, buffer, sizeof(buffer)) == sizeof(buffer));
    }
    EXPECT_TRUE(close(fd) == 0);
  }

  void TearDown() {
    // Remove image.
    EXPECT_TRUE(unlink(image_path_.c_str()) == 0);
  }

protected:
  // Path to a generated test image.
  std::string image_path_;
};

TEST_F(ImageDriveD64Test, ReadSectorTest) {
  ImageDriveD64 drive(image_path_, /*read_only=*/true);

  IECStatus status;
  std::string content;
  EXPECT_TRUE(drive.ReadSector(42, &content, &status)) << status.message;
}
