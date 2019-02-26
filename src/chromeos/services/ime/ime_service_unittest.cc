// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/ime/ime_service.h"
#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_task_environment.h"
#include "chromeos/services/ime/public/mojom/constants.mojom.h"
#include "chromeos/services/ime/public/mojom/input_engine.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/service_manager/public/cpp/service_binding.h"
#include "services/service_manager/public/cpp/test/test_connector_factory.h"
#include "services/service_manager/public/mojom/service_factory.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace chromeos {
namespace ime {

namespace {

const char kInvalidImeSpec[] = "ime_spec_never_support";
const std::vector<uint8_t> extra{0x66, 0x77, 0x88};

void ConnectCallback(bool* success, bool result) {
  *success = result;
}

void TestProcessTextCallback(std::string* res_out,
                             const std::string& response) {
  *res_out = response;
}

class TestClientChannel : mojom::InputChannel {
 public:
  TestClientChannel() : binding_(this) {}

  mojom::InputChannelPtr CreateInterfacePtrAndBind() {
    mojom::InputChannelPtr ptr;
    binding_.Bind(mojo::MakeRequest(&ptr));
    return ptr;
  }

  // mojom::InputChannel implementation.
  MOCK_METHOD2(ProcessText, void(const std::string&, ProcessTextCallback));
  MOCK_METHOD2(ProcessMessage,
               void(const std::vector<uint8_t>& message,
                    ProcessMessageCallback));

 private:
  mojo::Binding<mojom::InputChannel> binding_;

  DISALLOW_COPY_AND_ASSIGN(TestClientChannel);
};

class ImeServiceTest : public testing::Test {
 public:
  ImeServiceTest()
      : service_(test_connector_factory_.RegisterInstance(mojom::kServiceName)),
        connector_(test_connector_factory_.CreateConnector()) {}
  ~ImeServiceTest() override = default;

  MOCK_METHOD1(SentTextCallback, void(const std::string&));
  MOCK_METHOD1(SentMessageCallback, void(const std::vector<uint8_t>&));

 protected:
  void SetUp() override {
    connector_->BindInterface(mojom::kServiceName,
                              mojo::MakeRequest(&ime_manager_));

    // TODO(https://crbug.com/837156): Start or bind other services used.
    // Eg.  connector()->StartService(mojom::kSomeServiceName);
  }

  mojom::InputEngineManagerPtr ime_manager_;

 private:
  base::test::ScopedTaskEnvironment task_environment_;
  service_manager::TestConnectorFactory test_connector_factory_;
  ImeService service_;
  std::unique_ptr<service_manager::Connector> connector_;

