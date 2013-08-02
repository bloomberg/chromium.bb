// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/setup/native_messaging_host.h"

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
#include "remoting/host/pin_hash.h"
#include "remoting/host/setup/test_util.h"
#include "remoting/protocol/pairing_registry.h"
#include "remoting/protocol/protocol_mock_objects.h"
#include "testing/gtest/include/gtest/gtest.h"

using remoting::protocol::MockPairingRegistryDelegate;
using remoting::protocol::PairingRegistry;
using remoting::protocol::SynchronousPairingRegistry;

namespace {

void VerifyHelloResponse(const base::DictionaryValue* response) {
  ASSERT_TRUE(response);
  std::string value;
  EXPECT_TRUE(response->GetString("type", &value));
  EXPECT_EQ("helloResponse", value);
  EXPECT_TRUE(response->GetString("version", &value));
  EXPECT_EQ(STRINGIZE(VERSION), value);
}

void VerifyGetHostNameResponse(const base::DictionaryValue* response) {
  ASSERT_TRUE(response);
  std::string value;
  EXPECT_TRUE(response->GetString("type", &value));
  EXPECT_EQ("getHostNameResponse", value);
  EXPECT_TRUE(response->GetString("hostname", &value));
  EXPECT_EQ(net::GetHostName(), value);
}

void VerifyGetPinHashResponse(const base::DictionaryValue* response) {
  ASSERT_TRUE(response);
  std::string value;
  EXPECT_TRUE(response->GetString("type", &value));
  EXPECT_EQ("getPinHashResponse", value);
  EXPECT_TRUE(response->GetString("hash", &value));
  EXPECT_EQ(remoting::MakeHostPinHash("my_host", "1234"), value);
}

void VerifyGenerateKeyPairResponse(const base::DictionaryValue* response) {
  ASSERT_TRUE(response);
  std::string value;
  EXPECT_TRUE(response->GetString("type", &value));
  EXPECT_EQ("generateKeyPairResponse", value);
  EXPECT_TRUE(response->GetString("privateKey", &value));
  EXPECT_TRUE(response->GetString("publicKey", &value));
}

void VerifyGetDaemonConfigResponse(const base::DictionaryValue* response) {
  ASSERT_TRUE(response);
  std::string value;
  EXPECT_TRUE(response->GetString("type", &value));
  EXPECT_EQ("getDaemonConfigResponse", value);
  const base::DictionaryValue* config = NULL;
  EXPECT_TRUE(response->GetDictionary("config", &config));
  EXPECT_TRUE(base::DictionaryValue().Equals(config));
}

void VerifyGetUsageStatsConsentResponse(const base::DictionaryValue* response) {
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

void VerifyStopDaemonResponse(const base::DictionaryValue* response) {
  ASSERT_TRUE(response);
  std::string value;
  EXPECT_TRUE(response->GetString("type", &value));
  EXPECT_EQ("stopDaemonResponse", value);
  EXPECT_TRUE(response->GetString("result", &value));
  EXPECT_EQ("OK", value);
}

void VerifyGetDaemonStateResponse(const base::DictionaryValue* response) {
  ASSERT_TRUE(response);
  std::string value;
  EXPECT_TRUE(response->GetString("type", &value));
  EXPECT_EQ("getDaemonStateResponse", value);
  EXPECT_TRUE(response->GetString("state", &value));
  EXPECT_EQ("STARTED", value);
}

void VerifyUpdateDaemonConfigResponse(const base::DictionaryValue* response) {
  ASSERT_TRUE(response);
  std::string value;
  EXPECT_TRUE(response->GetString("type", &value));
  EXPECT_EQ("updateDaemonConfigResponse", value);
  EXPECT_TRUE(response->GetString("result", &value));
  EXPECT_EQ("OK", value);
}

void VerifyStartDaemonResponse(const base::DictionaryValue* response) {
  ASSERT_TRUE(response);
  std::string value;
  EXPECT_TRUE(response->GetString("type", &value));
  EXPECT_EQ("startDaemonResponse", value);
  EXPECT_TRUE(response->GetString("result", &value));
  EXPECT_EQ("OK", value);
}

}  // namespace

namespace remoting {

class MockDaemonController : public DaemonController {
 public:
  MockDaemonController();
  virtual ~MockDaemonController();

