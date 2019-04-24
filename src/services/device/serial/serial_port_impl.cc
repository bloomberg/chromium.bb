// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/serial/serial_port_impl.h"

#include <memory>
#include <utility>

#include "base/bind.h"
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
    : io_handler_(device::SerialIoHandler::Create(path, ui_task_runner)),
      out_stream_watcher_(FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL),
      weak_factory_(this) {}

SerialPortImpl::~SerialPortImpl() = default;

void SerialPortImpl::Open(mojom::SerialConnectionOptionsPtr options,
                          mojo::ScopedDataPipeProducerHandle out_stream,
                          mojom::SerialPortClientAssociatedPtrInfo client,
                          OpenCallback callback) {
  DCHECK(out_stream);
  out_stream_ = std::move(out_stream);
  if (client) {
    client_.Bind(std::move(client));
  }
  io_handler_->Open(*options, base::BindOnce(&SerialPortImpl::OnOpenCompleted,
                                             weak_factory_.GetWeakPtr(),
                                             std::move(callback)));
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

void SerialPortImpl::ClearReadError(
    mojo::ScopedDataPipeProducerHandle producer) {
  // Make sure |io_handler_| is still open and the |out_stream_| has been
  // closed.
  if (!io_handler_ || out_stream_) {
    return;
  }
  out_stream_watcher_.Cancel();
  out_stream_.swap(producer);
  out_stream_watcher_.Watch(
      out_stream_.get(),
      MOJO_HANDLE_SIGNAL_WRITABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
      base::BindRepeating(&SerialPortImpl::ReadFromPortAndWriteOut,
                          weak_factory_.GetWeakPtr()));
  out_stream_watcher_.ArmOrNotify();
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
  // Cancel pending reading as the new configure options are applied.
  io_handler_->CancelRead(mojom::SerialReceiveError::NONE);
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

void SerialPortImpl::OnOpenCompleted(OpenCallback callback, bool success) {
  if (success) {
    out_stream_watcher_.Watch(
        out_stream_.get(),
        MOJO_HANDLE_SIGNAL_WRITABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
        MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
        base::BindRepeating(&SerialPortImpl::ReadFromPortAndWriteOut,
                            weak_factory_.GetWeakPtr()));
    out_stream_watcher_.ArmOrNotify();
  }
  std::move(callback).Run(success);
}

void SerialPortImpl::ReadFromPortAndWriteOut(
    MojoResult result,
    const mojo::HandleSignalsState& state) {
  void* buffer;
  uint32_t num_bytes;
  if (result == MOJO_RESULT_OK) {
    result = out_stream_->BeginWriteData(&buffer, &num_bytes,
                                         MOJO_WRITE_DATA_FLAG_NONE);
  }
  if (result == MOJO_RESULT_OK) {
    io_handler_->Read(std::make_unique<ReceiveBuffer>(
        static_cast<char*>(buffer), num_bytes,
        base::BindOnce(&SerialPortImpl::WriteToOutStream,
                       weak_factory_.GetWeakPtr())));
    return;
  }
  if (result == MOJO_RESULT_SHOULD_WAIT) {
    // If there is no space to write, wait for more space.
    out_stream_watcher_.ArmOrNotify();
    return;
  }
  if (result == MOJO_RESULT_FAILED_PRECONDITION) {
    // The |out_stream_| has been closed.
    out_stream_.reset();
    return;
  }
  // The code should not reach other cases.
  NOTREACHED();
}

void SerialPortImpl::WriteToOutStream(int bytes_read,
                                      mojom::SerialReceiveError error) {
  out_stream_->EndWriteData(static_cast<uint32_t>(bytes_read));

  if (error != mojom::SerialReceiveError::NONE) {
    out_stream_.reset();
    if (client_) {
      client_->OnReadError(error);
    }
    return;
  }
  out_stream_watcher_.ArmOrNotify();
}

}  // namespace device
