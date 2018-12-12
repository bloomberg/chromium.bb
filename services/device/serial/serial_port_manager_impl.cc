// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/serial/serial_port_manager_impl.h"

#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace device {

namespace {

void CreateAndBindOnBlockableRunner(
    mojom::SerialPortManagerRequest request,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner) {
  mojo::MakeStrongBinding(
      std::make_unique<SerialPortManagerImpl>(std::move(ui_task_runner)),
      std::move(request));
}

}  // namespace

// static
void SerialPortManagerImpl::Create(mojom::SerialPortManagerRequest request) {
  // SerialPortManagerImpl must live on a thread that is allowed to do
  // blocking IO.
  scoped_refptr<base::SequencedTaskRunner> blockable_sequence_runner =
      base::CreateSequencedTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT});
  blockable_sequence_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&CreateAndBindOnBlockableRunner, std::move(request),
                     base::ThreadTaskRunnerHandle::Get()));
}

// SerialPortManagerImpl must be created in a blockable runner.
SerialPortManagerImpl::SerialPortManagerImpl(
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner)
    : enumerator_(device::SerialDeviceEnumerator::Create()),
      ui_task_runner_(std::move(ui_task_runner)) {}

SerialPortManagerImpl::~SerialPortManagerImpl() = default;

void SerialPortManagerImpl::GetDevices(GetDevicesCallback callback) {
  DCHECK(enumerator_);
  std::move(callback).Run(enumerator_->GetDevices());
}

void SerialPortManagerImpl::GetPort(const std::string& path,
                                    mojom::SerialPortRequest request) {
  SerialPortImpl::Create(path, std::move(request), ui_task_runner_);
}

}  // namespace device
