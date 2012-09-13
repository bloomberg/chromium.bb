// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_ANDROID_FORWARDER2_HOST_CONTROLLER_H_
#define TOOLS_ANDROID_FORWARDER2_HOST_CONTROLLER_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "tools/android/forwarder2/socket.h"
#include "tools/android/forwarder2/thread.h"

namespace forwarder2 {

class HostProxy;

class HostController : public Thread {
 public:
  HostController(int device_port,
                 const std::string& forward_to_host,
                 int forward_to_host_port,
                 int adb_port,
                 int exit_notifier_fd);
  virtual ~HostController();

 protected:
  // Thread:
  virtual void Run() OVERRIDE;

 private:
  void StartForwarder(scoped_ptr<Socket> host_server_data_socket_ptr);

  Socket adb_control_socket_;
  const int device_port_;
  const std::string forward_to_host_;
  const int forward_to_host_port_;
  const int adb_port_;

  // Used to notify the controller to exit.
  const int exit_notifier_fd_;

  DISALLOW_COPY_AND_ASSIGN(HostController);
};

}  // namespace forwarder2

#endif  // TOOLS_ANDROID_FORWARDER2_HOST_CONTROLLER_H_
