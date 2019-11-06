// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/serial/serial_connection.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "extensions/browser/api/api_resource_manager.h"
#include "extensions/common/api/serial.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"

namespace extensions {

namespace {

const int kDefaultBufferSize = 4096;

api::serial::SendError ConvertSendErrorFromMojo(
    device::mojom::SerialSendError input) {
  switch (input) {
    case device::mojom::SerialSendError::NONE:
      return api::serial::SEND_ERROR_NONE;
    case device::mojom::SerialSendError::DISCONNECTED:
      return api::serial::SEND_ERROR_DISCONNECTED;
    case device::mojom::SerialSendError::PENDING:
      return api::serial::SEND_ERROR_PENDING;
    case device::mojom::SerialSendError::TIMEOUT:
      return api::serial::SEND_ERROR_TIMEOUT;
    case device::mojom::SerialSendError::SYSTEM_ERROR:
      return api::serial::SEND_ERROR_SYSTEM_ERROR;
  }
  return api::serial::SEND_ERROR_NONE;
}

api::serial::ReceiveError ConvertReceiveErrorFromMojo(
    device::mojom::SerialReceiveError input) {
  switch (input) {
    case device::mojom::SerialReceiveError::NONE:
      return api::serial::RECEIVE_ERROR_NONE;
    case device::mojom::SerialReceiveError::DISCONNECTED:
      return api::serial::RECEIVE_ERROR_DISCONNECTED;
    case device::mojom::SerialReceiveError::TIMEOUT:
      return api::serial::RECEIVE_ERROR_TIMEOUT;
    case device::mojom::SerialReceiveError::DEVICE_LOST:
      return api::serial::RECEIVE_ERROR_DEVICE_LOST;
    case device::mojom::SerialReceiveError::BREAK:
      return api::serial::RECEIVE_ERROR_BREAK;
    case device::mojom::SerialReceiveError::FRAME_ERROR:
      return api::serial::RECEIVE_ERROR_FRAME_ERROR;
    case device::mojom::SerialReceiveError::OVERRUN:
      return api::serial::RECEIVE_ERROR_OVERRUN;
    case device::mojom::SerialReceiveError::BUFFER_OVERFLOW:
      return api::serial::RECEIVE_ERROR_BUFFER_OVERFLOW;
    case device::mojom::SerialReceiveError::PARITY_ERROR:
      return api::serial::RECEIVE_ERROR_PARITY_ERROR;
    case device::mojom::SerialReceiveError::SYSTEM_ERROR:
      return api::serial::RECEIVE_ERROR_SYSTEM_ERROR;
  }
  return api::serial::RECEIVE_ERROR_NONE;
}

api::serial::DataBits ConvertDataBitsFromMojo(
    device::mojom::SerialDataBits input) {
  switch (input) {
    case device::mojom::SerialDataBits::NONE:
      return api::serial::DATA_BITS_NONE;
    case device::mojom::SerialDataBits::SEVEN:
      return api::serial::DATA_BITS_SEVEN;
    case device::mojom::SerialDataBits::EIGHT:
      return api::serial::DATA_BITS_EIGHT;
  }
  return api::serial::DATA_BITS_NONE;
}

device::mojom::SerialDataBits ConvertDataBitsToMojo(
    api::serial::DataBits input) {
  switch (input) {
    case api::serial::DATA_BITS_NONE:
      return device::mojom::SerialDataBits::NONE;
    case api::serial::DATA_BITS_SEVEN:
      return device::mojom::SerialDataBits::SEVEN;
    case api::serial::DATA_BITS_EIGHT:
      return device::mojom::SerialDataBits::EIGHT;
  }
  return device::mojom::SerialDataBits::NONE;
}

api::serial::ParityBit ConvertParityBitFromMojo(
    device::mojom::SerialParityBit input) {
  switch (input) {
    case device::mojom::SerialParityBit::NONE:
      return api::serial::PARITY_BIT_NONE;
    case device::mojom::SerialParityBit::ODD:
      return api::serial::PARITY_BIT_ODD;
    case device::mojom::SerialParityBit::NO_PARITY:
      return api::serial::PARITY_BIT_NO;
    case device::mojom::SerialParityBit::EVEN:
      return api::serial::PARITY_BIT_EVEN;
  }
  return api::serial::PARITY_BIT_NONE;
}

device::mojom::SerialParityBit ConvertParityBitToMojo(
    api::serial::ParityBit input) {
  switch (input) {
    case api::serial::PARITY_BIT_NONE:
      return device::mojom::SerialParityBit::NONE;
    case api::serial::PARITY_BIT_NO:
      return device::mojom::SerialParityBit::NO_PARITY;
    case api::serial::PARITY_BIT_ODD:
      return device::mojom::SerialParityBit::ODD;
    case api::serial::PARITY_BIT_EVEN:
      return device::mojom::SerialParityBit::EVEN;
  }
  return device::mojom::SerialParityBit::NONE;
}

api::serial::StopBits ConvertStopBitsFromMojo(
    device::mojom::SerialStopBits input) {
  switch (input) {
    case device::mojom::SerialStopBits::NONE:
      return api::serial::STOP_BITS_NONE;
    case device::mojom::SerialStopBits::ONE:
      return api::serial::STOP_BITS_ONE;
    case device::mojom::SerialStopBits::TWO:
      return api::serial::STOP_BITS_TWO;
  }
  return api::serial::STOP_BITS_NONE;
}

device::mojom::SerialStopBits ConvertStopBitsToMojo(
    api::serial::StopBits input) {
  switch (input) {
    case api::serial::STOP_BITS_NONE:
      return device::mojom::SerialStopBits::NONE;
    case api::serial::STOP_BITS_ONE:
      return device::mojom::SerialStopBits::ONE;
    case api::serial::STOP_BITS_TWO:
      return device::mojom::SerialStopBits::TWO;
  }
  return device::mojom::SerialStopBits::NONE;
}

}  // namespace

static base::LazyInstance<BrowserContextKeyedAPIFactory<
    ApiResourceManager<SerialConnection>>>::DestructorAtExit g_factory =
    LAZY_INSTANCE_INITIALIZER;

// static
template <>
BrowserContextKeyedAPIFactory<ApiResourceManager<SerialConnection> >*
ApiResourceManager<SerialConnection>::GetFactoryInstance() {
  return g_factory.Pointer();
}

SerialConnection::SerialConnection(
    const std::string& owner_extension_id,
    device::mojom::SerialPortPtrInfo serial_port_info)
    : ApiResource(owner_extension_id),
      persistent_(false),
      buffer_size_(kDefaultBufferSize),
      receive_timeout_(0),
      send_timeout_(0),
      paused_(true),
      read_error_(base::nullopt),
      bytes_written_(0),
      receive_pipe_watcher_(FROM_HERE,
                            mojo::SimpleWatcher::ArmingPolicy::MANUAL),
      send_pipe_watcher_(FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL),
      client_binding_(this),
      weak_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(serial_port_info.is_valid());
  serial_port_.Bind(std::move(serial_port_info));
  serial_port_.set_connection_error_handler(base::BindOnce(
      &SerialConnection::OnConnectionError, base::Unretained(this)));
}