  DISALLOW_COPY_AND_ASSIGN(ImeServiceTest);
};

}  // namespace

// Tests that the service is instantiated and it will return false when
// activating an IME engine with an invalid IME spec.
TEST_F(ImeServiceTest, ConnectInvalidImeEngine) {
  bool success = true;
  TestClientChannel test_channel;
  mojom::InputChannelPtr to_engine_ptr;

  ime_manager_->ConnectToImeEngine(
      kInvalidImeSpec, mojo::MakeRequest(&to_engine_ptr),
      test_channel.CreateInterfacePtrAndBind(), extra,
      base::BindOnce(&ConnectCallback, &success));
  ime_manager_.FlushForTesting();
  EXPECT_FALSE(success);
}

TEST_F(ImeServiceTest, MultipleClients) {
  bool success = false;
  TestClientChannel test_channel1;
  TestClientChannel test_channel2;
  mojom::InputChannelPtr to_engine_ptr1;
  mojom::InputChannelPtr to_engine_ptr2;

  ime_manager_->ConnectToImeEngine(
      "m17n:ar", mojo::MakeRequest(&to_engine_ptr1),
      test_channel1.CreateInterfacePtrAndBind(), extra,
      base::BindOnce(&ConnectCallback, &success));
  ime_manager_.FlushForTesting();

  ime_manager_->ConnectToImeEngine(
      "m17n:ar", mojo::MakeRequest(&to_engine_ptr2),
      test_channel2.CreateInterfacePtrAndBind(), extra,
      base::BindOnce(&ConnectCallback, &success));
  ime_manager_.FlushForTesting();

  std::string response;
  std::string process_text_key =
      "{\"method\":\"keyEvent\",\"type\":\"keydown\""
      ",\"code\":\"KeyA\",\"shift\":true,\"altgr\":false,\"caps\":false}";
  to_engine_ptr1->ProcessText(
      process_text_key, base::BindOnce(&TestProcessTextCallback, &response));
  to_engine_ptr1.FlushForTesting();

  to_engine_ptr2->ProcessText(
      process_text_key, base::BindOnce(&TestProcessTextCallback, &response));
  to_engine_ptr2.FlushForTesting();

  std::string process_text_key_count = "{\"method\":\"countKey\"}";
  to_engine_ptr1->ProcessText(
      process_text_key_count,
      base::BindOnce(&TestProcessTextCallback, &response));
  to_engine_ptr1.FlushForTesting();
  EXPECT_EQ("1", response);

  to_engine_ptr2->ProcessText(
      process_text_key_count,
      base::BindOnce(&TestProcessTextCallback, &response));
  to_engine_ptr2.FlushForTesting();
  EXPECT_EQ("1", response);
}

// Tests that the rule-based Arabic keyboard can work correctly.
TEST_F(ImeServiceTest, RuleBasedArabic) {
  bool success = false;
  TestClientChannel test_channel;
  mojom::InputChannelPtr to_engine_ptr;

  ime_manager_->ConnectToImeEngine("m17n:ar", mojo::MakeRequest(&to_engine_ptr),
                                   test_channel.CreateInterfacePtrAndBind(),
                                   extra,
                                   base::BindOnce(&ConnectCallback, &success));
  ime_manager_.FlushForTesting();
  EXPECT_TRUE(success);

  // Test Shift+KeyA.
  std::string response;
  to_engine_ptr->ProcessText(
      "{\"method\":\"keyEvent\",\"type\":\"keydown\",\"code\":\"KeyA\","
      "\"shift\":true,\"altgr\":false,\"caps\":false}",
      base::BindOnce(&TestProcessTextCallback, &response));
  to_engine_ptr.FlushForTesting();
  const wchar_t* expected_response =
      L"{\"result\":true,\"operations\":[{\"method\":\"commitText\","
      L"\"arguments\":[\"\u0650\"]}]}";
  EXPECT_EQ(base::WideToUTF8(expected_response), response);

  // Test KeyB.
  to_engine_ptr->ProcessText(
      "{\"method\":\"keyEvent\",\"type\":\"keydown\",\"code\":\"KeyB\","
      "\"shift\":false,\"altgr\":false,\"caps\":false}",
      base::BindOnce(&TestProcessTextCallback, &response));
  to_engine_ptr.FlushForTesting();
  expected_response =
      L"{\"result\":true,\"operations\":[{\"method\":\"commitText\","
      L"\"arguments\":[\"\u0644\u0627\"]}]}";
  EXPECT_EQ(base::WideToUTF8(expected_response), response);

  // Test unhandled key.
  to_engine_ptr->ProcessText(
      "{\"method\":\"keyEvent\",\"type\":\"keydown\",\"code\":\"Enter\","
      "\"shift\":false,\"altgr\":false,\"caps\":false}",
      base::BindOnce(&TestProcessTextCallback, &response));
  to_engine_ptr.FlushForTesting();
  EXPECT_EQ("{\"result\":false}", response);

  // Test keyup.
  to_engine_ptr->ProcessText(
      "{\"method\":\"keyEvent\",\"type\":\"keyup\",\"code\":\"Enter\","
      "\"shift\":false,\"altgr\":false,\"caps\":false}",
      base::BindOnce(&TestProcessTextCallback, &response));
  to_engine_ptr.FlushForTesting();
  EXPECT_EQ("{\"result\":false}", response);

  // Test reset.
  to_engine_ptr->ProcessText(
      "{\"method\":\"reset\"}",
      base::BindOnce(&TestProcessTextCallback, &response));
  to_engine_ptr.FlushForTesting();
  EXPECT_EQ("{\"result\":true}", response);

  // Test invalid request.
  to_engine_ptr->ProcessText(
      "{\"method\":\"keyEvent\",\"type\":\"keydown\"}",
      base::BindOnce(&TestProcessTextCallback, &response));
  to_engine_ptr.FlushForTesting();
  EXPECT_EQ("{\"result\":false}", response);
}

// Tests that the rule-based DevaPhone keyboard can work correctly.
TEST_F(ImeServiceTest, RuleBasedDevaPhone) {
  bool success = false;
  TestClientChannel test_channel;
  mojom::InputChannelPtr to_engine_ptr;

  ime_manager_->ConnectToImeEngine(
      "m17n:deva_phone", mojo::MakeRequest(&to_engine_ptr),
      test_channel.CreateInterfacePtrAndBind(), extra,
      base::BindOnce(&ConnectCallback, &success));
  ime_manager_.FlushForTesting();
  EXPECT_TRUE(success);

  std::string response;

  // KeyN.
  to_engine_ptr->ProcessText(
      "{\"method\":\"keyEvent\",\"type\":\"keydown\",\"code\":\"KeyN\","
      "\"shift\":false,\"altgr\":false,\"caps\":false}",
      base::BindOnce(&TestProcessTextCallback, &response));
  to_engine_ptr.FlushForTesting();
  const char* expected_response =
      u8"{\"result\":true,\"operations\":[{\"method\":\"setComposition\","
      u8"\"arguments\":[\"\u0928\"]}]}";
  EXPECT_EQ(expected_response, response);

  // Backspace.
  to_engine_ptr->ProcessText(
      "{\"method\":\"keyEvent\",\"type\":\"keydown\",\"code\":\"Backspace\","
      "\"shift\":false,\"altgr\":false,\"caps\":false}",
      base::BindOnce(&TestProcessTextCallback, &response));
  to_engine_ptr.FlushForTesting();
  expected_response =
      u8"{\"result\":true,\"operations\":[{\"method\":\"setComposition\","
      u8"\"arguments\":[\"\"]}]}";
  EXPECT_EQ(expected_response, response);

  // KeyN + KeyC.
  to_engine_ptr->ProcessText(
      "{\"method\":\"keyEvent\",\"type\":\"keydown\",\"code\":\"KeyN\","
      "\"shift\":false,\"altgr\":false,\"caps\":false}",
      base::BindOnce(&TestProcessTextCallback, &response));
  to_engine_ptr->ProcessText(
      "{\"method\":\"keyEvent\",\"type\":\"keydown\",\"code\":\"KeyC\","
      "\"shift\":false,\"altgr\":false,\"caps\":false}",
      base::BindOnce(&TestProcessTextCallback, &response));
  to_engine_ptr.FlushForTesting();
  expected_response =
      u8"{\"result\":true,\"operations\":[{\"method\":\"setComposition\","
      u8"\"arguments\":[\"\u091e\u094d\u091a\"]}]}";
  EXPECT_EQ(expected_response, response);

  // Space.
  to_engine_ptr->ProcessText(
      "{\"method\":\"keyEvent\",\"type\":\"keydown\",\"code\":\"Space\","
      "\"shift\":false,\"altgr\":false,\"caps\":false}",
      base::BindOnce(&TestProcessTextCallback, &response));
  to_engine_ptr.FlushForTesting();
  expected_response =
      u8"{\"result\":true,\"operations\":[{\"method\":\"commitText\","
      u8"\"arguments\":[\"\u091e\u094d\u091a \"]}]}";
  EXPECT_EQ(expected_response, response);
}

}  // namespace ime
}  // namespace chromeos
