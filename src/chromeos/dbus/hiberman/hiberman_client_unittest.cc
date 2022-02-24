// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/hiberman/hiberman_client.h"

#include <string>
#include <utility>

#include "base/test/task_environment.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_object_proxy.h"
#include "dbus/object_path.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/hiberman/dbus-constants.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::SaveArg;

namespace chromeos {

class HibermanClientTest : public testing::Test {
 public:
  HibermanClientTest() = default;
  ~HibermanClientTest() override = default;

  void SetUp() override {
    dbus::Bus::Options options;
    options.bus_type = dbus::Bus::SYSTEM;
    bus_ = new dbus::MockBus(options);

    dbus::ObjectPath hibernate_object_path =
        dbus::ObjectPath(::hiberman::kHibernateServicePath);
    proxy_ = new dbus::MockObjectProxy(
        bus_.get(), ::hiberman::kHibernateServiceName,
        hibernate_object_path);

    // Makes sure `GetObjectProxy()` is caled with the correct service name and
    // path.
    EXPECT_CALL(*bus_.get(),
                GetObjectProxy(::hiberman::kHibernateServiceName,
                               hibernate_object_path))
        .WillRepeatedly(Return(proxy_.get()));

    EXPECT_CALL(*proxy_.get(), DoCallMethod(_, _, _))
        .WillRepeatedly(Invoke(this, &HibermanClientTest::OnCallMethod));

    HibermanClient::Initialize(bus_.get());

    // Execute callbacks posted by `client_->Init()`.
    base::RunLoop().RunUntilIdle();

    client_ = HibermanClient::Get();
  }

  void TearDown() override { HibermanClient::Shutdown(); }

 protected:

  base::test::SingleThreadTaskEnvironment task_environment_;

  // Mock bus and proxy for simulating calls.
  scoped_refptr<dbus::MockBus> bus_;
  scoped_refptr<dbus::MockObjectProxy> proxy_;

  // Convenience pointer to the global instance.
  HibermanClient* client_ = nullptr;

 private:
  // Handles calls to |proxy_|'s `CallMethod()`.
  void OnCallMethod(dbus::MethodCall* method_call,
                    int timeout_ms,
                    dbus::ObjectProxy::ResponseCallback* callback) {
    std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
    dbus::MessageWriter writer(response.get());
    if (method_call->GetMember() == ::hiberman::kResumeFromHibernateMethod) {
      dbus::MessageReader reader(method_call);
      std::string account_id;
      // The Resume method should have an account_id string.
      ASSERT_TRUE(reader.PopString(&account_id));
      // There's no reply data for this method.
    } else {
      ASSERT_FALSE(true) << "Unrecognized member: " << method_call->GetMember();
    }
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](dbus::ObjectProxy::ResponseCallback callback,
               std::unique_ptr<dbus::Response> response)
               {std::move(callback).Run(response.get()); },
            std::move(*callback), std::move(response)));
  }
};

TEST_F(HibermanClientTest, ResumeFromHibernate) {
  bool callback_called = false;
  auto callback = base::BindOnce(
      [](bool* callback_was_called,
         bool method_call_success) {
        *callback_was_called = true;
      },
      &callback_called);

  client_->ResumeFromHibernate("test@google.com", std::move(callback));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(callback_called);
}

}  // namespace chromeos
