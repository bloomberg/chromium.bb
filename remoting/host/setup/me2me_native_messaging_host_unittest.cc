// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/setup/me2me_native_messaging_host.h"

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/stringize_macros.h"
#include "base/values.h"
#include "google_apis/gaia/gaia_oauth_client.h"
#include "net/base/file_stream.h"
#include "net/base/net_util.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/host/native_messaging/native_messaging_channel.h"
#include "remoting/host/pin_hash.h"
#include "remoting/host/setup/test_util.h"
#include "remoting/protocol/pairing_registry.h"
#include "remoting/protocol/protocol_mock_objects.h"
#include "testing/gtest/include/gtest/gtest.h"

using remoting::protocol::MockPairingRegistryDelegate;
using remoting::protocol::PairingRegistry;
using remoting::protocol::SynchronousPairingRegistry;

namespace {

void VerifyHelloResponse(scoped_ptr<base::DictionaryValue> response) {
  ASSERT_TRUE(response);
  std::string value;
  EXPECT_TRUE(response->GetString("type", &value));
  EXPECT_EQ("helloResponse", value);
  EXPECT_TRUE(response->GetString("version", &value));
  EXPECT_EQ(STRINGIZE(VERSION), value);
}

void VerifyGetHostNameResponse(scoped_ptr<base::DictionaryValue> response) {
  ASSERT_TRUE(response);
  std::string value;
  EXPECT_TRUE(response->GetString("type", &value));
  EXPECT_EQ("getHostNameResponse", value);
  EXPECT_TRUE(response->GetString("hostname", &value));
  EXPECT_EQ(net::GetHostName(), value);
}

void VerifyGetPinHashResponse(scoped_ptr<base::DictionaryValue> response) {
  ASSERT_TRUE(response);
  std::string value;
  EXPECT_TRUE(response->GetString("type", &value));
  EXPECT_EQ("getPinHashResponse", value);
  EXPECT_TRUE(response->GetString("hash", &value));
  EXPECT_EQ(remoting::MakeHostPinHash("my_host", "1234"), value);
}

void VerifyGenerateKeyPairResponse(scoped_ptr<base::DictionaryValue> response) {
  ASSERT_TRUE(response);
  std::string value;
  EXPECT_TRUE(response->GetString("type", &value));
  EXPECT_EQ("generateKeyPairResponse", value);
  EXPECT_TRUE(response->GetString("privateKey", &value));
  EXPECT_TRUE(response->GetString("publicKey", &value));
}

void VerifyGetDaemonConfigResponse(scoped_ptr<base::DictionaryValue> response) {
  ASSERT_TRUE(response);
  std::string value;
  EXPECT_TRUE(response->GetString("type", &value));
  EXPECT_EQ("getDaemonConfigResponse", value);
  const base::DictionaryValue* config = NULL;
  EXPECT_TRUE(response->GetDictionary("config", &config));
  EXPECT_TRUE(base::DictionaryValue().Equals(config));
}

void VerifyGetUsageStatsConsentResponse(
    scoped_ptr<base::DictionaryValue> response) {
  ASSERT_TRUE(response);
  std::string value;
  EXPECT_TRUE(response->GetString("type", &value));
  EXPECT_EQ("getUsageStatsConsentResponse", value);
  bool supported, allowed, set_by_policy;
  EXPECT_TRUE(response->GetBoolean("supported", &supported));
  EXPECT_TRUE(response->GetBoolean("allowed", &allowed));
  EXPECT_TRUE(response->GetBoolean("setByPolicy", &set_by_policy));
  EXPECT_TRUE(supported);
  EXPECT_TRUE(allowed);
  EXPECT_TRUE(set_by_policy);
}

void VerifyStopDaemonResponse(scoped_ptr<base::DictionaryValue> response) {
  ASSERT_TRUE(response);
  std::string value;
  EXPECT_TRUE(response->GetString("type", &value));
  EXPECT_EQ("stopDaemonResponse", value);
  EXPECT_TRUE(response->GetString("result", &value));
  EXPECT_EQ("OK", value);
}

void VerifyGetDaemonStateResponse(scoped_ptr<base::DictionaryValue> response) {
  ASSERT_TRUE(response);
  std::string value;
  EXPECT_TRUE(response->GetString("type", &value));
  EXPECT_EQ("getDaemonStateResponse", value);
  EXPECT_TRUE(response->GetString("state", &value));
  EXPECT_EQ("STARTED", value);
}

void VerifyUpdateDaemonConfigResponse(
    scoped_ptr<base::DictionaryValue> response) {
  ASSERT_TRUE(response);
  std::string value;
  EXPECT_TRUE(response->GetString("type", &value));
  EXPECT_EQ("updateDaemonConfigResponse", value);
  EXPECT_TRUE(response->GetString("result", &value));
  EXPECT_EQ("OK", value);
}

void VerifyStartDaemonResponse(scoped_ptr<base::DictionaryValue> response) {
  ASSERT_TRUE(response);
  std::string value;
  EXPECT_TRUE(response->GetString("type", &value));
  EXPECT_EQ("startDaemonResponse", value);
  EXPECT_TRUE(response->GetString("result", &value));
  EXPECT_EQ("OK", value);
}

}  // namespace

