// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/it2me/it2me_native_messaging_host.h"

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/stringize_macros.h"
#include "base/values.h"
#include "net/base/file_stream.h"
#include "net/base/net_util.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/native_messaging/native_messaging_channel.h"
#include "remoting/host/setup/test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

const char kTestAccessCode[] = "888888";
const int kTestAccessCodeLifetimeInSeconds = 666;
const char kTestClientUsername[] = "some_user@gmail.com";

void VerifyId(scoped_ptr<base::DictionaryValue> response, int expected_value) {
  ASSERT_TRUE(response);

  int value;
  EXPECT_TRUE(response->GetInteger("id", &value));
  EXPECT_EQ(expected_value, value);
}

void VerifyStringProperty(scoped_ptr<base::DictionaryValue> response,
                          const std::string& name,
                          const std::string& expected_value) {
  ASSERT_TRUE(response);

  std::string value;
  EXPECT_TRUE(response->GetString(name, &value));
  EXPECT_EQ(expected_value, value);
}

// Verity the values of the "type" and "id" properties
void VerifyCommonProperties(scoped_ptr<base::DictionaryValue> response,
                            const std::string& type,
                            int id) {
  ASSERT_TRUE(response);

  std::string string_value;
  EXPECT_TRUE(response->GetString("type", &string_value));
  EXPECT_EQ(type, string_value);

  int int_value;
  EXPECT_TRUE(response->GetInteger("id", &int_value));
  EXPECT_EQ(id, int_value);
}

}  // namespace

class MockIt2MeHost : public It2MeHost {
 public:
  MockIt2MeHost(ChromotingHostContext* context,
                scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                base::WeakPtr<It2MeHost::Observer> observer,
                const XmppSignalStrategy::XmppServerConfig& xmpp_server_config,
                const std::string& directory_bot_jid)
      : It2MeHost(context,
                  task_runner,
                  observer,
                  xmpp_server_config,
                  directory_bot_jid) {}

  // It2MeHost overrides
  virtual void Connect() OVERRIDE;
  virtual void Disconnect() OVERRIDE;
  virtual void RequestNatPolicy() OVERRIDE;

 private:
  virtual ~MockIt2MeHost() {}

  void RunSetState(It2MeHostState state);

  DISALLOW_COPY_AND_ASSIGN(MockIt2MeHost);
};

void MockIt2MeHost::Connect() {
  if (!host_context()->ui_task_runner()->BelongsToCurrentThread()) {
    DCHECK(task_runner()->BelongsToCurrentThread());
    host_context()->ui_task_runner()->PostTask(
        FROM_HERE, base::Bind(&MockIt2MeHost::Connect, this));
    return;
  }

  RunSetState(kStarting);
  RunSetState(kRequestedAccessCode);

  std::string access_code(kTestAccessCode);
  base::TimeDelta lifetime =
      base::TimeDelta::FromSeconds(kTestAccessCodeLifetimeInSeconds);
  task_runner()->PostTask(FROM_HERE,
                          base::Bind(&It2MeHost::Observer::OnStoreAccessCode,
                                     observer(),
                                     access_code,
                                     lifetime));

  RunSetState(kReceivedAccessCode);

  std::string client_username(kTestClientUsername);
  task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&It2MeHost::Observer::OnClientAuthenticated,
                 observer(),
                 client_username));

  RunSetState(kConnected);
}

void MockIt2MeHost::Disconnect() {
  if (!host_context()->network_task_runner()->BelongsToCurrentThread()) {
    DCHECK(task_runner()->BelongsToCurrentThread());
    host_context()->network_task_runner()->PostTask(
        FROM_HERE, base::Bind(&MockIt2MeHost::Disconnect, this));
    return;
  }

  RunSetState(kDisconnecting);
  RunSetState(kDisconnected);
}

void MockIt2MeHost::RequestNatPolicy() {}

