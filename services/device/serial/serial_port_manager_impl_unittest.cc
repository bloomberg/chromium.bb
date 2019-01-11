// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/serial/serial_port_manager_impl.h"

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/task/post_task.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/device/device_service_test_base.h"
#include "services/device/public/mojom/constants.mojom.h"
#include "services/device/public/mojom/serial.mojom.h"
#include "services/device/serial/fake_serial_device_enumerator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

const base::FilePath kFakeDevicePath1(FILE_PATH_LITERAL("/dev/fakeserialmojo"));
const base::FilePath kFakeDevicePath2(FILE_PATH_LITERAL("\\\\COM800\\"));

void CreateAndBindOnBlockableRunner(
    mojom::SerialPortManagerRequest request,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner) {
  auto manager = std::make_unique<SerialPortManagerImpl>(
      std::move(io_task_runner), std::move(ui_task_runner));
  auto fake_enumerator = std::make_unique<FakeSerialEnumerator>();
  fake_enumerator->AddDevicePath(kFakeDevicePath1);
  fake_enumerator->AddDevicePath(kFakeDevicePath2);
  manager->SetSerialEnumeratorForTesting(std::move(fake_enumerator));
  mojo::MakeStrongBinding(std::move(manager), std::move(request));
}

void ExpectDevicesAndThen(const std::set<base::FilePath>& expected_paths,
                          base::OnceClosure continuation,
                          std::vector<mojom::SerialPortInfoPtr> results) {
  EXPECT_EQ(expected_paths.size(), results.size());
  std::set<base::FilePath> actual_paths;
  for (size_t i = 0; i < results.size(); ++i)
    actual_paths.insert(results[i]->path);
  EXPECT_EQ(expected_paths, actual_paths);
  std::move(continuation).Run();
}

void OnGotDevicesAndGetPort(mojom::SerialPortManagerPtr port_manager,
                            base::OnceClosure continuation,
                            std::vector<mojom::SerialPortInfoPtr> results) {
  EXPECT_GT(results.size(), 0u);

  mojom::SerialPortPtr serial_port;
  port_manager->GetPort(results[0]->token, mojo::MakeRequest(&serial_port));
  // Send a message on the pipe and wait for the response to make sure that the
  // interface request was bound successfully.
  serial_port.FlushForTesting();
  EXPECT_FALSE(serial_port.encountered_error());
  std::move(continuation).Run();
}

void OnGotDevicesForSimpleConnect(
    mojom::SerialPortManagerPtr port_manager,
    base::OnceClosure continuation,
    std::vector<mojom::SerialPortInfoPtr> results) {
  for (auto& device : results) {
    mojom::SerialPortPtr serial_port;
    port_manager->GetPort(device->token, mojo::MakeRequest(&serial_port));
    // Send a message on the pipe and wait for the response to make sure that
    // the interface request was bound successfully.
    serial_port.FlushForTesting();
    EXPECT_FALSE(serial_port.encountered_error());
  }
  std::move(continuation).Run();
}

}  // namespace

class SerialPortManagerImplTest : public DeviceServiceTestBase {
 public:
  SerialPortManagerImplTest() {
    blockable_runner_ = base::CreateSequencedTaskRunnerWithTraits(
        {base::MayBlock(), base::TaskPriority::BEST_EFFORT});
  }
  ~SerialPortManagerImplTest() override = default;

 protected:
  void SetUp() override { DeviceServiceTestBase::SetUp(); }

  void BindSerialPortManager(mojom::SerialPortManagerRequest request) {
    blockable_runner_->PostTask(
        FROM_HERE, base::BindOnce(&CreateAndBindOnBlockableRunner,
                                  std::move(request), io_thread_.task_runner(),
                                  base::ThreadTaskRunnerHandle::Get()));
  }

  scoped_refptr<base::SequencedTaskRunner> blockable_runner_;

  DISALLOW_COPY_AND_ASSIGN(SerialPortManagerImplTest);
};

// This is to simply test that we can enumerate devices on the platform without
// hanging or crashing.
TEST_F(SerialPortManagerImplTest, SimpleConnectTest) {
  mojom::SerialPortManagerPtr port_manager;
  connector()->BindInterface(mojom::kServiceName, &port_manager);

  base::RunLoop loop;
  auto* manager = port_manager.get();
  manager->GetDevices(base::BindOnce(&OnGotDevicesForSimpleConnect,
                                     std::move(port_manager),
                                     loop.QuitClosure()));
  loop.Run();
}

TEST_F(SerialPortManagerImplTest, GetDevices) {
  mojom::SerialPortManagerPtr port_manager;
  BindSerialPortManager(mojo::MakeRequest(&port_manager));
  std::set<base::FilePath> expected_paths = {kFakeDevicePath1,
                                             kFakeDevicePath2};

  base::RunLoop loop;
  port_manager->GetDevices(base::BindOnce(&ExpectDevicesAndThen, expected_paths,
                                          loop.QuitClosure()));
  loop.Run();
}

TEST_F(SerialPortManagerImplTest, GetPort) {
  mojom::SerialPortManagerPtr port_manager;
  BindSerialPortManager(mojo::MakeRequest(&port_manager));

  base::RunLoop loop;
  auto* manager = port_manager.get();
  manager->GetDevices(base::BindOnce(
      &OnGotDevicesAndGetPort, std::move(port_manager), loop.QuitClosure()));
  loop.Run();
}

}  // namespace device