SerialConnection::~SerialConnection() {}

bool SerialConnection::IsPersistent() const {
  return persistent();
}

void SerialConnection::set_buffer_size(int buffer_size) {
  buffer_size_ = buffer_size;
}

void SerialConnection::set_receive_timeout(int receive_timeout) {
  receive_timeout_ = receive_timeout;
}

void SerialConnection::set_send_timeout(int send_timeout) {
  send_timeout_ = send_timeout;
}

void SerialConnection::set_paused(bool paused) {
  DCHECK(serial_port_);
  if (paused_ == paused)
    return;

  paused_ = paused;
  if (!paused_) {
    // If |receive_pipe_| is closed and there is no pending ReceiveError event,
    // try to reconnect the data pipe.
    if (!receive_pipe_ && !read_error_) {
      mojo::ScopedDataPipeProducerHandle receive_producer;
      SetUpReceiveDataPipe(&receive_producer);
      serial_port_->ClearReadError(std::move(receive_producer));
    }
    receive_pipe_watcher_.ArmOrNotify();
    receive_timeout_task_.Cancel();
    SetTimeoutCallback();
  }
}

void SerialConnection::set_connection_error_handler(
    base::OnceClosure connection_error_handler) {
  if (serial_port_.encountered_error()) {
    // Already being disconnected, run client's error handler immediatelly.
    std::move(connection_error_handler).Run();
    return;
  }
  connection_error_handler_ = std::move(connection_error_handler);
}

