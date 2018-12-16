#include <unistd.h>
#include <thread>

#include "gtest/gtest.h"
#include "utils.h"

TEST(UtilsTest, ReadWriteString) {
  int pipefd[2];
  ASSERT_EQ(pipe(pipefd), 0);
  const std::string kTerminatedString = "terminated_string\r";
  std::thread producer([pipefd, kTerminatedString]() {
    IECStatus status;
    EXPECT_TRUE(WriteString(pipefd[1], kTerminatedString, &status))
        << status.message;
    close(pipefd[1]);
  });
  std::string result;
  IECStatus status;
  EXPECT_TRUE(ReadTerminatedString(pipefd[0], '\r', 256, &result, &status));
  producer.join();
  close(pipefd[0]);
  // We want to see the original string without the terminator here.
  EXPECT_EQ(result, kTerminatedString.substr(0, kTerminatedString.size() - 1))
      << status.message;
}

TEST(UtilsTest, ReadWriteStringNoTerminator) {
  int pipefd[2];
  ASSERT_EQ(pipe(pipefd), 0);
  const std::string kTerminatedString = "terminated_string\r";
  std::thread producer([pipefd, kTerminatedString]() {
    IECStatus status;
    EXPECT_TRUE(WriteString(pipefd[1], kTerminatedString, &status))
        << status.message;
    close(pipefd[1]);
  });
  std::string result;
  IECStatus status;
  EXPECT_FALSE(ReadTerminatedString(pipefd[0], '\r', 2, &result, &status));
  producer.join();
  close(pipefd[0]);
  EXPECT_EQ(status.status_code, IECStatus::CONNECTION_FAILURE)
      << status.message;
}
