// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/it2me/it2me_native_messaging_host.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/stringize_macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "components/policy/core/common/mock_policy_service.h"
#include "net/base/file_stream.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/native_messaging/log_message_handler.h"
#include "remoting/host/native_messaging/native_messaging_pipe.h"
#include "remoting/host/native_messaging/pipe_messaging_channel.h"
#include "remoting/host/policy_watcher.h"
#include "remoting/host/setup/test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

const char kTestAccessCode[] = "888888";
const int kTestAccessCodeLifetimeInSeconds = 666;
const char kTestClientUsername[] = "some_user@gmail.com";

void VerifyId(std::unique_ptr<base::DictionaryValue> response,
              int expected_value) {
  ASSERT_TRUE(response);

  int value;
  EXPECT_TRUE(response->GetInteger("id", &value));
  EXPECT_EQ(expected_value, value);
}

void VerifyStringProperty(std::unique_ptr<base::DictionaryValue> response,
                          const std::string& name,
                          const std::string& expected_value) {
  ASSERT_TRUE(response);

  std::string value;
  EXPECT_TRUE(response->GetString(name, &value));
  EXPECT_EQ(expected_value, value);
}

// Verity the values of the "type" and "id" properties
void VerifyCommonProperties(std::unique_ptr<base::DictionaryValue> response,
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

class FakePolicyService : public policy::PolicyService {
 public:
  FakePolicyService();
  ~FakePolicyService() override;

  // policy::PolicyService overrides.
  void AddObserver(policy::PolicyDomain domain,
                   policy::PolicyService::Observer* observer) override;
  void RemoveObserver(policy::PolicyDomain domain,
                      policy::PolicyService::Observer* observer) override;
  const policy::PolicyMap& GetPolicies(
      const policy::PolicyNamespace& ns) const override;
  bool IsInitializationComplete(policy::PolicyDomain domain) const override;
  void RefreshPolicies(const base::Closure& callback) override;

 private:
  policy::PolicyMap policy_map_;

  DISALLOW_COPY_AND_ASSIGN(FakePolicyService);
};

FakePolicyService::FakePolicyService() {}

FakePolicyService::~FakePolicyService() {}

void FakePolicyService::AddObserver(policy::PolicyDomain domain,
                                    policy::PolicyService::Observer* observer) {
}

void FakePolicyService::RemoveObserver(
    policy::PolicyDomain domain,
    policy::PolicyService::Observer* observer) {}

const policy::PolicyMap& FakePolicyService::GetPolicies(
    const policy::PolicyNamespace& ns) const {
  return policy_map_;
}

bool FakePolicyService::IsInitializationComplete(
    policy::PolicyDomain domain) const {
  return true;
}

void FakePolicyService::RefreshPolicies(const base::Closure& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, callback);
}

class MockIt2MeHost : public It2MeHost {
 public:
  MockIt2MeHost(std::unique_ptr<ChromotingHostContext> context,
                std::unique_ptr<PolicyWatcher> policy_watcher,
                base::WeakPtr<It2MeHost::Observer> observer,
                std::unique_ptr<SignalStrategy> signal_strategy,
                const std::string& username,
                const std::string& directory_bot_jid)
      : It2MeHost(std::move(context),
                  std::move(policy_watcher),
                  /*confirmation_dialog_factory=*/nullptr,
                  observer,
                  std::move(signal_strategy),
                  username,
                  directory_bot_jid) {}

  // It2MeHost overrides
  void Connect() override;
  void Disconnect() override;
  void RequestNatPolicy() override;

 private:
  ~MockIt2MeHost() override {}

  void RunSetState(It2MeHostState state);

  DISALLOW_COPY_AND_ASSIGN(MockIt2MeHost);
};

void MockIt2MeHost::Connect() {
  if (!host_context()->ui_task_runner()->BelongsToCurrentThread()) {
    host_context()->ui_task_runner()->PostTask(
        FROM_HERE, base::Bind(&MockIt2MeHost::Connect, this));
    return;
  }

  RunSetState(kStarting);
  RunSetState(kRequestedAccessCode);

  std::string access_code(kTestAccessCode);
  base::TimeDelta lifetime =
      base::TimeDelta::FromSeconds(kTestAccessCodeLifetimeInSeconds);
  host_context()->ui_task_runner()->PostTask(
      FROM_HERE, base::Bind(&It2MeHost::Observer::OnStoreAccessCode, observer(),
                            access_code, lifetime));

  RunSetState(kReceivedAccessCode);
  RunSetState(kConnecting);

  std::string client_username(kTestClientUsername);
  host_context()->ui_task_runner()->PostTask(
      FROM_HERE, base::Bind(&It2MeHost::Observer::OnClientAuthenticated,
                            observer(), client_username));

  RunSetState(kConnected);
}

void MockIt2MeHost::Disconnect() {
  if (!host_context()->network_task_runner()->BelongsToCurrentThread()) {
    DCHECK(host_context()->ui_task_runner()->BelongsToCurrentThread());
    host_context()->network_task_runner()->PostTask(
        FROM_HERE, base::Bind(&MockIt2MeHost::Disconnect, this));
    return;
  }

  RunSetState(kDisconnected);
}

