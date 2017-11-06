#include <chrono>
#include <experimental/filesystem>
#include <iostream>

#include <gflags/gflags.h>
#include <glog/logging.h>
#include "gtest/gtest.h"

#include "communication/bolt/client.hpp"
#include "communication/bolt/v1/session.hpp"
#include "communication/server.hpp"
#include "io/network/network_endpoint.hpp"
#include "io/network/socket.hpp"

DECLARE_int32(query_execution_time_sec);
DECLARE_int32(session_inactivity_timeout);

using namespace std::chrono_literals;
class TestClientSocket;
using io::network::NetworkEndpoint;
using io::network::Socket;
using communication::bolt::SessionData;
using communication::bolt::ClientException;
using SessionT = communication::bolt::Session<Socket>;
using ResultStreamT = SessionT::ResultStreamT;
using ServerT = communication::Server<SessionT, SessionData>;
using ClientT = communication::bolt::Client<Socket>;

class RunningServer {
 public:
  ~RunningServer() {
    server_.Shutdown();
    server_thread_.join();
  }

  SessionData session_data_;
  NetworkEndpoint endpoint_{"127.0.0.1", "0"};
  ServerT server_{endpoint_, session_data_};
  std::thread server_thread_{[&] { server_.Start(1); }};
};

class TestClient : public ClientT {
 public:
  TestClient(NetworkEndpoint endpoint)
      : ClientT(
            [&] {
              Socket socket;
              socket.Connect(endpoint);
              return socket;
            }(),
            "", "") {}
};

TEST(NetworkTimeouts, InactiveSession) {
  FLAGS_query_execution_time_sec = 60;
  FLAGS_session_inactivity_timeout = 1;
  RunningServer rs;

  TestClient client(rs.server_.endpoint());
  // Check that we can execute first query.
  client.Execute("RETURN 1", {});

  // After sleep, session should still be alive.
  std::this_thread::sleep_for(500ms);
  client.Execute("RETURN 1", {});

  // After sleep, session should still be alive.
  std::this_thread::sleep_for(500ms);
  client.Execute("RETURN 1", {});

  // After sleep, session should still be alive.
  std::this_thread::sleep_for(500ms);
  client.Execute("RETURN 1", {});

  // After sleep, session should have timed out.
  std::this_thread::sleep_for(1500ms);
  EXPECT_THROW(client.Execute("RETURN 1", {}), ClientException);
}

TEST(NetworkTimeouts, TimeoutInMultiCommandTransaction) {
  FLAGS_query_execution_time_sec = 1;
  FLAGS_session_inactivity_timeout = 60;
  RunningServer rs;

  TestClient client(rs.server_.endpoint());

  // Start explicit multicommand transaction.
  client.Execute("BEGIN", {});
  client.Execute("RETURN 1", {});

  // Session should still be alive.
  std::this_thread::sleep_for(500ms);
  client.Execute("RETURN 1", {});

  // Session shouldn't be alive anymore.
  std::this_thread::sleep_for(2s);
  EXPECT_THROW(client.Execute("RETURN 1", {}), ClientException);
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  google::InitGoogleLogging(argv[0]);
  return RUN_ALL_TESTS();
}