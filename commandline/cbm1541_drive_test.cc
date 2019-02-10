#include "cbm1541_drive.h"

#include "boost/format.hpp"
#include "gmock/gmock.h"
#include "iec_host_lib.h"
#include "gtest/gtest.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::StartsWith;

class MockIECBusConnection : public IECBusConnection {
public:
  MockIECBusConnection() : IECBusConnection(0, nullptr) {}

  MOCK_METHOD1(Reset, bool(IECStatus *status));
  MOCK_METHOD4(OpenChannel,
               bool(char device_number, char channel,
                    const std::string &data_string, IECStatus *status));
  MOCK_METHOD4(ReadFromChannel, bool(char device_number, char channel,
                                     std::string *result, IECStatus *status));
  MOCK_METHOD4(WriteToChannel,
               bool(char device_number, char channel,
                    const std::string &data_string, IECStatus *status));
  MOCK_METHOD3(CloseChannel,
               bool(char device_number, char channel, IECStatus *status));
};

class CBM1541DriveTest : public ::testing::Test {};

TEST_F(CBM1541DriveTest, FormatDiscTest) {
  MockIECBusConnection conn;
  CBM1541Drive drive(&conn, 8);
  IECStatus status;

  // We expect to receive one or more memory writes.
  EXPECT_CALL(conn, WriteToChannel(8, 15, StartsWith("M-W"), &status))
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));
  // And when asked for status we'll say that everything is fine.
  EXPECT_CALL(conn, ReadFromChannel(8, 15, _, &status))
      .Times(AtLeast(1))
      .WillRepeatedly(DoAll(SetArgPointee<2>("00, OK,00,00\r"), Return(true)));
  // Finally, we expect a single memory execute.
  EXPECT_CALL(conn, WriteToChannel(8, 15, StartsWith("M-E"), &status))
      .Times(1)
      .WillOnce(Return(true));

  EXPECT_TRUE(drive.FormatDiscLowLevel(40, &status)) << status.message;
}
