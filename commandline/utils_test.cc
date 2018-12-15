#include "utils.h"
#include "gtest/gtest.h"

TEST(UtilsTest, ReadTerminatedString) {
  std::string result;
  IECStatus status;
  EXPECT_TRUE(ReadTerminatedString(-1, '\r', 256, &result, &status));
}

