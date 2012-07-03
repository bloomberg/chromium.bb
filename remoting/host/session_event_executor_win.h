// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SESSION_EVENT_EXECUTOR_WIN_H_
#define REMOTING_HOST_SESSION_EVENT_EXECUTOR_WIN_H_

#include <set>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ipc/ipc_channel.h"

#include "remoting/host/event_executor.h"
#include "remoting/host/scoped_thread_desktop_win.h"
#include "remoting/protocol/host_event_stub.h"

namespace base {
class SingleThreadTaskRunner;
} // namespace base

namespace IPC {
class ChannelProxy;
} // namespace IPC

namespace remoting {

class SessionEventExecutorWin : public EventExecutor,
                                public IPC::Listener {
 public:
  SessionEventExecutorWin(
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      scoped_ptr<EventExecutor> nested_executor);
  ~SessionEventExecutorWin();

  // EventExecutor implementation.
  virtual void OnSessionStarted(
      scoped_ptr<protocol::ClipboardStub> client_clipboard) OVERRIDE;
  virtual void OnSessionFinished() OVERRIDE;

  // protocol::HostStub implementation.
  virtual void InjectClipboardEvent(
      const protocol::ClipboardEvent& event) OVERRIDE;
  virtual void InjectKeyEvent(const protocol::KeyEvent& event) OVERRIDE;
  virtual void InjectMouseEvent(const protocol::MouseEvent& event) OVERRIDE;

  // IPC::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

 private:
  // Switches to the desktop receiving a user input if different from
  // the current one.
  void SwitchToInputDesktop();

  // Pointer to the next event executor.
  scoped_ptr<EventExecutor> nested_executor_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  ScopedThreadDesktopWin desktop_;

  // The Chromoting IPC channel connecting the host with the service.
  scoped_ptr<IPC::ChannelProxy> chromoting_channel_;

  // Keys currently pressed by the client, used to detect Ctrl-Alt-Del.
  std::set<uint32> pressed_keys_;

  base::WeakPtrFactory<SessionEventExecutorWin> weak_ptr_factory_;
  base::WeakPtr<SessionEventExecutorWin> weak_ptr_;

  DISALLOW_COPY_AND_ASSIGN(SessionEventExecutorWin);
};

}  // namespace remoting

#endif  // REMOTING_HOST_SESSION_EVENT_EXECUTOR_WIN_H_