void SerialConnection::Open(const api::serial::ConnectionOptions& options,
                            OpenCompleteCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(serial_port_);

  if (options.persistent.get())
    set_persistent(*options.persistent);
  if (options.name.get())
    set_name(*options.name);
  if (options.buffer_size.get())
    set_buffer_size(*options.buffer_size);
  if (options.receive_timeout.get())
    set_receive_timeout(*options.receive_timeout);
  if (options.send_timeout.get())
    set_send_timeout(*options.send_timeout);

  mojo::ScopedDataPipeConsumerHandle consumer;
  mojo::ScopedDataPipeProducerHandle producer;
  if (!send_pipe_) {
    SetUpSendDataPipe(&consumer);
  }
  DCHECK(send_pipe_);
  // Make sure receive_pipe_ only be initialized once.
  if (!receive_pipe_) {
    SetUpReceiveDataPipe(&producer);
  }
  DCHECK(receive_pipe_);
  // In case Open() being called more than once.
  if (client_binding_) {
    client_binding_.Close();
  }
  device::mojom::SerialPortClientAssociatedPtrInfo client;
  client_binding_.Bind(mojo::MakeRequest(&client));
  client_binding_.set_connection_error_handler(base::BindOnce(
      &SerialConnection::OnClientBindingClosed, weak_factory_.GetWeakPtr()));

  serial_port_->Open(
      device::mojom::SerialConnectionOptions::From(options),
      std::move(consumer), std::move(producer), std::move(client),
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(callback), false));
}

void SerialConnection::SetUpReceiveDataPipe(
    mojo::ScopedDataPipeProducerHandle* producer) {
  MojoCreateDataPipeOptions options;
  options.struct_size = sizeof(MojoCreateDataPipeOptions);
  options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
  options.element_num_bytes = 1;
  options.capacity_num_bytes = buffer_size_;

  CHECK_EQ(MOJO_RESULT_OK,
           mojo::CreateDataPipe(&options, producer, &receive_pipe_));
  receive_pipe_watcher_.Watch(
      receive_pipe_.get(),
      MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
      base::BindRepeating(&SerialConnection::OnReadPipeReadableOrClosed,
                          weak_factory_.GetWeakPtr()));
}

void SerialConnection::SetUpSendDataPipe(
    mojo::ScopedDataPipeConsumerHandle* consumer) {
  MojoCreateDataPipeOptions options;
  options.struct_size = sizeof(MojoCreateDataPipeOptions);
  options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
  options.element_num_bytes = 1;
  options.capacity_num_bytes = buffer_size_;

  CHECK_EQ(MOJO_RESULT_OK,
           mojo::CreateDataPipe(&options, &send_pipe_, consumer));
  send_pipe_watcher_.Watch(
      send_pipe_.get(),
      MOJO_HANDLE_SIGNAL_WRITABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
      base::BindRepeating(&SerialConnection::OnSendPipeWritableOrClosed,
                          weak_factory_.GetWeakPtr()));
}

