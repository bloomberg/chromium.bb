// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"

#include "chromeos/services/ime/ime_service.h"
#include "chromeos/services/ime/public/mojom/constants.mojom.h"
#include "chromeos/services/ime/public/mojom/input_engine.mojom.h"

#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "services/service_manager/public/cpp/service_test.h"
#include "services/service_manager/public/mojom/service_factory.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::_;

namespace chromeos {
namespace ime {

namespace {
const char kTestServiceName[] = "ime_unittests";
const char kInvalidImeSpec[] = "ime_spec_never_support";
const std::vector<uint8_t> extra{0x66, 0x77, 0x88};

void ConnectCallback(bool* success, bool result) {
  *success = result;
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

class ImeServiceTestClient : public service_manager::test::ServiceTestClient,
                             public service_manager::mojom::ServiceFactory {
 public:
  ImeServiceTestClient(service_manager::test::ServiceTest* test)
      : service_manager::test::ServiceTestClient(test) {
    registry_.AddInterface<service_manager::mojom::ServiceFactory>(
        base::BindRepeating(&ImeServiceTestClient::Create,
                            base::Unretained(this)));
  }

 protected:
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override {
    registry_.BindInterface(interface_name, std::move(interface_pipe));
  }

  // service_manager::mojom::ServiceFactory
  void CreateService(
      service_manager::mojom::ServiceRequest request,
      const std::string& name,
      service_manager::mojom::PIDReceiverPtr pid_receiver) override {
    if (name == mojom::kServiceName) {
      service_context_.reset(new service_manager::ServiceContext(
          CreateImeService(), std::move(request)));
    }
  }

  void Create(service_manager::mojom::ServiceFactoryRequest request) {
    service_factory_bindings_.AddBinding(this, std::move(request));
  }

 private:
  service_manager::BinderRegistry registry_;
  mojo::BindingSet<service_manager::mojom::ServiceFactory>
      service_factory_bindings_;

  std::unique_ptr<service_manager::ServiceContext> service_context_;
  DISALLOW_COPY_AND_ASSIGN(ImeServiceTestClient);
};

class ImeServiceTest : public service_manager::test::ServiceTest {
 public:
  ImeServiceTest() : service_manager::test::ServiceTest(kTestServiceName) {}
  ~ImeServiceTest() override {}

  MOCK_METHOD1(SentTextCallback, void(const std::string&));
  MOCK_METHOD1(SentMessageCallback, void(const std::vector<uint8_t>&));

 protected:
  void SetUp() override {
    ServiceTest::SetUp();
    connector()->BindInterface(mojom::kServiceName,
                               mojo::MakeRequest(&ime_manager_));

    // TODO(https://crbug.com/837156): Start or bind other services used.
    // Eg.  connector()->StartService(mojom::kSomeServiceName);
  }

  // service_manager::test::ServiceTest
  std::unique_ptr<service_manager::Service> CreateService() override {
    return std::make_unique<ImeServiceTestClient>(this);
  }

  void TearDown() override {
    ime_manager_.reset();
    ServiceTest::TearDown();
  }

  mojom::InputEngineManagerPtr ime_manager_;

 private:
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

}  // namespace ime
}  // namespace chromeos
