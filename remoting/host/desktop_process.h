// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_DESKTOP_PROCESS_H_
#define REMOTING_HOST_DESKTOP_PROCESS_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "ipc/ipc_listener.h"

namespace IPC {
class ChannelProxy;
}  // namespace IPC

namespace remoting {

class DesktopProcess : public IPC::Listener {
 public:
  explicit DesktopProcess(const std::string& daemon_channel_name);
  ~DesktopProcess();

  // IPC::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnChannelConnected(int32 peer_pid) OVERRIDE;
  virtual void OnChannelError() OVERRIDE;

  // Runs the desktop process.
  int Run();

 private:
  // IPC channel connecting the desktop process with the daemon process.
  scoped_ptr<IPC::ChannelProxy> daemon_channel_;

  // Name of the IPC channel connecting the desktop process with the daemon
  // process.
  std::string daemon_channel_name_;

  DISALLOW_COPY_AND_ASSIGN(DesktopProcess);
};

}  // namespace remoting

#endif  // REMOTING_HOST_DESKTOP_PROCESS_H_