void MockIt2MeHost::RunSetState(It2MeHostState state) {
  if (!host_context()->network_task_runner()->BelongsToCurrentThread()) {
    host_context()->network_task_runner()->PostTask(
        FROM_HERE, base::Bind(&It2MeHost::SetStateForTesting, this, state));
  } else {
    SetStateForTesting(state);
  }
}

class MockIt2MeHostFactory : public It2MeHostFactory {
 public:
  MockIt2MeHostFactory() {}
  virtual scoped_refptr<It2MeHost> CreateIt2MeHost(
      ChromotingHostContext* context,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      base::WeakPtr<It2MeHost::Observer> observer,
      const XmppSignalStrategy::XmppServerConfig& xmpp_server_config,
      const std::string& directory_bot_jid) OVERRIDE {
    return new MockIt2MeHost(
        context, task_runner, observer, xmpp_server_config, directory_bot_jid);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockIt2MeHostFactory);
};  // MockIt2MeHostFactory

class It2MeNativeMessagingHostTest : public testing::Test {
 public:
  It2MeNativeMessagingHostTest() {}
  virtual ~It2MeNativeMessagingHostTest() {}

  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

 protected:
  scoped_ptr<base::DictionaryValue> ReadMessageFromOutputPipe();
  void WriteMessageToInputPipe(const base::Value& message);

  void VerifyHelloResponse(int request_id);
  void VerifyErrorResponse();
  void VerifyConnectResponses(int request_id);
  void VerifyDisconnectResponses(int request_id);

  // The Host process should shut down when it receives a malformed request.
  // This is tested by sending a known-good request, followed by |message|,
  // followed by the known-good request again. The response file should only
  // contain a single response from the first good request.
  void TestBadRequest(const base::Value& message, bool expect_error_response);
  void TestConnect();

 private:
  void StartHost();
  void StopHost();
  void ExitTest();

  // Each test creates two unidirectional pipes: "input" and "output".
  // It2MeNativeMessagingHost reads from input_read_file and writes to
  // output_write_file. The unittest supplies data to input_write_handle, and
  // verifies output from output_read_handle.
  //
  // unittest -> [input] -> It2MeNativeMessagingHost -> [output] -> unittest
  base::File input_write_file_;
  base::File output_read_file_;

  // Message loop of the test thread.
  scoped_ptr<base::MessageLoop> test_message_loop_;
  scoped_ptr<base::RunLoop> test_run_loop_;

  scoped_ptr<base::Thread> host_thread_;
  scoped_ptr<base::RunLoop> host_run_loop_;

  // Task runner of the host thread.
  scoped_refptr<AutoThreadTaskRunner> host_task_runner_;
  scoped_ptr<remoting::It2MeNativeMessagingHost> host_;

  DISALLOW_COPY_AND_ASSIGN(It2MeNativeMessagingHostTest);
};

void It2MeNativeMessagingHostTest::SetUp() {
  test_message_loop_.reset(new base::MessageLoop());
  test_run_loop_.reset(new base::RunLoop());

  // Run the host on a dedicated thread.
  host_thread_.reset(new base::Thread("host_thread"));
  host_thread_->Start();

  host_task_runner_ = new AutoThreadTaskRunner(
      host_thread_->message_loop_proxy(),
      base::Bind(&It2MeNativeMessagingHostTest::ExitTest,
                 base::Unretained(this)));

  host_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&It2MeNativeMessagingHostTest::StartHost,
                 base::Unretained(this)));

  // Wait until the host finishes starting.
  test_run_loop_->Run();
}

void It2MeNativeMessagingHostTest::TearDown() {
  // Closing the write-end of the input will send an EOF to the native
  // messaging reader. This will trigger a host shutdown.
  input_write_file_.Close();

  // Start a new RunLoop and Wait until the host finishes shutting down.
  test_run_loop_.reset(new base::RunLoop());
  test_run_loop_->Run();

  // Verify there are no more message in the output pipe.
  scoped_ptr<base::DictionaryValue> response = ReadMessageFromOutputPipe();
  EXPECT_FALSE(response);

  // The It2MeNativeMessagingHost dtor closes the handles that are passed to it.
  // So the only handle left to close is |output_read_file_|.
  output_read_file_.Close();
}

