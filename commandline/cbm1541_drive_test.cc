#include "cbm1541_drive.h"

#include "boost/format.hpp"
#include "iec_host_lib.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::StartsWith;
using ::testing::StrEq;

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

  // Done with one call, prepare for the next one.
  ::testing::Mock::VerifyAndClearExpectations(&conn);

  // No need to do another upload, our formatting code is already
  // installed on the drive and ready to execute.
  EXPECT_CALL(conn, WriteToChannel(8, 15, StartsWith("M-E"), &status))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(conn, ReadFromChannel(8, 15, _, &status))
      .Times(1)
      .WillOnce(DoAll(SetArgPointee<2>("00, OK,00,00\r"), Return(true)));

  EXPECT_TRUE(drive.FormatDiscLowLevel(40, &status)) << status.message;

  // Done with one call, prepare for the next one.
  ::testing::Mock::VerifyAndClearExpectations(&conn);

  // We expect connection failures to be passed through.
  IECStatus failure_status;
  failure_status.status_code = IECStatus::IEC_CONNECTION_FAILURE;
  EXPECT_CALL(conn, WriteToChannel(8, 15, StartsWith("M-E"), &status))
      .Times(1)
      .WillOnce(DoAll(SetArgPointee<3>(failure_status), Return(false)));
  EXPECT_FALSE(drive.FormatDiscLowLevel(40, &status));
  EXPECT_EQ(status.status_code, IECStatus::IEC_CONNECTION_FAILURE);

  // Done with one call, prepare for the next one.
  ::testing::Mock::VerifyAndClearExpectations(&conn);

  // Return a logical drive error after executing format.
  EXPECT_CALL(conn, WriteToChannel(8, 15, StartsWith("M-E"), &status))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(conn, ReadFromChannel(8, 15, _, &status))
      .Times(1)
      .WillOnce(DoAll(SetArgPointee<2>("42, ERROR,42,42\r"), Return(true)));
  EXPECT_FALSE(drive.FormatDiscLowLevel(40, &status));
  EXPECT_EQ(status.status_code, IECStatus::DRIVE_ERROR);
}

TEST_F(CBM1541DriveTest, WriteSectorTest) {
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

  // Opening DA channel and positioning block pointer (done once).
  EXPECT_CALL(conn, OpenChannel(8, 2, "#1", &status))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(conn, WriteToChannel(8, 15, StrEq("B-P:2 0"), &status))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(conn, OpenChannel(8, 3, "#3", &status))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(conn, WriteToChannel(8, 15, StrEq("B-P:3 0"), &status))
      .Times(1)
      .WillOnce(Return(true));

  std::string content(256, 0x42);
  // Expect sector content to be written to our DA channel.
  EXPECT_CALL(conn, WriteToChannel(8, 2, StrEq(content), &status))
      .Times(1)
      .WillOnce(Return(true));

  // Finally, we expect a single memory execute.
  EXPECT_CALL(conn, WriteToChannel(8, 15, StartsWith("M-E"), &status))
      .Times(1)
      .WillOnce(Return(true));

  EXPECT_TRUE(drive.WriteSector(42, content, &status)) << status.message;

  // Done with one call, prepare for the next one.
  ::testing::Mock::VerifyAndClearExpectations(&conn);

  // Expect sector content to be written to our DA channel.
  EXPECT_CALL(conn, WriteToChannel(8, 2, StrEq(content), &status))
      .Times(1)
      .WillOnce(Return(true));

  // No need to do another upload, our write sector code is already
  // installed on the drive and ready to execute.
  EXPECT_CALL(conn, WriteToChannel(8, 15, StartsWith("M-E"), &status))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(conn, ReadFromChannel(8, 15, _, &status))
      .Times(1)
      .WillOnce(DoAll(SetArgPointee<2>("00, OK,00,00\r"), Return(true)));

  EXPECT_TRUE(drive.WriteSector(43, content, &status)) << status.message;

  // Done with one call, prepare for the next one.
  ::testing::Mock::VerifyAndClearExpectations(&conn);

  EXPECT_CALL(conn, WriteToChannel(8, 2, StrEq(content), &status))
      .Times(1)
      .WillOnce(Return(true));

  // We expect connection failures to be passed through.
  IECStatus failure_status;
  failure_status.status_code = IECStatus::IEC_CONNECTION_FAILURE;
  EXPECT_CALL(conn, WriteToChannel(8, 15, StartsWith("M-E"), &status))
      .Times(1)
      .WillOnce(DoAll(SetArgPointee<3>(failure_status), Return(false)));
  EXPECT_FALSE(drive.WriteSector(42, content, &status));
  EXPECT_EQ(status.status_code, IECStatus::IEC_CONNECTION_FAILURE);

  // Done with one call, prepare for the next one.
  ::testing::Mock::VerifyAndClearExpectations(&conn);

  EXPECT_CALL(conn, WriteToChannel(8, 2, StrEq(content), &status))
      .Times(1)
      .WillOnce(Return(true));

  // Return a logical drive error after executing write sector.
  EXPECT_CALL(conn, WriteToChannel(8, 15, StartsWith("M-E"), &status))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(conn, ReadFromChannel(8, 15, _, &status))
      .Times(1)
      .WillOnce(DoAll(SetArgPointee<2>("42, ERROR,42,42\r"), Return(true)));
  EXPECT_FALSE(drive.WriteSector(42, content, &status));
  EXPECT_EQ(status.status_code, IECStatus::DRIVE_ERROR);

  // Done with one call, prepare for the next one.
  ::testing::Mock::VerifyAndClearExpectations(&conn);

  EXPECT_CALL(conn, WriteToChannel(8, 2, StrEq(content), &status))
      .Times(1)
      .WillOnce(DoAll(SetArgPointee<3>(failure_status), Return(false)));
  EXPECT_FALSE(drive.WriteSector(42, content, &status));
  EXPECT_EQ(status.status_code, IECStatus::IEC_CONNECTION_FAILURE);

  // The destructor of our CBM1541Drive will call CloseChannel.
  EXPECT_CALL(conn, CloseChannel(8, 2, _)).Times(1).WillOnce(Return(true));
  EXPECT_CALL(conn, CloseChannel(8, 3, _)).Times(1).WillOnce(Return(true));
}

