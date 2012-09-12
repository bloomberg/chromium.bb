// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_ANDROID_FORWARDER2_DEVICE_CONTROLLER_H_
#define TOOLS_ANDROID_FORWARDER2_DEVICE_CONTROLLER_H_

#include <hash_map>
#include <string>

#include "base/basictypes.h"
#include "base/id_map.h"
#include "base/memory/linked_ptr.h"
#include "tools/android/forwarder2/socket.h"

namespace forwarder2 {

class DeviceListener;

class DeviceController {
 public:
  explicit DeviceController(int exit_notifier_fd);
  ~DeviceController();

  bool Start(const std::string& adb_unix_socket);

 private:
  void KillAllListeners();
  void CleanUpDeadListeners();

  // Map from Port to DeviceListener objects (owns the pointer).
  typedef IDMap<DeviceListener, IDMapOwnPointer> ListenersMap;

  ListenersMap listeners_;
  Socket kickstart_adb_socket_;

  // Used to notify the controller to exit.
  const int exit_notifier_fd_;

  DISALLOW_COPY_AND_ASSIGN(DeviceController);
};

}  // namespace forwarder

#endif  // TOOLS_ANDROID_FORWARDER2_DEVICE_CONTROLLER_H_