scoped_ptr<base::DictionaryValue>
It2MeNativeMessagingHostTest::ReadMessageFromOutputPipe() {
  uint32 length;
  int read_result = output_read_file_.ReadAtCurrentPos(
      reinterpret_cast<char*>(&length), sizeof(length));
  if (read_result != sizeof(length)) {
    // The output pipe has been closed, return an empty message.
    return scoped_ptr<base::DictionaryValue>();
  }

  std::string message_json(length, '\0');
  read_result = output_read_file_.ReadAtCurrentPos(
      string_as_array(&message_json), length);
  if (read_result != static_cast<int>(length)) {
    LOG(ERROR) << "Message size (" << read_result
               << ") doesn't match the header (" << length << ").";
    return scoped_ptr<base::DictionaryValue>();
  }

  scoped_ptr<base::Value> message(base::JSONReader::Read(message_json));
  if (!message || !message->IsType(base::Value::TYPE_DICTIONARY)) {
    LOG(ERROR) << "Malformed message:" << message_json;
    return scoped_ptr<base::DictionaryValue>();
  }

  return scoped_ptr<base::DictionaryValue>(
      static_cast<base::DictionaryValue*>(message.release()));
}

void It2MeNativeMessagingHostTest::WriteMessageToInputPipe(
    const base::Value& message) {
  std::string message_json;
  base::JSONWriter::Write(&message, &message_json);

  uint32 length = message_json.length();
  input_write_file_.WriteAtCurrentPos(reinterpret_cast<char*>(&length),
                                      sizeof(length));
  input_write_file_.WriteAtCurrentPos(message_json.data(), length);
}

void It2MeNativeMessagingHostTest::VerifyHelloResponse(int request_id) {
  scoped_ptr<base::DictionaryValue> response = ReadMessageFromOutputPipe();
  VerifyCommonProperties(response.Pass(), "helloResponse", request_id);
}

void It2MeNativeMessagingHostTest::VerifyErrorResponse() {
  scoped_ptr<base::DictionaryValue> response = ReadMessageFromOutputPipe();
  VerifyStringProperty(response.Pass(), "type", "error");
}

void It2MeNativeMessagingHostTest::VerifyConnectResponses(int request_id) {
  bool connect_response_received = false;
  bool starting_received = false;
  bool requestedAccessCode_received = false;
  bool receivedAccessCode_received = false;
  bool connected_received = false;

  // We expect a total of 5 messages: 1 connectResponse and 4 hostStateChanged.
  for (int i = 0; i < 5; ++i) {
    scoped_ptr<base::DictionaryValue> response = ReadMessageFromOutputPipe();
    ASSERT_TRUE(response);

    std::string type;
    ASSERT_TRUE(response->GetString("type", &type));

    if (type == "connectResponse") {
      EXPECT_FALSE(connect_response_received);
      connect_response_received = true;
      VerifyId(response.Pass(), request_id);
    } else if (type == "hostStateChanged") {
      std::string state;
      ASSERT_TRUE(response->GetString("state", &state));

      std::string value;
      if (state == It2MeNativeMessagingHost::HostStateToString(kStarting)) {
        EXPECT_FALSE(starting_received);
        starting_received = true;
      } else if (state == It2MeNativeMessagingHost::HostStateToString(
                              kRequestedAccessCode)) {
        EXPECT_FALSE(requestedAccessCode_received);
        requestedAccessCode_received = true;
      } else if (state == It2MeNativeMessagingHost::HostStateToString(
                              kReceivedAccessCode)) {
        EXPECT_FALSE(receivedAccessCode_received);
        receivedAccessCode_received = true;

        EXPECT_TRUE(response->GetString("accessCode", &value));
        EXPECT_EQ(kTestAccessCode, value);

        int accessCodeLifetime;
        EXPECT_TRUE(
            response->GetInteger("accessCodeLifetime", &accessCodeLifetime));
        EXPECT_EQ(kTestAccessCodeLifetimeInSeconds, accessCodeLifetime);
      } else if (state ==
                 It2MeNativeMessagingHost::HostStateToString(kConnected)) {
        EXPECT_FALSE(connected_received);
        connected_received = true;

        EXPECT_TRUE(response->GetString("client", &value));
        EXPECT_EQ(kTestClientUsername, value);
      } else {
        ADD_FAILURE() << "Unexpected host state: " << state;
      }
    } else {
      ADD_FAILURE() << "Unexpected message type: " << type;
    }
  }
}

