#include <csignal>
#include <thread>
#include <unistd.h>

#include "utils.h"
#include "gtest/gtest.h"

class BufferedReadWriterTest : public ::testing::Test {
protected:
  void SetUp() override {
    // Ignore broken pipe. It happens naturally during our tests
    // and should abort other tests.
    signal(SIGPIPE, SIG_IGN);
    ASSERT_EQ(pipe(pipefd_), 0);
  }

  void TearDown() override {
    close(pipefd_[0]);
    close(pipefd_[1]);
    if (producer_.joinable()) {
      producer_.join();
    }
  }

  // Write input to the writing side of the pipe from a separate thread.
  void ProduceString(const std::string &input) {
    if (producer_.joinable()) {
      // Wait for any previous producer to finish.
      producer_.join();
    }
    producer_ = std::thread([this, input]() {
      IECStatus status;
      BufferedReadWriter writer(pipefd_[1]);
      // Write out the specified string. Note that this may fail for
      // some tests, because not all tests consume all the data.
      // Hence, there's no point in looking at the error.
      writer.WriteString(input, &status);
    });
  }

  std::thread producer_;
  int pipefd_[2] = {-1, -1};
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
}

TEST_F(BufferedReadWriterTest, ReadWriteLookAheadExceedsBuffer) {
  const std::string kTerminatedString = "terminated_string\r";
  ProduceString(kTerminatedString);

  std::string result;
  IECStatus status;
  BufferedReadWriter reader(pipefd_[0]);
  EXPECT_FALSE(
      reader.ReadTerminatedString('\r', kMaxReadAhead + 1, &result, &status));
  // We requested a read ahead of more than our buffer size. We'll refuse that.
  EXPECT_EQ(status.status_code, IECStatus::INVALID_ARGUMENT) << status.message;
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
}

TEST_F(BufferedReadWriterTest, ReadUpTo) {
  const std::string kTerminatedString = "terminated_string\ranother";
  ProduceString(kTerminatedString);

  std::string result;
  IECStatus status;
  BufferedReadWriter reader(pipefd_[0]);

  // min_length > max_length should produce an error.
  EXPECT_FALSE(reader.ReadUpTo(2, 1, &result, &status));
  EXPECT_EQ(status.status_code, IECStatus::INVALID_ARGUMENT) << status.message;
  status.Clear();

  EXPECT_TRUE(reader.ReadUpTo(1, 2, &result, &status)) << status.message;
  EXPECT_EQ(result, "te");

  // Now read the rest of the first line up to the '\r'. This should leave
  // some data in the buffer.
  EXPECT_TRUE(reader.ReadTerminatedString('\r', 256, &result, &status));
  EXPECT_EQ(result, "rminated_string") << status.message;

  // Produce more data that is guaranteed not to be buffered yet.
  const std::string kTerminatedString2 = "_terminated_string\rextra_data\r";
  ProduceString(kTerminatedString2);

  // We expect to seamlessly pick up the next few characters after the '\r',
  // including two of the newly produced ones that aren't in the buffer yet.
  EXPECT_TRUE(reader.ReadUpTo(9, 9, &result, &status)) << status.message;
  EXPECT_EQ(result, "another_t");

  // Again, read the rest of the line.
  EXPECT_TRUE(reader.ReadTerminatedString('\r', 256, &result, &status));
  EXPECT_EQ(result, "erminated_string") << status.message;

  // Finally, read more data that is already fully in the buffer.
  EXPECT_TRUE(reader.ReadUpTo(6, 6, &result, &status)) << status.message;
  EXPECT_EQ(result, "extra_");

  // Make sure the rest of the buffer can be consumed by reading the next line.
  EXPECT_TRUE(reader.ReadTerminatedString('\r', 256, &result, &status));
  EXPECT_EQ(result, "data") << status.message;
}