TEST_F(CBM1541DriveTest, ReadSectorTest) {
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

  // Opening DA channel and positioning block pointer (done once).
  EXPECT_CALL(conn, OpenChannel(8, 2, "#1", &status))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(conn, WriteToChannel(8, 15, StrEq("B-P:2 0"), &status))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(conn, OpenChannel(8, 3, "#3", &status))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(conn, WriteToChannel(8, 15, StrEq("B-P:3 0"), &status))
      .Times(1)
      .WillOnce(Return(true));

  // Finally, we expect a single memory execute.
  EXPECT_CALL(conn, WriteToChannel(8, 15, StartsWith("M-E"), &status))
      .Times(1)
      .WillOnce(Return(true));

  std::string content(256, 0x42);
  // Expect sector content to be read from our DA channel.
  EXPECT_CALL(conn, ReadFromChannel(8, 3, _, &status))
      .Times(1)
      .WillOnce(DoAll(SetArgPointee<2>(content), Return(true)));

  std::string read_content;
  EXPECT_TRUE(drive.ReadSector(42, &read_content, &status)) << status.message;
  EXPECT_EQ(read_content, content);

  // Done with one call, prepare for the next one.
  ::testing::Mock::VerifyAndClearExpectations(&conn);

  // We expect a single memory execute.
  EXPECT_CALL(conn, WriteToChannel(8, 15, StartsWith("M-E"), &status))
      .Times(1)
      .WillOnce(Return(true));

  // Expect sector content to be read from our DA channel.
  EXPECT_CALL(conn, ReadFromChannel(8, 3, _, &status))
      .Times(1)
      .WillOnce(DoAll(SetArgPointee<2>(content), Return(true)));

  EXPECT_CALL(conn, ReadFromChannel(8, 15, _, &status))
      .Times(1)
      .WillOnce(DoAll(SetArgPointee<2>("00, OK,00,00\r"), Return(true)));

  read_content.clear();
  EXPECT_TRUE(drive.ReadSector(43, &read_content, &status)) << status.message;
  EXPECT_EQ(read_content, content);

  // Done with one call, prepare for the next one.
  ::testing::Mock::VerifyAndClearExpectations(&conn);

  // We expect connection failures to be passed through.
  IECStatus failure_status;
  failure_status.status_code = IECStatus::IEC_CONNECTION_FAILURE;
  // We expect a memory execute.
  EXPECT_CALL(conn, WriteToChannel(8, 15, StartsWith("M-E"), &status))
      .Times(1)
      .WillOnce(DoAll(SetArgPointee<3>(failure_status), Return(false)));
  EXPECT_FALSE(drive.ReadSector(42, &read_content, &status));
  EXPECT_EQ(status.status_code, IECStatus::IEC_CONNECTION_FAILURE);

  // Done with one call, prepare for the next one.
  ::testing::Mock::VerifyAndClearExpectations(&conn);

  EXPECT_CALL(conn, WriteToChannel(8, 15, StartsWith("M-E"), &status))
      .Times(1)
      .WillOnce(Return(true));

  // Expect sector content to be read from our DA channel.
  EXPECT_CALL(conn, ReadFromChannel(8, 3, _, &status))
      .Times(1)
      .WillOnce(DoAll(SetArgPointee<2>(content), Return(true)));

  EXPECT_CALL(conn, ReadFromChannel(8, 15, _, &status))
      .Times(1)
      .WillOnce(DoAll(SetArgPointee<2>("42, ERROR,42,42\r"), Return(true)));

  EXPECT_FALSE(drive.ReadSector(42, &read_content, &status));
  EXPECT_EQ(status.status_code, IECStatus::DRIVE_ERROR);

  // Done with one call, prepare for the next one.
  ::testing::Mock::VerifyAndClearExpectations(&conn);

  EXPECT_CALL(conn, WriteToChannel(8, 15, StartsWith("M-E"), &status))
      .Times(1)
      .WillOnce(Return(true));

  EXPECT_CALL(conn, ReadFromChannel(8, 3, _, &status))
      .Times(1)
      .WillOnce(DoAll(SetArgPointee<3>(failure_status), Return(false)));
  EXPECT_FALSE(drive.ReadSector(42, &read_content, &status));
  EXPECT_EQ(status.status_code, IECStatus::IEC_CONNECTION_FAILURE);

  // The destructor of our CBM1541Drive will call CloseChannel.
  EXPECT_CALL(conn, CloseChannel(8, 2, _)).Times(1).WillOnce(Return(true));
  EXPECT_CALL(conn, CloseChannel(8, 3, _)).Times(1).WillOnce(Return(true));
}