void It2MeNativeMessagingHostTest::VerifyDisconnectResponses(int request_id) {
  bool disconnect_response_received = false;
  bool disconnecting_received = false;
  bool disconnected_received = false;

  // We expect a total of 3 messages: 1 connectResponse and 2 hostStateChanged.
  for (int i = 0; i < 3; ++i) {
    scoped_ptr<base::DictionaryValue> response = ReadMessageFromOutputPipe();
    ASSERT_TRUE(response);

    std::string type;
    ASSERT_TRUE(response->GetString("type", &type));

    if (type == "disconnectResponse") {
      EXPECT_FALSE(disconnect_response_received);
      disconnect_response_received = true;
      VerifyId(response.Pass(), request_id);
    } else if (type == "hostStateChanged") {
      std::string state;
      ASSERT_TRUE(response->GetString("state", &state));
      if (state ==
          It2MeNativeMessagingHost::HostStateToString(kDisconnecting)) {
        EXPECT_FALSE(disconnecting_received);
        disconnecting_received = true;
      } else if (state ==
                 It2MeNativeMessagingHost::HostStateToString(kDisconnected)) {
        EXPECT_FALSE(disconnected_received);
        disconnected_received = true;
      } else {
        ADD_FAILURE() << "Unexpected host state: " << state;
      }
    } else {
      ADD_FAILURE() << "Unexpected message type: " << type;
    }
  }
}

void It2MeNativeMessagingHostTest::TestBadRequest(const base::Value& message,
                                                  bool expect_error_response) {
  base::DictionaryValue good_message;
  good_message.SetString("type", "hello");
  good_message.SetInteger("id", 1);

  WriteMessageToInputPipe(good_message);
  WriteMessageToInputPipe(message);
  WriteMessageToInputPipe(good_message);

  VerifyHelloResponse(1);

  if (expect_error_response)
    VerifyErrorResponse();

  scoped_ptr<base::DictionaryValue> response = ReadMessageFromOutputPipe();
  EXPECT_FALSE(response);
}

void It2MeNativeMessagingHostTest::StartHost() {
  DCHECK(host_task_runner_->RunsTasksOnCurrentThread());

  base::File input_read_file;
  base::File output_write_file;

  ASSERT_TRUE(MakePipe(&input_read_file, &input_write_file_));
  ASSERT_TRUE(MakePipe(&output_read_file_, &output_write_file));

  // Creating a native messaging host with a mock It2MeHostFactory.
  scoped_ptr<It2MeHostFactory> factory(new MockIt2MeHostFactory());

  scoped_ptr<NativeMessagingChannel> channel(
      new NativeMessagingChannel(input_read_file.Pass(),
                                 output_write_file.Pass()));

  host_.reset(
      new It2MeNativeMessagingHost(
          host_task_runner_, channel.Pass(), factory.Pass()));
  host_->Start(base::Bind(&It2MeNativeMessagingHostTest::StopHost,
                          base::Unretained(this)));

  // Notify the test that the host has finished starting up.
  test_message_loop_->message_loop_proxy()->PostTask(
      FROM_HERE, test_run_loop_->QuitClosure());
}

