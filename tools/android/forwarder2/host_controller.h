// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_ANDROID_FORWARDER2_HOST_CONTROLLER_H_
#define TOOLS_ANDROID_FORWARDER2_HOST_CONTROLLER_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread.h"
#include "tools/android/forwarder2/pipe_notifier.h"
#include "tools/android/forwarder2/socket.h"

namespace forwarder2 {

class HostProxy;

class HostController {
 public:
  // Callback used for self-deletion that lets the client perform some cleanup
  // work before deleting the HostController instance.
  typedef base::Callback<void (HostController*)> DeleteCallback;

  // If |device_port| is zero, it dynamically allocates a port.
  HostController(int device_port,
                 const std::string& forward_to_host,
                 int forward_to_host_port,
                 int adb_port,
                 int exit_notifier_fd,
                 const DeleteCallback& delete_callback);

  ~HostController();

  // Connects to the device forwarder app and sets the |device_port_| to the
  // dynamically allocated port (when the provided |device_port| is zero).
  // Returns true on success.  Clients must call Connect() before calling
  // Start().
  bool Connect();

  // Starts the internal controller thread.
  void Start();

  int adb_port() const { return adb_port_; }

  // Gets the current device allocated port. Must be called after Connect().
  int device_port() const { return device_port_; }

 private:
  void ThreadHandler();

  void StartForwarder(scoped_ptr<Socket> host_server_data_socket_ptr);

  // Helper method that creates a socket and adds the appropriate event file
  // descriptors.
  scoped_ptr<Socket> CreateSocket();

  Socket adb_control_socket_;
  int device_port_;
  const std::string forward_to_host_;
  const int forward_to_host_port_;
  const int adb_port_;
  // Used to notify the controller when the process is killed.
  const int global_exit_notifier_fd_;
  // Used to let the client delete the instance in case an error happened.
  const DeleteCallback delete_callback_;
  // Used to cancel the pending blocking IO operations when the host controller
  // instance is deleted.
  PipeNotifier delete_controller_notifier_;
  bool ready_;
  base::Thread thread_;

  DISALLOW_COPY_AND_ASSIGN(HostController);
};

}  // namespace forwarder2

#endif  // TOOLS_ANDROID_FORWARDER2_HOST_CONTROLLER_H_
