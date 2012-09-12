// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/android/forwarder2/device_controller.h"

#include <errno.h>
#include <stdlib.h>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/safe_strerror_posix.h"
#include "tools/android/forwarder2/command.h"
#include "tools/android/forwarder2/device_listener.h"
#include "tools/android/forwarder2/socket.h"

namespace forwarder2 {

DeviceController::DeviceController(int exit_notifier_fd)
    : exit_notifier_fd_(exit_notifier_fd) {
  kickstart_adb_socket_.set_exit_notifier_fd(exit_notifier_fd);
}

DeviceController::~DeviceController() {
  KillAllListeners();
  CleanUpDeadListeners();
  CHECK_EQ(0, listeners_.size());
}

void DeviceController::CleanUpDeadListeners() {
  // Clean up dead listeners.
  for (ListenersMap::iterator it(&listeners_); !it.IsAtEnd(); it.Advance()) {
    if (!it.GetCurrentValue()->is_alive())
      // Remove deletes the listener.
      listeners_.Remove(it.GetCurrentKey());
  }
}

void DeviceController::KillAllListeners() {
  for (ListenersMap::iterator it(&listeners_); !it.IsAtEnd(); it.Advance())
    it.GetCurrentValue()->ForceExit();
  for (ListenersMap::iterator it(&listeners_); !it.IsAtEnd(); it.Advance()) {
    it.GetCurrentValue()->Join();
    CHECK(!it.GetCurrentValue()->is_alive());
  }
}

bool DeviceController::Start(const std::string& adb_unix_socket) {
  if (!kickstart_adb_socket_.BindUnix(adb_unix_socket,
                                      true /* abstract */)) {
    LOG(ERROR) << "Could not BindAndListen DeviceController socket on port: "
               << adb_unix_socket;
    return false;
  }
  while (true) {
    CleanUpDeadListeners();
    scoped_ptr<Socket> socket(new Socket);
    if (!kickstart_adb_socket_.Accept(socket.get())) {
      LOG(ERROR) << "Could not Accept DeviceController socket: "
                 << safe_strerror(errno);
      break;
    }
    // So that |socket| doesn't block on read if it has notifications.
    socket->set_exit_notifier_fd(exit_notifier_fd_);
    int port;
    command::Type command;
    if (!ReadCommand(socket.get(), &port, &command)) {
      LOG(ERROR) << "Invalid command received.";
      socket->Close();
      continue;
    }
    DeviceListener* listener = listeners_.Lookup(port);
    switch (command) {
      case command::LISTEN: {
        if (listener != NULL) {
          LOG(WARNING) << "Already forwarding port " << port
                       << ". Attempting to restart the listener.\n";
          listener->ForceExit();
          listener->Join();
          CHECK(!listener->is_alive());
          // Remove deletes the listener object.
          listeners_.Remove(port);
        }
        // Listener object will be deleted by the CleanUpDeadListeners method.
        DeviceListener* new_listener = new DeviceListener(socket.Pass(), port);
        listeners_.AddWithID(new_listener, port);
        new_listener->Start();
        break;
      }
      case command::DATA_CONNECTION:
        if (listener == NULL) {
          LOG(ERROR) << "Data Connection command received, but "
                     << "listener has not been set up yet for port " << port;
          // After this point it is assumed that, once we close our Adb Data
          // socket, the Adb forwarder command will propagate the closing of
          // sockets all the way to the host side.
          socket->Close();
          continue;
        } else if (!listener->SetAdbDataSocket(socket.Pass())) {
          LOG(ERROR) << "Could not set Adb Data Socket for port: " << port;
          // Same assumption as above, but in this case the socket is closed
          // inside SetAdbDataSocket.
          continue;
        }
        break;
      default:
        // TODO(felipeg): add a KillAllListeners command.
        LOG(ERROR) << "Invalid command received. Port: " << port
                   << " Command: " << command;
        socket->Close();
        continue;
        break;
    }
  }
  KillAllListeners();
  CleanUpDeadListeners();
  kickstart_adb_socket_.Close();

  return true;
}

}  // namespace forwarder
