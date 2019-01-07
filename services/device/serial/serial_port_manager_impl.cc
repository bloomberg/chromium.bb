// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/serial/serial_port_manager_impl.h"

#include <string>
#include <utility>

#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/device/serial/serial_device_enumerator.h"
#include "services/device/serial/serial_port_impl.h"

namespace device {

namespace {

void CreateAndBindOnBlockableRunner(
    mojom::SerialPortManagerRequest request,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner) {
  mojo::MakeStrongBinding(
      std::make_unique<SerialPortManagerImpl>(std::move(io_task_runner),
                                              std::move(ui_task_runner)),
      std::move(request));
}

}  // namespace

// static
void SerialPortManagerImpl::Create(
    mojom::SerialPortManagerRequest request,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner) {
  // SerialPortManagerImpl must live on a thread that is allowed to do
  // blocking IO.
  scoped_refptr<base::SequencedTaskRunner> blockable_sequence_runner =
      base::CreateSequencedTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT});
  blockable_sequence_runner->PostTask(
      FROM_HERE, base::BindOnce(&CreateAndBindOnBlockableRunner,
                                std::move(request), std::move(io_task_runner),
                                base::ThreadTaskRunnerHandle::Get()));
}

// SerialPortManagerImpl must be created in a blockable runner.
SerialPortManagerImpl::SerialPortManagerImpl(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner)
    : enumerator_(SerialDeviceEnumerator::Create()),
      io_task_runner_(std::move(io_task_runner)),
      ui_task_runner_(std::move(ui_task_runner)) {}

SerialPortManagerImpl::~SerialPortManagerImpl() = default;

void SerialPortManagerImpl::SetSerialEnumeratorForTesting(
    std::unique_ptr<SerialDeviceEnumerator> fake_enumerator) {
  DCHECK(fake_enumerator);
  enumerator_ = std::move(fake_enumerator);
}

void SerialPortManagerImpl::GetDevices(GetDevicesCallback callback) {
  DCHECK(enumerator_);
  std::move(callback).Run(enumerator_->GetDevices());
}

void SerialPortManagerImpl::GetPort(const base::UnguessableToken& token,
                                    mojom::SerialPortRequest request) {
  DCHECK(enumerator_);
  base::Optional<std::string> path = enumerator_->GetPathFromToken(token);
  if (path) {
    io_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&SerialPortImpl::Create, *path,
                                  std::move(request), ui_task_runner_));
  }
}

}  // namespace device
