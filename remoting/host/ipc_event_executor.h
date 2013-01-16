// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_IPC_EVENT_EXECUTOR_H_
#define REMOTING_HOST_IPC_EVENT_EXECUTOR_H_

#include "base/memory/ref_counted.h"
#include "remoting/host/event_executor.h"
#include "remoting/proto/event.pb.h"

namespace remoting {

class DesktopSessionProxy;

// Routes EventExecutor calls though the IPC channel to the desktop session
// agent running in the desktop integration process.
class IpcEventExecutor : public EventExecutor {
 public:
  explicit IpcEventExecutor(
      scoped_refptr<DesktopSessionProxy> desktop_session_proxy);
  virtual ~IpcEventExecutor();

  // ClipboardStub interface.
  virtual void InjectClipboardEvent(
      const protocol::ClipboardEvent& event) OVERRIDE;

  // InputStub interface.
  virtual void InjectKeyEvent(const protocol::KeyEvent& event) OVERRIDE;
  virtual void InjectMouseEvent(const protocol::MouseEvent& event) OVERRIDE;

  // EventExecutor interface.
  virtual void Start(
      scoped_ptr<protocol::ClipboardStub> client_clipboard) OVERRIDE;

 private:
  // Wraps the IPC channel to the desktop process.
  scoped_refptr<DesktopSessionProxy> desktop_session_proxy_;

  DISALLOW_COPY_AND_ASSIGN(IpcEventExecutor);
};

}  // namespace remoting

#endif  // REMOTING_HOST_IPC_EVENT_EXECUTOR_H_
