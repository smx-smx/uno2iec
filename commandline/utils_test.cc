#include <unistd.h>
#include <thread>

#include "gtest/gtest.h"
#include "utils.h"

class BufferedReadWriterTest : public ::testing::Test {
 protected:
  void SetUp() override { ASSERT_EQ(pipe(pipefd_), 0); }

  void TearDown() override {
    close(pipefd_[0]);
    if (producer_.joinable()) {
      producer_.join();
    }
  }

  // Write input to the writing side of the pipe from a separate thread.
  void ProduceString(const std::string& input) {
    producer_ = std::thread([this, input]() {
      IECStatus status;
      BufferedReadWriter writer(pipefd_[1]);
      EXPECT_TRUE(writer.WriteString(input, &status)) << status.message;
      close(pipefd_[1]);
    });
  }

  std::thread producer_;
  int pipefd_[2];
};

TEST_F(BufferedReadWriterTest, ReadWriteString) {
  const std::string kTerminatedString = "terminated_string\r";
  ProduceString(kTerminatedString);

  std::string result;
  IECStatus status;
  BufferedReadWriter reader(pipefd_[0]);
  EXPECT_TRUE(reader.ReadTerminatedString('\r', 256, &result, &status));
  close(pipefd_[0]);
  // We want to see the original string without the terminator here.
  EXPECT_EQ(result, kTerminatedString.substr(0, kTerminatedString.size() - 1))
      << status.message;
}

TEST_F(BufferedReadWriterTest, ReadWriteStringNoTerminator) {
  const std::string kTerminatedString = "terminated_string\r";
  ProduceString(kTerminatedString);

  std::string result;
  IECStatus status;
  BufferedReadWriter reader(pipefd_[0]);
  EXPECT_FALSE(reader.ReadTerminatedString('\r', 2, &result, &status));
  EXPECT_EQ(status.status_code, IECStatus::CONNECTION_FAILURE)
      << status.message;
  // We should be able to recover from this problem by re-issuing the same
  // request with a larger read ahead.
  EXPECT_TRUE(reader.ReadTerminatedString('\r', 256, &result, &status));
  EXPECT_EQ(result, kTerminatedString.substr(0, kTerminatedString.size() - 1))
      << status.message;

  close(pipefd_[0]);
}

TEST_F(BufferedReadWriterTest, ReadWriteLookAheadExceedsBuffer) {
  const std::string kTerminatedString = "terminated_string\r";
  ProduceString(kTerminatedString);

  std::string result;
  IECStatus status;
  BufferedReadWriter reader(pipefd_[0]);
  EXPECT_TRUE(
      reader.ReadTerminatedString('\r', kReadBufferSize + 1, &result, &status));
  close(pipefd_[0]);
  // We requested a read ahead of more than our buffer size. We'll refuse that.
  EXPECT_EQ(status.status_code, IECStatus::INVALID_ARGUMENT)
      << status.message;
}

TEST_F(BufferedReadWriterTest, ReadWriteStringMultiLine) {
  const std::string kTerminatedString =
      "terminated_string\ranother_terminated_string\r";
  ProduceString(kTerminatedString);

  std::string result;
  IECStatus status;
  BufferedReadWriter reader(pipefd_[0]);
  EXPECT_TRUE(reader.ReadTerminatedString('\r', 256, &result, &status));
  EXPECT_EQ(result, "terminated_string") << status.message;
  // Now read the second line. It should be whole.
  EXPECT_TRUE(reader.ReadTerminatedString('\r', 256, &result, &status));
  EXPECT_EQ(result, "another_terminated_string") << status.message;

  close(pipefd_[0]);
}
