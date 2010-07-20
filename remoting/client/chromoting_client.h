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
class ClientConfig;
class ClientContext;
class InputHandler;

class ChromotingClient : public HostConnection::HostEventCallback {
 public:
  // Objects passed in are not owned by this class.
  ChromotingClient(ClientConfig* config,
                   ClientContext* context,
                   HostConnection* connection,
                   ChromotingView* view,
                   InputHandler* input_handler,
                   CancelableTask* client_done);
  virtual ~ChromotingClient();

  void Start();
  void Stop();
  void ClientDone();

  // Signals that the associated view may need updating.
  virtual void Repaint();

  // Sets the viewport to do display.  The viewport may be larger and/or
  // smaller than the actual image background being displayed.
  //
  // TODO(ajwong): This doesn't make sense to have here.  We're going to have
  // threading isseus since pepper view needs to be called from the main pepper
  // thread synchronously really.
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

  // Handles for chromotocol messages.
  void InitClient(HostMessage* msg);
  void BeginUpdate(HostMessage* msg);
  void HandleUpdate(HostMessage* msg);
  void EndUpdate(HostMessage* msg);

  // The following are not owned by this class.
  ClientConfig* config_;
  ClientContext* context_;
  HostConnection* connection_;
  ChromotingView* view_;
  InputHandler* input_handler_;

  // If non-NULL, this is called when the client is done.
  CancelableTask* client_done_;

  State state_;

  DISALLOW_COPY_AND_ASSIGN(ChromotingClient);
};

}  // namespace remoting

DISABLE_RUNNABLE_METHOD_REFCOUNT(remoting::ChromotingClient);

#endif  // REMOTING_CLIENT_CHROMOTING_CLIENT_H