void SerialConnection::OnReadError(device::mojom::SerialReceiveError error) {
  DCHECK_NE(device::mojom::SerialReceiveError::NONE, error);
  if (receive_pipe_) {
    // Wait for remaining data to be read from the pipe before signaling an
    // error.
    read_error_ = error;
    return;
  }
  // Dispatch OnReceiveError event when the data pipe has been closed.
  receive_event_cb_.Run(std::vector<uint8_t>(),
                        ConvertReceiveErrorFromMojo(error));
}

void SerialConnection::OnSendError(device::mojom::SerialSendError error) {
  DCHECK_NE(device::mojom::SerialSendError::NONE, error);
  send_pipe_watcher_.Cancel();
  send_pipe_.reset();

  if (send_complete_) {
    // Respond the send request with bytes written currently.
    std::move(send_complete_)
        .Run(bytes_written_, ConvertSendErrorFromMojo(error));
  }
  bytes_written_ = 0;
}

void SerialConnection::OnReadPipeClosed() {
  receive_pipe_watcher_.Cancel();
  receive_pipe_.reset();

  if (read_error_) {
    // Dispatch OnReceiveError if there is a pending error.
    receive_event_cb_.Run(std::vector<uint8_t>(),
                          ConvertReceiveErrorFromMojo(read_error_.value()));
    read_error_.reset();
  }
}

void SerialConnection::OnReadPipeReadableOrClosed(
    MojoResult result,
    const mojo::HandleSignalsState& state) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Data pipe disconnected.
  if (result != MOJO_RESULT_OK) {
    OnReadPipeClosed();
    return;
  }
  // Return when it is paused, the watcher will be disarmed.
  if (paused_)
    return;

  DCHECK(receive_pipe_);
  const void* buffer;
  uint32_t read_bytes;
  result = receive_pipe_->BeginReadData(&buffer, &read_bytes,
                                        MOJO_READ_DATA_FLAG_NONE);
  if (result == MOJO_RESULT_SHOULD_WAIT) {
    receive_pipe_watcher_.ArmOrNotify();
    return;
  }
  // For remote pipe producer handle has been closed.
  if (result == MOJO_RESULT_FAILED_PRECONDITION) {
    OnReadPipeClosed();
    return;
  }
  const char* char_buffer = static_cast<const char*>(buffer);
  std::vector<uint8_t> data(char_buffer, char_buffer + read_bytes);
  result = receive_pipe_->EndReadData(read_bytes);
  DCHECK_EQ(MOJO_RESULT_OK, result);

  receive_event_cb_.Run(std::move(data), api::serial::RECEIVE_ERROR_NONE);
  receive_timeout_task_.Cancel();
  // If there is no error nor paused, go on with the polling process and set
  // timeout callback.
  receive_pipe_watcher_.ArmOrNotify();
  SetTimeoutCallback();
}

void SerialConnection::StartPolling(const ReceiveEventCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  receive_event_cb_ = callback;
  DCHECK(receive_event_cb_);
  DCHECK(receive_pipe_);
  DCHECK(paused_);
  set_paused(false);
}

bool SerialConnection::Send(const std::vector<uint8_t>& data,
                            SendCompleteCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (send_complete_)
    return false;

  DCHECK(serial_port_);
  bytes_written_ = 0;
  send_complete_ = std::move(callback);

  DCHECK(data_to_send_.empty());
  data_to_send_.assign(data.begin(), data.end());

  if (!send_pipe_) {
    mojo::ScopedDataPipeConsumerHandle consumer;
    SetUpSendDataPipe(&consumer);
    serial_port_->ClearSendError(std::move(consumer));
  }
  send_pipe_watcher_.ArmOrNotify();

  send_timeout_task_.Cancel();
  if (send_timeout_ > 0) {
    send_timeout_task_.Reset(base::Bind(&SerialConnection::OnSendTimeout,
                                        weak_factory_.GetWeakPtr()));
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, send_timeout_task_.callback(),
        base::TimeDelta::FromMilliseconds(send_timeout_));
  }
  return true;
}

