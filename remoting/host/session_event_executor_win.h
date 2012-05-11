// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SESSION_EVENT_EXECUTOR_WIN_H_
#define REMOTING_HOST_SESSION_EVENT_EXECUTOR_WIN_H_

#include <set>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "ipc/ipc_channel.h"

#include "remoting/host/event_executor.h"
#include "remoting/host/scoped_thread_desktop_win.h"
#include "remoting/protocol/host_event_stub.h"

class MessageLoop;

namespace base {
class MessageLoopProxy;
} // namespace base

namespace IPC {
class ChannelProxy;
} // namespace IPC

namespace remoting {

class SessionEventExecutorWin : public EventExecutor,
                                public IPC::Channel::Listener {
 public:
  SessionEventExecutorWin(MessageLoop* message_loop,
                          base::MessageLoopProxy* io_message_loop,
                          scoped_ptr<EventExecutor> nested_executor);
  ~SessionEventExecutorWin();

  // EventExecutor implementation.
  virtual void OnSessionStarted() OVERRIDE;
  virtual void OnSessionFinished() OVERRIDE;

  // protocol::HostStub implementation.
  virtual void InjectClipboardEvent(
      const protocol::ClipboardEvent& event) OVERRIDE;
  virtual void InjectKeyEvent(const protocol::KeyEvent& event) OVERRIDE;
  virtual void InjectMouseEvent(const protocol::MouseEvent& event) OVERRIDE;

  // IPC::Channel::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

 private:
  // Switches to the desktop receiving a user input if different from
  // the current one.
  void SwitchToInputDesktop();

  // Pointer to the next event executor.
  scoped_ptr<EventExecutor> nested_executor_;

  MessageLoop* message_loop_;

  ScopedThreadDesktopWin desktop_;

  // The Chromoting IPC channel connecting the host with the service.
  scoped_ptr<IPC::ChannelProxy> chromoting_channel_;

  // Keys currently pressed by the client, used to detect Ctrl-Alt-Del.
  std::set<uint32> pressed_keys_;

  DISALLOW_COPY_AND_ASSIGN(SessionEventExecutorWin);
};

}  // namespace remoting

#endif  // REMOTING_HOST_SESSION_EVENT_EXECUTOR_WIN_H_