void MockIt2MeHost::RequestNatPolicy() {}

void MockIt2MeHost::RunSetState(It2MeHostState state) {
  if (!host_context()->network_task_runner()->BelongsToCurrentThread()) {
    host_context()->network_task_runner()->PostTask(
        FROM_HERE, base::Bind(&It2MeHost::SetStateForTesting, this, state, ""));
  } else {
    SetStateForTesting(state, "");
  }
}

class MockIt2MeHostFactory : public It2MeHostFactory {
 public:
  MockIt2MeHostFactory();
  ~MockIt2MeHostFactory() override;

  scoped_refptr<It2MeHost> CreateIt2MeHost(
      std::unique_ptr<ChromotingHostContext> context,
      policy::PolicyService* policy_service,
      base::WeakPtr<It2MeHost::Observer> observer,
      std::unique_ptr<SignalStrategy> signal_strategy,
      const std::string& username,
      const std::string& directory_bot_jid) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockIt2MeHostFactory);
};

MockIt2MeHostFactory::MockIt2MeHostFactory() : It2MeHostFactory() {}

MockIt2MeHostFactory::~MockIt2MeHostFactory() {}

scoped_refptr<It2MeHost> MockIt2MeHostFactory::CreateIt2MeHost(
    std::unique_ptr<ChromotingHostContext> context,
    policy::PolicyService* policy_service,
    base::WeakPtr<It2MeHost::Observer> observer,
    std::unique_ptr<SignalStrategy> signal_strategy,
    const std::string& username,
    const std::string& directory_bot_jid) {
  return new MockIt2MeHost(std::move(context),
                           /*policy_watcher=*/nullptr, observer,
                           std::move(signal_strategy), username,
                           directory_bot_jid);
}

}  // namespace

class It2MeNativeMessagingHostTest : public testing::Test {
 public:
  It2MeNativeMessagingHostTest() {}
  ~It2MeNativeMessagingHostTest() override {}

  void SetUp() override;
  void TearDown() override;

 protected:
  std::unique_ptr<base::DictionaryValue> ReadMessageFromOutputPipe();
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
  std::unique_ptr<base::MessageLoop> test_message_loop_;
  std::unique_ptr<base::RunLoop> test_run_loop_;

  std::unique_ptr<base::Thread> host_thread_;
  std::unique_ptr<base::RunLoop> host_run_loop_;

  // Task runner of the host thread.
  scoped_refptr<AutoThreadTaskRunner> host_task_runner_;
  std::unique_ptr<remoting::NativeMessagingPipe> pipe_;

  std::unique_ptr<policy::PolicyService> fake_policy_service_;

  DISALLOW_COPY_AND_ASSIGN(It2MeNativeMessagingHostTest);
};