void SerialConnection::Configure(const api::serial::ConnectionOptions& options,
                                 ConfigureCompleteCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(serial_port_);
  if (options.persistent.get())
    set_persistent(*options.persistent);
  if (options.name.get())
    set_name(*options.name);
  if (options.buffer_size.get())
    set_buffer_size(*options.buffer_size);
  if (options.receive_timeout.get())
    set_receive_timeout(*options.receive_timeout);
  if (options.send_timeout.get())
    set_send_timeout(*options.send_timeout);
  serial_port_->ConfigurePort(
      device::mojom::SerialConnectionOptions::From(options),
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(callback), false));
}

void SerialConnection::GetInfo(GetInfoCompleteCallback callback) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(serial_port_);

  auto info = std::make_unique<api::serial::ConnectionInfo>();
  info->paused = paused_;
  info->persistent = persistent_;
  info->name = name_;
  info->buffer_size = buffer_size_;
  info->receive_timeout = receive_timeout_;
  info->send_timeout = send_timeout_;

  auto resp_callback = base::BindOnce(
      [](GetInfoCompleteCallback callback,
         std::unique_ptr<api::serial::ConnectionInfo> info,
         device::mojom::SerialConnectionInfoPtr port_info) {
        if (!port_info) {
          // Even without remote port info, return partial info and indicate
          // that it's not complete info.
          std::move(callback).Run(false, std::move(info));
          return;
        }
        info->bitrate.reset(new int(port_info->bitrate));
        info->data_bits = ConvertDataBitsFromMojo(port_info->data_bits);
        info->parity_bit = ConvertParityBitFromMojo(port_info->parity_bit);
        info->stop_bits = ConvertStopBitsFromMojo(port_info->stop_bits);
        info->cts_flow_control.reset(new bool(port_info->cts_flow_control));
        std::move(callback).Run(true, std::move(info));
      },
      std::move(callback), std::move(info));
  serial_port_->GetPortInfo(mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      std::move(resp_callback), nullptr));
}

void SerialConnection::Flush(FlushCompleteCallback callback) const {
  DCHECK(serial_port_);
  return serial_port_->Flush(
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(callback), false));
}

void SerialConnection::GetControlSignals(
    GetControlSignalsCompleteCallback callback) const {
  DCHECK(serial_port_);
  auto resp_callback = base::BindOnce(
      [](GetControlSignalsCompleteCallback callback,
         device::mojom::SerialPortControlSignalsPtr signals) {
        if (!signals) {
          std::move(callback).Run(nullptr);
          return;
        }
        auto control_signals =
            std::make_unique<api::serial::DeviceControlSignals>();
        control_signals->dcd = signals->dcd;
        control_signals->cts = signals->cts;
        control_signals->ri = signals->ri;
        control_signals->dsr = signals->dsr;
        std::move(callback).Run(std::move(control_signals));
      },
      std::move(callback));
  serial_port_->GetControlSignals(mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      std::move(resp_callback), nullptr));
}

void SerialConnection::SetControlSignals(
    const api::serial::HostControlSignals& control_signals,
    SetControlSignalsCompleteCallback callback) {
  DCHECK(serial_port_);
  serial_port_->SetControlSignals(
      device::mojom::SerialHostControlSignals::From(control_signals),
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(callback), false));
}

void SerialConnection::SetBreak(SetBreakCompleteCallback callback) {
  DCHECK(serial_port_);
  serial_port_->SetBreak(
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(callback), false));
}

void SerialConnection::ClearBreak(ClearBreakCompleteCallback callback) {
  DCHECK(serial_port_);
  serial_port_->ClearBreak(
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(callback), false));
}

void SerialConnection::SetTimeoutCallback() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (receive_timeout_ > 0) {
    receive_timeout_task_.Reset(base::Bind(&SerialConnection::OnReceiveTimeout,
                                           weak_factory_.GetWeakPtr()));
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, receive_timeout_task_.callback(),
        base::TimeDelta::FromMilliseconds(receive_timeout_));
  }
}

void SerialConnection::OnReceiveTimeout() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(serial_port_);
  receive_event_cb_.Run(std::vector<uint8_t>(),
                        api::serial::RECEIVE_ERROR_TIMEOUT);
}