namespace remoting {

class MockDaemonControllerDelegate : public DaemonController::Delegate {
 public:
  MockDaemonControllerDelegate();
  virtual ~MockDaemonControllerDelegate();

  // DaemonController::Delegate interface.
  virtual DaemonController::State GetState() OVERRIDE;
  virtual scoped_ptr<base::DictionaryValue> GetConfig() OVERRIDE;
  virtual void SetConfigAndStart(
      scoped_ptr<base::DictionaryValue> config,
      bool consent,
      const DaemonController::CompletionCallback& done) OVERRIDE;
  virtual void UpdateConfig(
      scoped_ptr<base::DictionaryValue> config,
      const DaemonController::CompletionCallback& done) OVERRIDE;
  virtual void Stop(const DaemonController::CompletionCallback& done) OVERRIDE;
  virtual void SetWindow(void* window_handle) OVERRIDE;
  virtual std::string GetVersion() OVERRIDE;
  virtual DaemonController::UsageStatsConsent GetUsageStatsConsent() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDaemonControllerDelegate);
};

MockDaemonControllerDelegate::MockDaemonControllerDelegate() {}

MockDaemonControllerDelegate::~MockDaemonControllerDelegate() {}

DaemonController::State MockDaemonControllerDelegate::GetState() {
  return DaemonController::STATE_STARTED;
}

scoped_ptr<base::DictionaryValue> MockDaemonControllerDelegate::GetConfig() {
  return scoped_ptr<base::DictionaryValue>(new base::DictionaryValue());
}

void MockDaemonControllerDelegate::SetConfigAndStart(
    scoped_ptr<base::DictionaryValue> config,
    bool consent,
    const DaemonController::CompletionCallback& done) {

  // Verify parameters passed in.
  if (consent && config && config->HasKey("start")) {
    done.Run(DaemonController::RESULT_OK);
  } else {
    done.Run(DaemonController::RESULT_FAILED);
  }
}

void MockDaemonControllerDelegate::UpdateConfig(
    scoped_ptr<base::DictionaryValue> config,
    const DaemonController::CompletionCallback& done) {
  if (config && config->HasKey("update")) {
    done.Run(DaemonController::RESULT_OK);
  } else {
    done.Run(DaemonController::RESULT_FAILED);
  }
}

void MockDaemonControllerDelegate::Stop(
    const DaemonController::CompletionCallback& done) {
  done.Run(DaemonController::RESULT_OK);
}

void MockDaemonControllerDelegate::SetWindow(void* window_handle) {}

std::string MockDaemonControllerDelegate::GetVersion() {
  // Unused - NativeMessagingHost returns the compiled-in version string
  // instead of calling this method.
  NOTREACHED();
  return std::string();
}

DaemonController::UsageStatsConsent
MockDaemonControllerDelegate::GetUsageStatsConsent() {
  DaemonController::UsageStatsConsent consent;
  consent.supported = true;
  consent.allowed = true;
  consent.set_by_policy = true;
  return consent;
}

