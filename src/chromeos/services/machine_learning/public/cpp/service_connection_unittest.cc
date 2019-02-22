// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/machine_learning/public/cpp/service_connection.h"

#include "base/macros.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/services/machine_learning/public/mojom/machine_learning_service.mojom.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/embedder/scoped_ipc_support.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace machine_learning {
namespace {

class ServiceConnectionTest : public testing::Test {
 public:
  ServiceConnectionTest() = default;

 protected:
  static void SetUpTestCase() {
    DBusThreadManager::Initialize();

    static base::Thread ipc_thread("ipc");
    ipc_thread.StartWithOptions(
        base::Thread::Options(base::MessageLoop::TYPE_IO, 0));
    static mojo::core::ScopedIPCSupport ipc_support(
        ipc_thread.task_runner(),
        mojo::core::ScopedIPCSupport::ShutdownPolicy::CLEAN);
  }

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;

  DISALLOW_COPY_AND_ASSIGN(ServiceConnectionTest);
};

// Tests that BindModelProvider runs OK (no crash) in a basic Mojo environment.
TEST_F(ServiceConnectionTest, BindModelProvider) {
  mojom::ModelPtr model;
  mojom::ModelSpecPtr spec = mojom::ModelSpec::New(mojom::ModelId::TEST_MODEL);
  ServiceConnection::GetInstance()->LoadModel(
      std::move(spec), mojo::MakeRequest(&model),
      base::BindOnce([](mojom::LoadModelResult result) {}));
}

}  // namespace
}  // namespace machine_learning
}  // namespace chromeos