void It2MeNativeMessagingHostTest::SetUp() {
  test_message_loop_.reset(new base::MessageLoop());
  test_run_loop_.reset(new base::RunLoop());

#if defined(OS_CHROMEOS)
  // On Chrome OS, the browser owns the PolicyService so simulate that here.
  fake_policy_service_.reset(new FakePolicyService());
#endif  // defined(OS_CHROMEOS)

  // Run the host on a dedicated thread.
  host_thread_.reset(new base::Thread("host_thread"));
  host_thread_->Start();

  host_task_runner_ = new AutoThreadTaskRunner(
      host_thread_->task_runner(),
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
  // Release reference to AutoThreadTaskRunner, so the host thread can be shut
  // down.
  host_task_runner_ = nullptr;

  // Closing the write-end of the input will send an EOF to the native
  // messaging reader. This will trigger a host shutdown.
  input_write_file_.Close();

  // Start a new RunLoop and Wait until the host finishes shutting down.
  test_run_loop_.reset(new base::RunLoop());
  test_run_loop_->Run();

  // Verify there are no more message in the output pipe.
  std::unique_ptr<base::DictionaryValue> response = ReadMessageFromOutputPipe();
  EXPECT_FALSE(response);

  // The It2MeNativeMessagingHost dtor closes the handles that are passed to it.
  // So the only handle left to close is |output_read_file_|.
  output_read_file_.Close();
}

std::unique_ptr<base::DictionaryValue>
It2MeNativeMessagingHostTest::ReadMessageFromOutputPipe() {
  while (true) {
    uint32_t length;
    int read_result = output_read_file_.ReadAtCurrentPos(
        reinterpret_cast<char*>(&length), sizeof(length));
    if (read_result != sizeof(length)) {
      // The output pipe has been closed, return an empty message.
      return nullptr;
    }

    std::string message_json(length, '\0');
    read_result = output_read_file_.ReadAtCurrentPos(
        base::string_as_array(&message_json), length);
    if (read_result != static_cast<int>(length)) {
      LOG(ERROR) << "Message size (" << read_result
                 << ") doesn't match the header (" << length << ").";
      return nullptr;
    }

    std::unique_ptr<base::Value> message = base::JSONReader::Read(message_json);
    if (!message || !message->IsType(base::Value::Type::DICTIONARY)) {
      LOG(ERROR) << "Malformed message:" << message_json;
      return nullptr;
    }

    std::unique_ptr<base::DictionaryValue> result = base::WrapUnique(
        static_cast<base::DictionaryValue*>(message.release()));
    std::string type;
    // If this is a debug message log, ignore it, otherwise return it.
    if (!result->GetString("type", &type) ||
        type != LogMessageHandler::kDebugMessageTypeName) {
      return result;
    }
  }
}

void It2MeNativeMessagingHostTest::WriteMessageToInputPipe(
    const base::Value& message) {
  std::string message_json;
  base::JSONWriter::Write(message, &message_json);

  uint32_t length = message_json.length();
  input_write_file_.WriteAtCurrentPos(reinterpret_cast<char*>(&length),
                                      sizeof(length));
  input_write_file_.WriteAtCurrentPos(message_json.data(), length);
}

void It2MeNativeMessagingHostTest::VerifyHelloResponse(int request_id) {
  std::unique_ptr<base::DictionaryValue> response = ReadMessageFromOutputPipe();
  VerifyCommonProperties(std::move(response), "helloResponse", request_id);
}

void It2MeNativeMessagingHostTest::VerifyErrorResponse() {
  std::unique_ptr<base::DictionaryValue> response = ReadMessageFromOutputPipe();
  VerifyStringProperty(std::move(response), "type", "error");
}

void It2MeNativeMessagingHostTest::VerifyConnectResponses(int request_id) {
  bool connect_response_received = false;
  bool starting_received = false;
  bool requestedAccessCode_received = false;
  bool receivedAccessCode_received = false;
  bool connecting_received = false;
  bool connected_received = false;

  // We expect a total of 6 messages: 1 connectResponse and 5 hostStateChanged.
  for (int i = 0; i < 6; ++i) {
    std::unique_ptr<base::DictionaryValue> response =
        ReadMessageFromOutputPipe();
    ASSERT_TRUE(response);

    std::string type;
    ASSERT_TRUE(response->GetString("type", &type));

    if (type == "connectResponse") {
      EXPECT_FALSE(connect_response_received);
      connect_response_received = true;
      VerifyId(std::move(response), request_id);
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
                 It2MeNativeMessagingHost::HostStateToString(kConnecting)) {
        EXPECT_FALSE(connecting_received);
        connecting_received = true;
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
  bool disconnected_received = false;

  // We expect a total of 3 messages: 1 connectResponse and 1 hostStateChanged.
  for (int i = 0; i < 2; ++i) {
    std::unique_ptr<base::DictionaryValue> response =
        ReadMessageFromOutputPipe();
    ASSERT_TRUE(response);

    std::string type;
    ASSERT_TRUE(response->GetString("type", &type));

    if (type == "disconnectResponse") {
      EXPECT_FALSE(disconnect_response_received);
      disconnect_response_received = true;
      VerifyId(std::move(response), request_id);
    } else if (type == "hostStateChanged") {
      std::string state;
      ASSERT_TRUE(response->GetString("state", &state));
      if (state == It2MeNativeMessagingHost::HostStateToString(kDisconnected)) {
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

  if (expect_error_response) {
    VerifyErrorResponse();
  }

  std::unique_ptr<base::DictionaryValue> response = ReadMessageFromOutputPipe();
  EXPECT_FALSE(response);
}

void It2MeNativeMessagingHostTest::StartHost() {
  DCHECK(host_task_runner_->RunsTasksOnCurrentThread());

  base::File input_read_file;
  base::File output_write_file;

  ASSERT_TRUE(MakePipe(&input_read_file, &input_write_file_));
  ASSERT_TRUE(MakePipe(&output_read_file_, &output_write_file));

  pipe_.reset(new NativeMessagingPipe());

  std::unique_ptr<extensions::NativeMessagingChannel> channel(
      new PipeMessagingChannel(std::move(input_read_file),
                               std::move(output_write_file)));

  // Creating a native messaging host with a mock It2MeHostFactory.
  std::unique_ptr<extensions::NativeMessageHost> it2me_host(
      new It2MeNativeMessagingHost(
          /*needs_elevation=*/false, fake_policy_service_.get(),
          ChromotingHostContext::Create(host_task_runner_),
          base::WrapUnique(new MockIt2MeHostFactory())));
  it2me_host->Start(pipe_.get());

  pipe_->Start(std::move(it2me_host), std::move(channel));

  // Notify the test that the host has finished starting up.
  test_message_loop_->task_runner()->PostTask(
      FROM_HERE, test_run_loop_->QuitClosure());
}

void It2MeNativeMessagingHostTest::ExitTest() {
  if (!test_message_loop_->task_runner()->RunsTasksOnCurrentThread()) {
    test_message_loop_->task_runner()->PostTask(
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

  std::unique_ptr<base::DictionaryValue> response = ReadMessageFromOutputPipe();
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
  for (int i = 0; i < 3; ++i) {
    TestConnect();
  }
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