  virtual State GetState() OVERRIDE;
  virtual void GetConfig(const GetConfigCallback& callback) OVERRIDE;
  virtual void SetConfigAndStart(scoped_ptr<base::DictionaryValue> config,
                                 bool consent,
                                 const CompletionCallback& callback) OVERRIDE;
  virtual void UpdateConfig(scoped_ptr<base::DictionaryValue> config,
                            const CompletionCallback& callback) OVERRIDE;
  virtual void Stop(const CompletionCallback& callback) OVERRIDE;
  virtual void SetWindow(void* window_handle) OVERRIDE;
  virtual void GetVersion(const GetVersionCallback& callback) OVERRIDE;
  virtual void GetUsageStatsConsent(
      const GetUsageStatsConsentCallback& callback) OVERRIDE;

  // Returns a record of functions called, so that unittests can verify these
  // were called in the proper sequence.
  std::string call_log() { return call_log_; }

 private:
  std::string call_log_;

  DISALLOW_COPY_AND_ASSIGN(MockDaemonController);
};

MockDaemonController::MockDaemonController() {}

MockDaemonController::~MockDaemonController() {}

DaemonController::State MockDaemonController::GetState() {
  call_log_ += "GetState:";
  return DaemonController::STATE_STARTED;
}

void MockDaemonController::GetConfig(const GetConfigCallback& callback) {
  call_log_ += "GetConfig:";
  scoped_ptr<base::DictionaryValue> config(new base::DictionaryValue());
  callback.Run(config.Pass());
}

void MockDaemonController::SetConfigAndStart(
    scoped_ptr<base::DictionaryValue> config, bool consent,
    const CompletionCallback& callback) {
  call_log_ += "SetConfigAndStart:";

  // Verify parameters passed in.
  if (consent && config && config->HasKey("start")) {
    callback.Run(DaemonController::RESULT_OK);
  } else {
    callback.Run(DaemonController::RESULT_FAILED);
  }
}

void MockDaemonController::UpdateConfig(
    scoped_ptr<base::DictionaryValue> config,
    const CompletionCallback& callback) {
  call_log_ += "UpdateConfig:";
  if (config && config->HasKey("update")) {
    callback.Run(DaemonController::RESULT_OK);
  } else {
    callback.Run(DaemonController::RESULT_FAILED);
  }
}

void MockDaemonController::Stop(const CompletionCallback& callback) {
  call_log_ += "Stop:";
  callback.Run(DaemonController::RESULT_OK);
}

void MockDaemonController::SetWindow(void* window_handle) {}

void MockDaemonController::GetVersion(const GetVersionCallback& callback) {
  // Unused - NativeMessagingHost returns the compiled-in version string
  // instead of calling this method.
}

void MockDaemonController::GetUsageStatsConsent(
    const GetUsageStatsConsentCallback& callback) {
  call_log_ += "GetUsageStatsConsent:";
  callback.Run(true, true, true);
}

class NativeMessagingHostTest : public testing::Test {
 public:
  NativeMessagingHostTest();
  virtual ~NativeMessagingHostTest();

  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  void Run();

  scoped_ptr<base::DictionaryValue> ReadMessageFromOutputPipe();

  void WriteMessageToInputPipe(const base::Value& message);

  // The Host process should shut down when it receives a malformed request.
  // This is tested by sending a known-good request, followed by |message|,
  // followed by the known-good request again. The response file should only
  // contain a single response from the first good request.
  void TestBadRequest(const base::Value& message);

 protected:
  // Reference to the MockDaemonController, which is owned by |host_|.
  MockDaemonController* daemon_controller_;
  std::string call_log_;

 private:
  // Each test creates two unidirectional pipes: "input" and "output".
  // NativeMessagingHost reads from input_read_handle and writes to
  // output_write_handle. The unittest supplies data to input_write_handle, and
  // verifies output from output_read_handle.
  //
  // unittest -> [input] -> NativeMessagingHost -> [output] -> unittest
  base::PlatformFile input_read_handle_;
  base::PlatformFile input_write_handle_;
  base::PlatformFile output_read_handle_;
  base::PlatformFile output_write_handle_;

  base::MessageLoop message_loop_;
  base::RunLoop run_loop_;
  scoped_ptr<remoting::NativeMessagingHost> host_;

