// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_ANDROID_FORWARDER2_DEVICE_LISTENER_H_
#define TOOLS_ANDROID_FORWARDER2_DEVICE_LISTENER_H_

#include <pthread.h>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "tools/android/forwarder2/pipe_notifier.h"
#include "tools/android/forwarder2/socket.h"
#include "tools/android/forwarder2/thread.h"

namespace forwarder2 {

class Forwarder;

class DeviceListener : public Thread {
 public:
  DeviceListener(scoped_ptr<Socket> adb_control_socket, int port);
  virtual ~DeviceListener();

  bool WaitForAdbDataSocket();

  bool SetAdbDataSocket(scoped_ptr<Socket> adb_data_socket);

  // |is_alive_| is set only on creation and written once when Run() terminates.
  // So even in case of a race condition, the worst that could happen is for the
  // main thread to see the listener alive when it isn't. And also, this is not
  // a problem since the main thread checks the liveliness of the listeners in a
  // loop.
  bool is_alive() const { return is_alive_; }
  void ForceExit();

 protected:
  // Thread:
  virtual void Run() OVERRIDE;

 private:
  bool BindListenerSocket();
  void RunInternal();

  // Must be called after successfully acquired mutex.
  void SetMustExitLocked();

  // The listener socket for sending control commands.
  scoped_ptr<Socket> adb_control_socket_;

  // The local device listener socket for accepting connections from the local
  // port (listener_port_).
  Socket listener_socket_;

  // This is the adb connection to transport the actual data, used for creating
  // the forwarder. Ownership transferred to the Forwarder.
  scoped_ptr<Socket> adb_data_socket_;

  const int listener_port_;
  pthread_mutex_t adb_data_socket_mutex_;
  pthread_cond_t adb_data_socket_cond_;
  bool is_alive_;
  bool must_exit_;

  // Used for the listener thread to be notified from ForceExit() which is
  // called from the main thread. We have one notifier per Listener thread since
  // each Listener thread may be requested to exit for different reasons
  // independently from each other and independent from the main program,
  // ex. when the host requests to forward/listen the same port again.  Both the
  // |adb_control_socket_| and |listener_socket_| must share the same receiver
  // file descriptor from |exit_notifier_| and it is set in the constructor.
  PipeNotifier exit_notifier_;

  DISALLOW_COPY_AND_ASSIGN(DeviceListener);
};

}  // namespace forwarder

#endif  // TOOLS_ANDROID_FORWARDER2_DEVICE_LISTENER_H_