class NativeMessagingHostTest : public testing::Test {
 public:
  NativeMessagingHostTest();
  virtual ~NativeMessagingHostTest();

  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  void Run();

  // Deletes |host_|.
  void DeleteHost();

  scoped_ptr<base::DictionaryValue> ReadMessageFromOutputPipe();

  void WriteMessageToInputPipe(const base::Value& message);

  // The Host process should shut down when it receives a malformed request.
  // This is tested by sending a known-good request, followed by |message|,
  // followed by the known-good request again. The response file should only
  // contain a single response from the first good request.
  void TestBadRequest(const base::Value& message);

 protected:
  // Reference to the MockDaemonControllerDelegate, which is owned by
  // |channel_|.
  MockDaemonControllerDelegate* daemon_controller_delegate_;

 private:
  // Each test creates two unidirectional pipes: "input" and "output".
  // NativeMessagingHost reads from input_read_handle and writes to
  // output_write_handle. The unittest supplies data to input_write_handle, and
  // verifies output from output_read_handle.
  //
  // unittest -> [input] -> NativeMessagingHost -> [output] -> unittest
  base::PlatformFile input_write_handle_;
  base::PlatformFile output_read_handle_;

  base::MessageLoop message_loop_;
  base::RunLoop run_loop_;
  scoped_refptr<AutoThreadTaskRunner> task_runner_;
  scoped_ptr<remoting::NativeMessagingChannel> channel_;

  DISALLOW_COPY_AND_ASSIGN(NativeMessagingHostTest);
};

NativeMessagingHostTest::NativeMessagingHostTest()
    : message_loop_(base::MessageLoop::TYPE_IO) {}

NativeMessagingHostTest::~NativeMessagingHostTest() {}

void NativeMessagingHostTest::SetUp() {
  base::PlatformFile input_read_handle;
  base::PlatformFile output_write_handle;

  ASSERT_TRUE(MakePipe(&input_read_handle, &input_write_handle_));
  ASSERT_TRUE(MakePipe(&output_read_handle_, &output_write_handle));

  // Arrange to run |message_loop_| until no components depend on it.
  task_runner_ = new AutoThreadTaskRunner(
      message_loop_.message_loop_proxy(), run_loop_.QuitClosure());

  daemon_controller_delegate_ = new MockDaemonControllerDelegate();
  scoped_refptr<DaemonController> daemon_controller(
      new DaemonController(
          scoped_ptr<DaemonController::Delegate>(daemon_controller_delegate_)));

  scoped_refptr<PairingRegistry> pairing_registry =
      new SynchronousPairingRegistry(scoped_ptr<PairingRegistry::Delegate>(
          new MockPairingRegistryDelegate()));
  scoped_ptr<NativeMessagingChannel::Delegate> host(
      new NativeMessagingHost(daemon_controller,
                              pairing_registry,
                              scoped_ptr<remoting::OAuthClient>()));
  channel_.reset(
      new NativeMessagingChannel(host.Pass(),
                                 input_read_handle,
                                 output_write_handle));
}

void NativeMessagingHostTest::TearDown() {
  // DaemonController destroys its internals asynchronously. Let these and any
  // other pending tasks run to make sure we don't leak the memory owned by
  // them.
  message_loop_.RunUntilIdle();

  // The NativeMessagingHost dtor closes the handles that are passed to it.
  // |input_write_handle_| gets closed just before starting the host. So the
  // only handle left to close is |output_read_handle_|.
  base::ClosePlatformFile(output_read_handle_);
}

void NativeMessagingHostTest::Run() {
  // Close the write-end of input, so that the host sees EOF after reading
  // messages and won't block waiting for more input.
  base::ClosePlatformFile(input_write_handle_);
  channel_->Start(base::Bind(&NativeMessagingHostTest::DeleteHost,
                             base::Unretained(this)));
  run_loop_.Run();
}

void NativeMessagingHostTest::DeleteHost() {
  // Destroy |channel_| so that it closes its end of the output pipe, so that
  // TestBadRequest() will see EOF and won't block waiting for more data.
  channel_.reset();
  task_runner_ = NULL;
}