  DISALLOW_COPY_AND_ASSIGN(NativeMessagingHostTest);
};

NativeMessagingHostTest::NativeMessagingHostTest()
    : message_loop_(base::MessageLoop::TYPE_IO) {}

NativeMessagingHostTest::~NativeMessagingHostTest() {}

void NativeMessagingHostTest::SetUp() {
  ASSERT_TRUE(MakePipe(&input_read_handle_, &input_write_handle_));
  ASSERT_TRUE(MakePipe(&output_read_handle_, &output_write_handle_));

  daemon_controller_ = new MockDaemonController();
  scoped_ptr<DaemonController> daemon_controller(daemon_controller_);

  scoped_refptr<PairingRegistry> pairing_registry =
      new SynchronousPairingRegistry(scoped_ptr<PairingRegistry::Delegate>(
          new MockPairingRegistryDelegate()));

  host_.reset(new NativeMessagingHost(daemon_controller.Pass(),
                                      pairing_registry,
                                      input_read_handle_, output_write_handle_,
                                      message_loop_.message_loop_proxy(),
                                      run_loop_.QuitClosure()));
}

void NativeMessagingHostTest::TearDown() {
  // The NativeMessagingHost dtor closes the handles that are passed to it.
  // |input_write_handle_| gets closed just before starting the host. So the
  // only handle left to close is |output_read_handle_|.
  base::ClosePlatformFile(output_read_handle_);
}

void NativeMessagingHostTest::Run() {
  // Close the write-end of input, so that the host sees EOF after reading
  // messages and won't block waiting for more input.
  base::ClosePlatformFile(input_write_handle_);
  host_->Start();
  run_loop_.Run();

  // Destroy |host_| so that it closes its end of the output pipe, so that
  // TestBadRequest() will see EOF and won't block waiting for more data.
  // Since |host_| owns |daemon_controller_|, capture its call log first.
  call_log_ = daemon_controller_->call_log();
  host_.reset(NULL);
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

  WriteMessageToInputPipe(good_message);
  WriteMessageToInputPipe(message);
  WriteMessageToInputPipe(good_message);

  Run();

  // Read from output pipe, and verify responses.
  scoped_ptr<base::DictionaryValue> response =
      ReadMessageFromOutputPipe();
  VerifyHelloResponse(response.get());

  response = ReadMessageFromOutputPipe();
  EXPECT_FALSE(response);
}

// Test all valid request-types.
TEST_F(NativeMessagingHostTest, All) {
  base::DictionaryValue message;
  message.SetString("type", "hello");
  WriteMessageToInputPipe(message);

  message.SetString("type", "getHostName");
  WriteMessageToInputPipe(message);

  message.SetString("type", "getPinHash");
  message.SetString("hostId", "my_host");
  message.SetString("pin", "1234");
  WriteMessageToInputPipe(message);

  message.Clear();
  message.SetString("type", "generateKeyPair");
  WriteMessageToInputPipe(message);

  message.SetString("type", "getDaemonConfig");
  WriteMessageToInputPipe(message);

  message.SetString("type", "getUsageStatsConsent");
  WriteMessageToInputPipe(message);

  message.SetString("type", "stopDaemon");
  WriteMessageToInputPipe(message);

  message.SetString("type", "getDaemonState");
  WriteMessageToInputPipe(message);

  // Following messages require a "config" dictionary.
  base::DictionaryValue config;
  config.SetBoolean("update", true);
  message.Set("config", config.DeepCopy());
  message.SetString("type", "updateDaemonConfig");
  WriteMessageToInputPipe(message);

  config.Clear();
  config.SetBoolean("start", true);
  message.Set("config", config.DeepCopy());
  message.SetBoolean("consent", true);
  message.SetString("type", "startDaemon");
  WriteMessageToInputPipe(message);

  Run();

  // Read from output pipe, and verify responses.
  scoped_ptr<base::DictionaryValue> response = ReadMessageFromOutputPipe();
  VerifyHelloResponse(response.get());

  response = ReadMessageFromOutputPipe();
  VerifyGetHostNameResponse(response.get());

  response = ReadMessageFromOutputPipe();
  VerifyGetPinHashResponse(response.get());

  response = ReadMessageFromOutputPipe();
  VerifyGenerateKeyPairResponse(response.get());

  response = ReadMessageFromOutputPipe();
  VerifyGetDaemonConfigResponse(response.get());

  response = ReadMessageFromOutputPipe();
  VerifyGetUsageStatsConsentResponse(response.get());

  response = ReadMessageFromOutputPipe();
  VerifyStopDaemonResponse(response.get());

  response = ReadMessageFromOutputPipe();
  VerifyGetDaemonStateResponse(response.get());

  response = ReadMessageFromOutputPipe();
  VerifyUpdateDaemonConfigResponse(response.get());

  response = ReadMessageFromOutputPipe();
  VerifyStartDaemonResponse(response.get());

  // Verify that DaemonController methods were called in the correct sequence.
  // This detects cases where NativeMessagingHost might call a wrong method
  // that takes the same parameters and writes out the same response.
  EXPECT_EQ("GetConfig:GetUsageStatsConsent:Stop:GetState:UpdateConfig:"
            "SetConfigAndStart:", call_log_);
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
