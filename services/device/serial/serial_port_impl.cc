// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/serial/serial_port_impl.h"

#include <memory>
#include <utility>

#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/device/serial/buffer.h"
#include "services/device/serial/serial_io_handler.h"

namespace device {

// static
void SerialPortImpl::Create(
    const base::FilePath& path,
    mojom::SerialPortRequest request,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner) {
  mojo::MakeStrongBinding(
      std::make_unique<SerialPortImpl>(path, ui_task_runner),
      std::move(request));
}

SerialPortImpl::SerialPortImpl(
    const base::FilePath& path,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner)
    : io_handler_(device::SerialIoHandler::Create(path, ui_task_runner)) {}

SerialPortImpl::~SerialPortImpl() = default;

void SerialPortImpl::Open(mojom::SerialConnectionOptionsPtr options,
                          OpenCallback callback) {
  io_handler_->Open(*options, std::move(callback));
}

void SerialPortImpl::Read(uint32_t bytes, ReadCallback callback) {
  auto buffer = base::MakeRefCounted<net::IOBuffer>(static_cast<size_t>(bytes));
  io_handler_->Read(std::make_unique<ReceiveBuffer>(
      buffer, bytes,
      base::BindOnce(
          [](ReadCallback callback, scoped_refptr<net::IOBuffer> buffer,
             int bytes_read, mojom::SerialReceiveError error) {
            std::move(callback).Run(
                std::vector<uint8_t>(buffer->data(),
                                     buffer->data() + bytes_read),
                error);
          },
          std::move(callback), buffer)));
}

void SerialPortImpl::Write(const std::vector<uint8_t>& data,
                           WriteCallback callback) {
  io_handler_->Write(std::make_unique<SendBuffer>(
      data, base::BindOnce(
                [](WriteCallback callback, int bytes_sent,
                   mojom::SerialSendError error) {
                  std::move(callback).Run(bytes_sent, error);
                },
                std::move(callback))));
}

void SerialPortImpl::CancelRead(mojom::SerialReceiveError reason) {
  io_handler_->CancelRead(reason);
}

void SerialPortImpl::CancelWrite(mojom::SerialSendError reason) {
  io_handler_->CancelWrite(reason);
}

void SerialPortImpl::Flush(FlushCallback callback) {
  std::move(callback).Run(io_handler_->Flush());
}

void SerialPortImpl::GetControlSignals(GetControlSignalsCallback callback) {
  std::move(callback).Run(io_handler_->GetControlSignals());
}

void SerialPortImpl::SetControlSignals(
    mojom::SerialHostControlSignalsPtr signals,
    SetControlSignalsCallback callback) {
  std::move(callback).Run(io_handler_->SetControlSignals(*signals));
}

void SerialPortImpl::ConfigurePort(mojom::SerialConnectionOptionsPtr options,
                                   ConfigurePortCallback callback) {
  std::move(callback).Run(io_handler_->ConfigurePort(*options));
}

void SerialPortImpl::GetPortInfo(GetPortInfoCallback callback) {
  std::move(callback).Run(io_handler_->GetPortInfo());
}

void SerialPortImpl::SetBreak(SetBreakCallback callback) {
  std::move(callback).Run(io_handler_->SetBreak());
}

void SerialPortImpl::ClearBreak(ClearBreakCallback callback) {
  std::move(callback).Run(io_handler_->ClearBreak());
}

}  // namespace device