void SerialConnection::OnSendTimeout() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(serial_port_);
  if (send_complete_) {
    send_pipe_watcher_.Cancel();
    // Respond the send request with bytes_written
    // without closing the data pipe.
    std::move(send_complete_)
        .Run(bytes_written_, api::serial::SEND_ERROR_TIMEOUT);
    bytes_written_ = 0;
  }
}

void SerialConnection::OnSendPipeWritableOrClosed(
    MojoResult result,
    const mojo::HandleSignalsState& state) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Data pipe disconnected.
  if (result != MOJO_RESULT_OK) {
    OnSendPipeClosed();
    return;
  }
  // There is no send task.
  if (!send_complete_) {
    return;
  }

  DCHECK(send_pipe_);
  uint32_t num_bytes = data_to_send_.size();
  result = send_pipe_->WriteData(data_to_send_.data(), &num_bytes,
                                 MOJO_WRITE_DATA_FLAG_NONE);
  if (result == MOJO_RESULT_SHOULD_WAIT) {
    send_pipe_watcher_.ArmOrNotify();
    return;
  }

  // For remote pipe producer handle has been closed.
  if (result == MOJO_RESULT_FAILED_PRECONDITION) {
    OnSendPipeClosed();
    return;
  }

  // For result == MOJO_RESULT_OK case.
  data_to_send_.erase(data_to_send_.begin(), data_to_send_.begin() + num_bytes);
  bytes_written_ += num_bytes;

  if (data_to_send_.empty()) {
    send_timeout_task_.Cancel();
    std::move(send_complete_).Run(bytes_written_, api::serial::SEND_ERROR_NONE);
  } else {
    // Wait for next cycle to send the remaining bytes.
    send_pipe_watcher_.ArmOrNotify();
  }
}

void SerialConnection::OnSendPipeClosed() {
  OnSendError(device::mojom::SerialSendError::DISCONNECTED);
}

void SerialConnection::OnConnectionError() {
  // Run client's error handler if existing.
  if (connection_error_handler_) {
    std::move(connection_error_handler_).Run();
  }
}

void SerialConnection::OnClientBindingClosed() {
  client_binding_.Close();
  OnReadError(device::mojom::SerialReceiveError::DISCONNECTED);
  OnSendError(device::mojom::SerialSendError::DISCONNECTED);
}

}  // namespace extensions

namespace mojo {

// static
device::mojom::SerialHostControlSignalsPtr
TypeConverter<device::mojom::SerialHostControlSignalsPtr,
              extensions::api::serial::HostControlSignals>::
    Convert(const extensions::api::serial::HostControlSignals& input) {
  device::mojom::SerialHostControlSignalsPtr output(
      device::mojom::SerialHostControlSignals::New());
  if (input.dtr.get()) {
    output->has_dtr = true;
    output->dtr = *input.dtr;
  }
  if (input.rts.get()) {
    output->has_rts = true;
    output->rts = *input.rts;
  }
  return output;
}

// static
device::mojom::SerialConnectionOptionsPtr
TypeConverter<device::mojom::SerialConnectionOptionsPtr,
              extensions::api::serial::ConnectionOptions>::
    Convert(const extensions::api::serial::ConnectionOptions& input) {
  device::mojom::SerialConnectionOptionsPtr output(
      device::mojom::SerialConnectionOptions::New());
  if (input.bitrate.get() && *input.bitrate > 0)
    output->bitrate = *input.bitrate;
  output->data_bits = extensions::ConvertDataBitsToMojo(input.data_bits);
  output->parity_bit = extensions::ConvertParityBitToMojo(input.parity_bit);
  output->stop_bits = extensions::ConvertStopBitsToMojo(input.stop_bits);
  if (input.cts_flow_control.get()) {
    output->has_cts_flow_control = true;
    output->cts_flow_control = *input.cts_flow_control;
  }
  return output;
}

}  // namespace mojo