scoped_ptr<base::DictionaryValue>
NativeMessagingHostTest::ReadMessageFromOutputPipe() {
  uint32 length;
  int read_result = base::ReadPlatformFileAtCurrentPos(
      output_read_handle_, reinterpret_cast<char*>(&length), sizeof(length));
  if (read_result != sizeof(length)) {
    return scoped_ptr<base::DictionaryValue>();
  }

  std::string message_json(length, '\0');
  read_result = base::ReadPlatformFileAtCurrentPos(
      output_read_handle_, string_as_array(&message_json), length);
  if (read_result != static_cast<int>(length)) {
    return scoped_ptr<base::DictionaryValue>();
  }

  scoped_ptr<base::Value> message(base::JSONReader::Read(message_json));
  if (!message || !message->IsType(base::Value::TYPE_DICTIONARY)) {
    return scoped_ptr<base::DictionaryValue>();
  }

  return scoped_ptr<base::DictionaryValue>(
      static_cast<base::DictionaryValue*>(message.release()));
}

void NativeMessagingHostTest::WriteMessageToInputPipe(
    const base::Value& message) {
  std::string message_json;
  base::JSONWriter::Write(&message, &message_json);

  uint32 length = message_json.length();
  base::WritePlatformFileAtCurrentPos(input_write_handle_,
                                      reinterpret_cast<char*>(&length),
                                      sizeof(length));
  base::WritePlatformFileAtCurrentPos(input_write_handle_, message_json.data(),
                                      length);
}

void NativeMessagingHostTest::TestBadRequest(const base::Value& message) {
  base::DictionaryValue good_message;
  good_message.SetString("type", "hello");

  // This test currently relies on synchronous processing of hello messages and
  // message parameters verification.
  WriteMessageToInputPipe(good_message);
  WriteMessageToInputPipe(message);
  WriteMessageToInputPipe(good_message);

  Run();

  // Read from output pipe, and verify responses.
  scoped_ptr<base::DictionaryValue> response =
      ReadMessageFromOutputPipe();
  VerifyHelloResponse(response.Pass());

  response = ReadMessageFromOutputPipe();
  EXPECT_FALSE(response);
}

// Test all valid request-types.
TEST_F(NativeMessagingHostTest, All) {
  int next_id = 0;
  base::DictionaryValue message;
  message.SetInteger("id", next_id++);
  message.SetString("type", "hello");
  WriteMessageToInputPipe(message);

  message.SetInteger("id", next_id++);
  message.SetString("type", "getHostName");
  WriteMessageToInputPipe(message);

  message.SetInteger("id", next_id++);
  message.SetString("type", "getPinHash");
  message.SetString("hostId", "my_host");
  message.SetString("pin", "1234");
  WriteMessageToInputPipe(message);

  message.Clear();
  message.SetInteger("id", next_id++);
  message.SetString("type", "generateKeyPair");
  WriteMessageToInputPipe(message);

  message.SetInteger("id", next_id++);
  message.SetString("type", "getDaemonConfig");
  WriteMessageToInputPipe(message);

  message.SetInteger("id", next_id++);
  message.SetString("type", "getUsageStatsConsent");
  WriteMessageToInputPipe(message);

  message.SetInteger("id", next_id++);
  message.SetString("type", "stopDaemon");
  WriteMessageToInputPipe(message);

  message.SetInteger("id", next_id++);
  message.SetString("type", "getDaemonState");
  WriteMessageToInputPipe(message);

  // Following messages require a "config" dictionary.
  base::DictionaryValue config;
  config.SetBoolean("update", true);
  message.Set("config", config.DeepCopy());
  message.SetInteger("id", next_id++);
  message.SetString("type", "updateDaemonConfig");
  WriteMessageToInputPipe(message);

  config.Clear();
  config.SetBoolean("start", true);
  message.Set("config", config.DeepCopy());
  message.SetBoolean("consent", true);
  message.SetInteger("id", next_id++);
  message.SetString("type", "startDaemon");
  WriteMessageToInputPipe(message);

  Run();

  void (*verify_routines[])(scoped_ptr<base::DictionaryValue>) = {
    &VerifyHelloResponse,
    &VerifyGetHostNameResponse,
    &VerifyGetPinHashResponse,
    &VerifyGenerateKeyPairResponse,
    &VerifyGetDaemonConfigResponse,
    &VerifyGetUsageStatsConsentResponse,
    &VerifyStopDaemonResponse,
    &VerifyGetDaemonStateResponse,
    &VerifyUpdateDaemonConfigResponse,
    &VerifyStartDaemonResponse,
  };
  ASSERT_EQ(arraysize(verify_routines), static_cast<size_t>(next_id));

  // Read all responses from output pipe, and verify them.
  for (int i = 0; i < next_id; ++i) {
    scoped_ptr<base::DictionaryValue> response = ReadMessageFromOutputPipe();

    // Make sure that id is available and is in the range.
    int id;
    ASSERT_TRUE(response->GetInteger("id", &id));
    ASSERT_TRUE(0 <= id && id < next_id);

    // Call the verification routine corresponding to the message id.
    ASSERT_TRUE(verify_routines[id]);
    verify_routines[id](response.Pass());

    // Clear the pointer so that the routine cannot be called the second time.
    verify_routines[id] = NULL;
  }
}

