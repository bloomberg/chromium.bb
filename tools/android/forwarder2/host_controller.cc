// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/android/forwarder2/host_controller.h"

#include <string>

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
                               int exit_notifier_fd)
    : device_port_(device_port),
      forward_to_host_(forward_to_host),
      forward_to_host_port_(forward_to_host_port),
      adb_port_(adb_port),
      exit_notifier_fd_(exit_notifier_fd) {
  adb_control_socket_.set_exit_notifier_fd(exit_notifier_fd);
}

HostController::~HostController() {
}

void HostController::StartForwarder(
    scoped_ptr<Socket> host_server_data_socket) {
  scoped_ptr<Socket> adb_data_socket(new Socket);
  if (!adb_data_socket->ConnectTcp("", adb_port_)) {
    LOG(ERROR) << "Could not Connect AdbDataSocket on port: "
               << adb_port_;
    return;
  }
  // Open the Adb data connection, and send a command with the
  // |device_forward_port| as a way for the device to identify the connection.
  SendCommand(command::DATA_CONNECTION,
              device_port_,
              adb_data_socket.get());

  // Check that the device received the new Adb Data Connection.  Observe that
  // this check is done through the |adb_control_socket_| that is handled in the
  // DeviceListener thread just after the call to WaitForAdbDataSocket().
  if (!ReceivedCommand(command::ADB_DATA_SOCKET_SUCCESS,
                       &adb_control_socket_)) {
    LOG(ERROR) << "Device could not handle the new Adb Data Connection.";
    host_server_data_socket->Close();
    adb_data_socket->Close();
    return;
  }
  Forwarder* forwarder = new Forwarder(host_server_data_socket.Pass(),
                                       adb_data_socket.Pass());
  // Forwarder object will self delete after returning.
  forwarder->Start();
}

void HostController::Run() {
  if (!adb_control_socket_.ConnectTcp("", adb_port_)) {
    LOG(ERROR) << "Could not Connect HostController socket on port: "
               << adb_port_;
    return;
  }
  // Send the command to the device start listening to the "device_forward_port"
  SendCommand(command::LISTEN, device_port_, &adb_control_socket_);
  if (!ReceivedCommand(command::BIND_SUCCESS, &adb_control_socket_)) {
    LOG(ERROR) << "Device binding error using port " << device_port_;
    adb_control_socket_.Close();
    return;
  }
  while (true) {
    if (!ReceivedCommand(command::ACCEPT_SUCCESS,
                         &adb_control_socket_)) {
      LOG(ERROR) << "Device socket error on accepting using port "
                 << device_port_;
      break;
    }
    // Try to connect to host server.
    scoped_ptr<Socket> host_server_data_socket(new Socket);
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
      } else {
        break;
      }
    }
    SendCommand(command::HOST_SERVER_SUCCESS,
                device_port_,
                &adb_control_socket_);
    StartForwarder(host_server_data_socket.Pass());
  }
  adb_control_socket_.Close();
}

}  // namespace forwarder2
