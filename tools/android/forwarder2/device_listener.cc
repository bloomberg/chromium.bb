// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/android/forwarder2/device_listener.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#include <string>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "tools/android/forwarder2/command.h"
#include "tools/android/forwarder2/forwarder.h"
#include "tools/android/forwarder2/socket.h"

namespace {
// Timeout for the Listener to be waiting for a new Adb Data Connection.
const int kDeviceListenerTimeoutSeconds = 10;
}

namespace forwarder2 {

DeviceListener::DeviceListener(scoped_ptr<Socket> adb_control_socket, int port)
    : adb_control_socket_(adb_control_socket.Pass()),
      listener_port_(port),
      is_alive_(true),
      must_exit_(false) {
  CHECK(adb_control_socket_.get());
  adb_control_socket_->set_exit_notifier_fd(exit_notifier_.receiver_fd());
  listener_socket_.set_exit_notifier_fd(exit_notifier_.receiver_fd());
  pthread_mutex_init(&adb_data_socket_mutex_, NULL);
  pthread_cond_init(&adb_data_socket_cond_, NULL);
}

DeviceListener::~DeviceListener() {
  CHECK(!is_alive());
  adb_control_socket_->Close();
  listener_socket_.Close();
}

void DeviceListener::SetMustExitLocked() {
  must_exit_ = true;
  exit_notifier_.Notify();
}

void DeviceListener::ForceExit() {
  // Set must_exit and wake up the threads waiting.
  pthread_mutex_lock(&adb_data_socket_mutex_);
  SetMustExitLocked();
  pthread_cond_broadcast(&adb_data_socket_cond_);
  pthread_mutex_unlock(&adb_data_socket_mutex_);
}

bool DeviceListener::WaitForAdbDataSocket() {
  timespec time_to_wait = {};
  time_to_wait.tv_sec = time(NULL) + kDeviceListenerTimeoutSeconds;

  pthread_mutex_lock(&adb_data_socket_mutex_);
  int ret = 0;
  while (!must_exit_ && adb_data_socket_.get() == NULL && ret == 0) {
    ret = pthread_cond_timedwait(&adb_data_socket_cond_,
                                 &adb_data_socket_mutex_,
                                 &time_to_wait);
    if (ret != 0) {
      if (adb_data_socket_.get()) {
        adb_data_socket_->Close();
        adb_data_socket_.reset();
      }
      LOG(ERROR) << "Error while waiting for Adb Data Socket.";
      SetMustExitLocked();
    }
  }
  pthread_mutex_unlock(&adb_data_socket_mutex_);
  if (ret == ETIMEDOUT) {
    LOG(ERROR) << "DeviceListener timeout while waiting for "
               << "the Adb Data Socket for port: " << listener_port_;
  }
  // Check both return value and also if set_must_exit() was called.
  return ret == 0 && !must_exit_ && adb_data_socket_.get();
}

bool DeviceListener::SetAdbDataSocket(scoped_ptr<Socket> adb_data_socket) {
  CHECK(adb_data_socket.get());
  pthread_mutex_lock(&adb_data_socket_mutex_);
  adb_data_socket_.swap(adb_data_socket);
  int ret = pthread_cond_broadcast(&adb_data_socket_cond_);
  if (ret != 0)
    SetMustExitLocked();

  // We must check |must_exit_| while still in the lock, since the ownership of
  // the adb_data_socket_ must be transactionally transferred to the other
  // thread.
  if (must_exit_ && adb_data_socket_.get()) {
    adb_data_socket_->Close();
    adb_data_socket_.reset();
  }
  pthread_mutex_unlock(&adb_data_socket_mutex_);
  return ret == 0;
}

void DeviceListener::RunInternal() {
  while (!must_exit_) {
    scoped_ptr<Socket> device_data_socket(new Socket);
    if (!listener_socket_.Accept(device_data_socket.get())) {
      LOG(WARNING) << "Could not Accept in ListenerSocket.";
      SendCommand(command::ACCEPT_ERROR,
                  listener_port_,
                  adb_control_socket_.get());
      break;
    }
    SendCommand(command::ACCEPT_SUCCESS,
                listener_port_,
                adb_control_socket_.get());
    if (!ReceivedCommand(command::HOST_SERVER_SUCCESS,
                         adb_control_socket_.get())) {
      SendCommand(command::ACK, listener_port_, adb_control_socket_.get());
      LOG(ERROR) << "Host could not connect to server.";
      device_data_socket->Close();
      if (adb_control_socket_->has_error()) {
        LOG(ERROR) << "Adb Control connection lost. "
                   << "Listener port: " << listener_port_;
        break;
      }
      // It can continue if the host forwarder could not connect to the host
      // server but the control connection is still alive (no errors). The
      // device acknowledged that (above), and it can re-try later.
      continue;
    }
    if (!WaitForAdbDataSocket()) {
      LOG(ERROR) << "Device could not receive an Adb Data connection.";
      SendCommand(command::ADB_DATA_SOCKET_ERROR,
                  listener_port_,
                  adb_control_socket_.get());
      device_data_socket->Close();
      continue;
    }
    SendCommand(command::ADB_DATA_SOCKET_SUCCESS,
                listener_port_,
                adb_control_socket_.get());
    CHECK(adb_data_socket_.get());
    // Forwarder object will self delete after returning from the Run() call.
    Forwarder* forwarder = new Forwarder(device_data_socket.Pass(),
                                         adb_data_socket_.Pass());
    forwarder->Start();
  }
}

bool DeviceListener::BindListenerSocket() {
  bool success = listener_socket_.BindTcp("", listener_port_);
  if (success) {
    SendCommand(command::BIND_SUCCESS,
                listener_port_,
                adb_control_socket_.get());
  } else {
    LOG(ERROR) << "Device could not bind and listen to local port.";
    SendCommand(command::BIND_ERROR,
                listener_port_,
                adb_control_socket_.get());
  }
  return success;
}

void DeviceListener::Run() {
  if (BindListenerSocket())
    RunInternal();
  adb_control_socket_->Close();
  listener_socket_.Close();
  // This must be the last statement of the Run() method of the DeviceListener
  // class, since the main thread checks if this thread is dead and deletes
  // the object. It will be a race condition otherwise.
  is_alive_ = false;
}

}  // namespace forwarder