// Verify that response ID matches request ID.
TEST_F(NativeMessagingHostTest, Id) {
  base::DictionaryValue message;
  message.SetString("type", "hello");
  WriteMessageToInputPipe(message);
  message.SetString("id", "42");
  WriteMessageToInputPipe(message);

  Run();

  scoped_ptr<base::DictionaryValue> response =
      ReadMessageFromOutputPipe();
  EXPECT_TRUE(response);
  std::string value;
  EXPECT_FALSE(response->GetString("id", &value));

  response = ReadMessageFromOutputPipe();
  EXPECT_TRUE(response);
  EXPECT_TRUE(response->GetString("id", &value));
  EXPECT_EQ("42", value);
}

// Verify non-Dictionary requests are rejected.
TEST_F(NativeMessagingHostTest, WrongFormat) {
  base::ListValue message;
  TestBadRequest(message);
}

// Verify requests with no type are rejected.
TEST_F(NativeMessagingHostTest, MissingType) {
  base::DictionaryValue message;
  TestBadRequest(message);
}

// Verify rejection if type is unrecognized.
TEST_F(NativeMessagingHostTest, InvalidType) {
  base::DictionaryValue message;
  message.SetString("type", "xxx");
  TestBadRequest(message);
}

// Verify rejection if getPinHash request has no hostId.
TEST_F(NativeMessagingHostTest, GetPinHashNoHostId) {
  base::DictionaryValue message;
  message.SetString("type", "getPinHash");
  message.SetString("pin", "1234");
  TestBadRequest(message);
}

// Verify rejection if getPinHash request has no pin.
TEST_F(NativeMessagingHostTest, GetPinHashNoPin) {
  base::DictionaryValue message;
  message.SetString("type", "getPinHash");
  message.SetString("hostId", "my_host");
  TestBadRequest(message);
}

// Verify rejection if updateDaemonConfig request has invalid config.
TEST_F(NativeMessagingHostTest, UpdateDaemonConfigInvalidConfig) {
  base::DictionaryValue message;
  message.SetString("type", "updateDaemonConfig");
  message.SetString("config", "xxx");
  TestBadRequest(message);
}

// Verify rejection if startDaemon request has invalid config.
TEST_F(NativeMessagingHostTest, StartDaemonInvalidConfig) {
  base::DictionaryValue message;
  message.SetString("type", "startDaemon");
  message.SetString("config", "xxx");
  message.SetBoolean("consent", true);
  TestBadRequest(message);
}

// Verify rejection if startDaemon request has no "consent" parameter.
TEST_F(NativeMessagingHostTest, StartDaemonNoConsent) {
  base::DictionaryValue message;
  message.SetString("type", "startDaemon");
  message.Set("config", base::DictionaryValue().DeepCopy());
  TestBadRequest(message);
}

}  // namespace remoting
