// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ChromotingClient is the controller for the Client implementation.

#ifndef REMOTING_CLIENT_CHROMOTING_CLIENT_H
#define REMOTING_CLIENT_CHROMOTING_CLIENT_H

#include "base/task.h"
#include "remoting/client/host_connection.h"

class MessageLoop;

namespace remoting {

class ChromotingView;

class ChromotingClient : public HostConnection::HostEventCallback {
 public:
  ChromotingClient(MessageLoop* message_loop,
                   HostConnection* connection,
                   ChromotingView* view);
  virtual ~ChromotingClient();

  // Signals that the associated view may need updating.
  virtual void Repaint();

  // Sets the viewport to do display.  The viewport may be larger and/or
  // smaller than the actual image background being displayed.
  virtual void SetViewport(int x, int y, int width, int height);

  // HostConnection::HostEventCallback implementation.
  virtual void HandleMessages(HostConnection* conn, HostMessageList* messages);
  virtual void OnConnectionOpened(HostConnection* conn);
  virtual void OnConnectionClosed(HostConnection* conn);
  virtual void OnConnectionFailed(HostConnection* conn);

 private:
  enum State {
    CREATED,
    CONNECTED,
    DISCONNECTED,
    FAILED,
  };

  MessageLoop* message_loop();

  // Convenience method for modifying the state on this object's message loop.
  void SetState(State s);

  // TODO(ajwong): Do all of these methods need to run on the client's thread?
  void DoSetState(State s);
  void DoRepaint();
  void DoSetViewport(int x, int y, int width, int height);

  // Handles for chromotocol messages.
  void DoInitClient(HostMessage* msg);
  void DoBeginUpdate(HostMessage* msg);
  void DoHandleUpdate(HostMessage* msg);
  void DoEndUpdate(HostMessage* msg);

  MessageLoop* message_loop_;

  State state_;

  // Connection to views and hosts.  Not owned.
  HostConnection* host_connection_;
  ChromotingView* view_;

  DISALLOW_COPY_AND_ASSIGN(ChromotingClient);
};

}  // namespace remoting

DISABLE_RUNNABLE_METHOD_REFCOUNT(remoting::ChromotingClient);

#endif  // REMOTING_CLIENT_CHROMOTING_CLIENT_H
