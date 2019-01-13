#include <csignal>
#include <functional>
#include <mutex>
#include <sys/socket.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>

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
    std::string resp;
    EXPECT_TRUE(writer.ReadTerminatedString('\r', 256, &resp, &status))
        << status.message;
    EXPECT_EQ(resp.substr(0, 3), "OK>");
    // TODO(aeckleder): Serve until done.
  }

  void AddRequestResponse(const std::string &req, const std::string &resp) {
    std::lock_guard<std::mutex> lock(request_response_map_m_);
    request_response_map_.emplace(req, resp);
  }

  std::thread producer_;
  int pipefd_[2] = {-1, -1};

  // Provides a map from request to response to be used by the
  // background thread.
  std::map<std::string, std::string> request_response_map_;
  std::mutex request_response_map_m_;
};

TEST_F(IECBusConnectionTest, ReadResponse) {
  IECBusConnection bus_conn(pipefd_[0],
                            [](char level, const std::string &channel,
                               const std::string &message) {});
  IECStatus status;
  EXPECT_TRUE(bus_conn.Initialize(&status)) << status.message;
}
