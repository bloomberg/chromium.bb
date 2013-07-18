// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/android/forwarder2/host_controller.h"

#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "tools/android/forwarder2/command.h"
#include "tools/android/forwarder2/forwarder.h"
#include "tools/android/forwarder2/socket.h"

namespace forwarder2 {

HostController::HostController(int device_port,
                               const std::string& forward_to_host,
                               int forward_to_host_port,
                               int adb_port,
                               int exit_notifier_fd,
                               const DeleteCallback& delete_callback)
    : device_port_(device_port),
      forward_to_host_(forward_to_host),
      forward_to_host_port_(forward_to_host_port),
      adb_port_(adb_port),
      global_exit_notifier_fd_(exit_notifier_fd),
      delete_callback_(delete_callback),
      ready_(false),
      thread_("HostControllerThread") {
  adb_control_socket_.AddEventFd(global_exit_notifier_fd_);
  adb_control_socket_.AddEventFd(delete_controller_notifier_.receiver_fd());
}

HostController::~HostController() {
  delete_controller_notifier_.Notify();
  // Note that the Forwarder instance (that also received a delete notification)
  // might still be running on its own thread at this point. This is not a
  // problem since it will self-delete once the socket that it is operating on
  // is closed.
}

bool HostController::Connect() {
  if (!adb_control_socket_.ConnectTcp("", adb_port_)) {
    LOG(ERROR) << "Could not connect HostController socket on port: "
               << adb_port_;
    delete_callback_.Run(this);
    return false;
  }
  // Send the command to the device start listening to the "device_forward_port"
  bool send_command_success = SendCommand(
      command::LISTEN, device_port_, &adb_control_socket_);
  CHECK(send_command_success);
  int device_port_allocated;
  command::Type command;
  if (!ReadCommand(&adb_control_socket_, &device_port_allocated, &command) ||
      command != command::BIND_SUCCESS) {
    LOG(ERROR) << "Device binding error using port " << device_port_;
    delete_callback_.Run(this);
    return false;
  }
  // When doing dynamically allocation of port, we get the port from the
  // BIND_SUCCESS command we received above.
  device_port_ = device_port_allocated;
  ready_ = true;
  return true;
}

void HostController::Start() {
  thread_.Start();
  thread_.message_loop_proxy()->PostTask(
      FROM_HERE,
      base::Bind(&HostController::ThreadHandler, base::Unretained(this)));
}

void HostController::ThreadHandler() {
  CHECK(ready_) << "HostController not ready. Must call Connect() first.";
  while (true) {
    if (!ReceivedCommand(command::ACCEPT_SUCCESS, &adb_control_socket_)) {
      delete_callback_.Run(this);
      return;
    }
    // Try to connect to host server.
    scoped_ptr<Socket> host_server_data_socket(CreateSocket());
    if (!host_server_data_socket->ConnectTcp(
            forward_to_host_, forward_to_host_port_)) {
      LOG(ERROR) << "Could not Connect HostServerData socket on port: "
                 << forward_to_host_port_;
      SendCommand(command::HOST_SERVER_ERROR,
                  device_port_,
                  &adb_control_socket_);
      if (ReceivedCommand(command::ACK, &adb_control_socket_)) {
        // It can continue if the host forwarder could not connect to the host
        // server but the device acknowledged that, so that the device could
        // re-try later.
        continue;
      }
      delete_callback_.Run(this);
      return;
    }
    SendCommand(command::HOST_SERVER_SUCCESS,
                device_port_,
                &adb_control_socket_);
    StartForwarder(host_server_data_socket.Pass());
  }
}

void HostController::StartForwarder(
    scoped_ptr<Socket> host_server_data_socket) {
  scoped_ptr<Socket> adb_data_socket(CreateSocket());
  if (!adb_data_socket->ConnectTcp("", adb_port_)) {
    LOG(ERROR) << "Could not connect AdbDataSocket on port: " << adb_port_;
    delete_callback_.Run(this);
    return;
  }
  // Open the Adb data connection, and send a command with the
  // |device_forward_port| as a way for the device to identify the connection.
  SendCommand(command::DATA_CONNECTION,
              device_port_,
              adb_data_socket.get());

  // Check that the device received the new Adb Data Connection. Note that this
  // check is done through the |adb_control_socket_| that is handled in the
  // DeviceListener thread just after the call to WaitForAdbDataSocket().
  if (!ReceivedCommand(command::ADB_DATA_SOCKET_SUCCESS,
                       &adb_control_socket_)) {
    LOG(ERROR) << "Device could not handle the new Adb Data Connection.";
    delete_callback_.Run(this);
    return;
  }
  Forwarder* forwarder = new Forwarder(host_server_data_socket.Pass(),
                                       adb_data_socket.Pass());
  // Forwarder object will self delete after returning.
  forwarder->Start();
}

scoped_ptr<Socket> HostController::CreateSocket() {
  scoped_ptr<Socket> socket(new Socket());
  socket->AddEventFd(global_exit_notifier_fd_);
  socket->AddEventFd(delete_controller_notifier_.receiver_fd());
  return socket.Pass();
}

}  // namespace forwarder2
