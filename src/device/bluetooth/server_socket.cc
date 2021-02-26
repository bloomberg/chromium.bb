// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/server_socket.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_socket.h"
#include "device/bluetooth/device.h"
#include "device/bluetooth/socket.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/io_buffer.h"

namespace bluetooth {

ServerSocket::ServerSocket(
    scoped_refptr<device::BluetoothSocket> bluetooth_socket)
    : server_socket_(std::move(bluetooth_socket)) {}

ServerSocket::~ServerSocket() {
  server_socket_->Close();
}

void ServerSocket::Accept(AcceptCallback callback) {
  auto copyable_callback = base::AdaptCallbackForRepeating(std::move(callback));
  server_socket_->Accept(
      base::BindOnce(&ServerSocket::OnAccept, weak_ptr_factory_.GetWeakPtr(),
                     copyable_callback),
      base::BindOnce(&ServerSocket::OnAcceptError,
                     weak_ptr_factory_.GetWeakPtr(), copyable_callback));
}

void ServerSocket::OnAccept(
    AcceptCallback callback,
    const device::BluetoothDevice* device,
    scoped_refptr<device::BluetoothSocket> bluetooth_socket) {
  mojo::ScopedDataPipeProducerHandle receive_pipe_producer_handle;
  mojo::ScopedDataPipeConsumerHandle receive_pipe_consumer_handle;
  MojoResult result =
      mojo::CreateDataPipe(/*options=*/nullptr, &receive_pipe_producer_handle,
                           &receive_pipe_consumer_handle);
  if (result != MOJO_RESULT_OK) {
    bluetooth_socket->Close();
    OnAcceptError(std::move(callback), "Failed to create receiving DataPipe.");
    return;
  }

  mojo::ScopedDataPipeProducerHandle send_pipe_producer_handle;
  mojo::ScopedDataPipeConsumerHandle send_pipe_consumer_handle;
  result = mojo::CreateDataPipe(/*options=*/nullptr, &send_pipe_producer_handle,
                                &send_pipe_consumer_handle);
  if (result != MOJO_RESULT_OK) {
    bluetooth_socket->Close();
    OnAcceptError(std::move(callback), "Failed to create sending DataPipe.");
    return;
  }

  mojo::PendingRemote<mojom::Socket> pending_socket;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<Socket>(std::move(bluetooth_socket),
                               std::move(receive_pipe_producer_handle),
                               std::move(send_pipe_consumer_handle)),
      pending_socket.InitWithNewPipeAndPassReceiver());

  mojom::AcceptConnectionResultPtr accept_connection_result =
      mojom::AcceptConnectionResult::New();
  accept_connection_result->device = Device::ConstructDeviceInfoStruct(device);
  accept_connection_result->socket = std::move(pending_socket);
  accept_connection_result->receive_stream =
      std::move(receive_pipe_consumer_handle);
  accept_connection_result->send_stream = std::move(send_pipe_producer_handle);

  std::move(callback).Run(std::move(accept_connection_result));
}

void ServerSocket::OnAcceptError(AcceptCallback callback,
                                 const std::string& error_message) {
  LOG(ERROR) << "Failed to accept incoming connection: " << error_message;
  std::move(callback).Run(/*result=*/nullptr);
}

}  // namespace bluetooth
