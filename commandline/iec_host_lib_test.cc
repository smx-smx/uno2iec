#include <csignal>
#include <functional>
#include <mutex>
#include <sys/socket.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>

#include "boost/format.hpp"
#include "iec_host_lib.h"
#include "gtest/gtest.h"

class IECBusConnectionTest : public ::testing::Test {
protected:
  void SetUp() override {
    // Not creating a pipe here, strictly speaking, because we want
    // bidirectional comms.
    ASSERT_EQ(socketpair(AF_LOCAL, SOCK_STREAM, 0, pipefd_), 0);
    producer_ = std::thread(
        std::bind(&IECBusConnectionTest::RunArduinoFakeThread, this));
  }

  void TearDown() override {
    // No need to close the other fd as it ends up being owned by the IEC bus
    // connection.
    close(pipefd_[1]);
    // TODO(aeckleder): Signal shutdown.
    producer_.join();
  }

  // Executed in a background thread, this will serve as a fake Arduino,
  // going through the initialization protocol, then serving preconfigured
  // responses stored in request_response_map_.
  void RunArduinoFakeThread() {
    IECStatus status;
    BufferedReadWriter writer(pipefd_[1]);
    EXPECT_TRUE(writer.WriteString("connect_arduino:3\r", &status))
        << status.message;
    std::string r;
    EXPECT_TRUE(writer.ReadTerminatedString('\r', 256, &r, &status))
        << status.message;
    EXPECT_EQ(r.substr(0, 3), "OK>");

    while (true) {
      // Closing the fd is our termination condition, keeping it simple.
      if (!writer.ReadUpTo(1, 1, &r, &status))
        return;
      // Read required number of params as determined by the command we receive.
      std::string params;
      switch (r[0]) {
      case 'r':
        // No additional parameters to read.
        break;
      case 'o': {
        // Size of the command string is variable. Read fixed extra data first,
        // then read the command string.
        if (!writer.ReadUpTo(3, 3, &params, &status))
          return;
        std::string cmd_string;
        if (!writer.ReadUpTo(params[2], params[2], &cmd_string, &status))
          return;
        r = r + params + cmd_string;
      } break;
      case 'g':
      case 'c':
        if (!writer.ReadUpTo(2, 2, &params, &status))
          return;
        r = r + params;
        break;
      }

      // r now contains the entire request string. Try to match it to a
      // response.
      std::string response;
      {
        std::lock_guard<std::mutex> lock(request_response_map_m_);
        auto it = request_response_map_.find(r);
        EXPECT_TRUE(it != request_response_map_.end()) << "Unknown request: "
                                                       << r;
        if (it != request_response_map_.end()) {
          response = it->second;
        }
      }
      if (response.size() > 0) {
        EXPECT_TRUE(writer.WriteString(response, &status)) << status.message;
      }
    }
  }

  void AddRequestResponse(const std::string &req, const std::string &resp) {
    std::lock_guard<std::mutex> lock(request_response_map_m_);
    request_response_map_.emplace(req, resp);
    std::cout << "Adding '" << req << "' -> '" << resp << "'" << std::endl;
  }

  std::thread producer_;
  int pipefd_[2] = {-1, -1};

  // Provides a map from request to response to be used by the
  // background thread.
  std::map<std::string, std::string> request_response_map_;
  std::mutex request_response_map_m_;
};

TEST_F(IECBusConnectionTest, BasicProtocolTest) {
  IECBusConnection bus_conn(
      pipefd_[0],
      [](char level, const std::string &channel, const std::string &message) {
        // We'd like to learn about errors we produce.
        ASSERT_NE(level, 'E') << level << ":" << channel << ": " << message;
        std::cout << level << ":" << channel << ":" << message;
      });
  AddRequestResponse("r", "s\r");
  AddRequestResponse(
      (boost::format("o%c%c%cN:SOMEDISC,ID") % char(8) % char(15) % char(13))
          .str(),
      "s\r");
  AddRequestResponse((boost::format("c%c%c") % char(8) % char(15)).str(),
                     "s\r");
  AddRequestResponse((boost::format("g%c%c") % char(8) % char(15)).str(),
                     "r00, OK,00,00\\r\rs\r");
  IECStatus status;
  EXPECT_TRUE(bus_conn.Initialize(&status)) << status.message;
  EXPECT_TRUE(bus_conn.Reset(&status));
  EXPECT_TRUE(bus_conn.OpenChannel(8, 15, "N:SOMEDISC,ID", &status));
  std::string response;
  EXPECT_TRUE(bus_conn.ReadFromChannel(8, 15, &response, &status));
  EXPECT_EQ(response, "00, OK,00,00\r");
  EXPECT_TRUE(bus_conn.CloseChannel(8, 15, &status));
}
