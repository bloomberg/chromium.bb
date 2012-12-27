// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_IPC_DESKTOP_ENVIRONMENT_H_
#define REMOTING_HOST_IPC_DESKTOP_ENVIRONMENT_H_

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "ipc/ipc_platform_file.h"
#include "remoting/host/desktop_environment.h"

namespace base {
class SingleThreadTaskRunner;
}  // base

namespace remoting {

class DesktopSessionConnector;
class DesktopSessionProxy;

// A variant of desktop environment integrating with the desktop by means of
// a helper process and talking to that process via IPC.
class IpcDesktopEnvironment : public DesktopEnvironment {
 public:
  // |desktop_session_connector| is used to bind the IpcDesktopEnvironment to
  // a desktop session, to be notified with a new IPC channel every time
  // the desktop process is changed. |desktop_session_connector| must outlive
  // |this|.  |client| specifies the client session owning |this|.
  IpcDesktopEnvironment(
      scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> network_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
      DesktopSessionConnector* desktop_session_connector,
      scoped_refptr<DesktopSessionProxy> desktop_session_proxy);
  virtual ~IpcDesktopEnvironment();

  virtual void Start(
      scoped_ptr<protocol::ClipboardStub> client_clipboard,
      const std::string& client_jid,
      const base::Closure& disconnect_callback) OVERRIDE;

 private:
  // Used for IPC I/O.
  scoped_refptr<base::SingleThreadTaskRunner> network_task_runner_;

  DesktopSessionConnector* desktop_session_connector_;

  scoped_refptr<DesktopSessionProxy> desktop_session_proxy_;

  // True if |this| has been connected to a desktop session.
  bool connected_;

  DISALLOW_COPY_AND_ASSIGN(IpcDesktopEnvironment);
};

}  // namespace remoting

#endif  // REMOTING_HOST_IPC_DESKTOP_ENVIRONMENT_H_