void It2MeNativeMessagingHostTest::StopHost() {
  DCHECK(host_task_runner_->RunsTasksOnCurrentThread());

  host_.reset();

  // Wait till all shutdown tasks have completed.
  base::RunLoop().RunUntilIdle();

  // Trigger a test shutdown via ExitTest().
  host_task_runner_ = NULL;
}

void It2MeNativeMessagingHostTest::ExitTest() {
  if (!test_message_loop_->message_loop_proxy()->RunsTasksOnCurrentThread()) {
    test_message_loop_->message_loop_proxy()->PostTask(
        FROM_HERE,
        base::Bind(&It2MeNativeMessagingHostTest::ExitTest,
                   base::Unretained(this)));
    return;
  }
  test_run_loop_->Quit();
}

void It2MeNativeMessagingHostTest::TestConnect() {
  base::DictionaryValue connect_message;
  int next_id = 0;

  // Send the "connect" request.
  connect_message.SetInteger("id", ++next_id);
  connect_message.SetString("type", "connect");
  connect_message.SetString("xmppServerAddress", "talk.google.com:5222");
  connect_message.SetBoolean("xmppServerUseTls", true);
  connect_message.SetString("directoryBotJid", "remoting@bot.talk.google.com");
  connect_message.SetString("userName", "chromo.pyauto@gmail.com");
  connect_message.SetString("authServiceWithToken", "oauth2:sometoken");
  WriteMessageToInputPipe(connect_message);

  VerifyConnectResponses(next_id);

  base::DictionaryValue disconnect_message;
  disconnect_message.SetInteger("id", ++next_id);
  disconnect_message.SetString("type", "disconnect");
  WriteMessageToInputPipe(disconnect_message);

  VerifyDisconnectResponses(next_id);
}

// Test hello request.
TEST_F(It2MeNativeMessagingHostTest, Hello) {
  int next_id = 0;
  base::DictionaryValue message;
  message.SetInteger("id", ++next_id);
  message.SetString("type", "hello");
  WriteMessageToInputPipe(message);

  VerifyHelloResponse(next_id);
}

// Verify that response ID matches request ID.
TEST_F(It2MeNativeMessagingHostTest, Id) {
  base::DictionaryValue message;
  message.SetString("type", "hello");
  WriteMessageToInputPipe(message);
  message.SetString("id", "42");
  WriteMessageToInputPipe(message);

  scoped_ptr<base::DictionaryValue> response = ReadMessageFromOutputPipe();
  EXPECT_TRUE(response);
  std::string value;
  EXPECT_FALSE(response->GetString("id", &value));

  response = ReadMessageFromOutputPipe();
  EXPECT_TRUE(response);
  EXPECT_TRUE(response->GetString("id", &value));
  EXPECT_EQ("42", value);
}

TEST_F(It2MeNativeMessagingHostTest, Connect) {
  // A new It2MeHost instance is created for every it2me session. The native
  // messaging host, on the other hand, is long lived. This test verifies
  // multiple It2Me host startup and shutdowns.
  for (int i = 0; i < 3; ++i)
    TestConnect();
}

// Verify non-Dictionary requests are rejected.
TEST_F(It2MeNativeMessagingHostTest, WrongFormat) {
  base::ListValue message;
  // No "error" response will be sent for non-Dictionary messages.
  TestBadRequest(message, false);
}

// Verify requests with no type are rejected.
TEST_F(It2MeNativeMessagingHostTest, MissingType) {
  base::DictionaryValue message;
  TestBadRequest(message, true);
}

// Verify rejection if type is unrecognized.
TEST_F(It2MeNativeMessagingHostTest, InvalidType) {
  base::DictionaryValue message;
  message.SetString("type", "xxx");
  TestBadRequest(message, true);
}

}  // namespace remoting

