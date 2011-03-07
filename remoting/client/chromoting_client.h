// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ChromotingClient is the controller for the Client implementation.

#ifndef REMOTING_CLIENT_CHROMOTING_CLIENT_H
#define REMOTING_CLIENT_CHROMOTING_CLIENT_H

#include <list>

#include "base/task.h"
#include "remoting/client/client_config.h"
#include "remoting/client/chromoting_view.h"
#include "remoting/protocol/client_stub.h"
#include "remoting/protocol/connection_to_host.h"
#include "remoting/protocol/input_stub.h"
#include "remoting/protocol/video_stub.h"
#include "remoting/jingle_glue/xmpp_proxy.h"

class MessageLoop;

namespace remoting {

namespace protocol {
class LocalLoginStatus;
class NotifyResolutionRequest;
}  // namespace protocol

class ClientContext;
class InputHandler;
class RectangleUpdateDecoder;

// TODO(sergeyu): Move VideoStub implementation to RectangleUpdateDecoder.
class ChromotingClient : public protocol::ConnectionToHost::HostEventCallback,
                         public protocol::ClientStub,
                         public protocol::VideoStub {
 public:
  // Objects passed in are not owned by this class.
  ChromotingClient(const ClientConfig& config,
                   ClientContext* context,
                   protocol::ConnectionToHost* connection,
                   ChromotingView* view,
                   RectangleUpdateDecoder* rectangle_decoder,
                   InputHandler* input_handler,
                   CancelableTask* client_done);
  virtual ~ChromotingClient();

  void Start();
  void StartSandboxed(scoped_refptr<XmppProxy> xmpp_proxy,
                      const std::string& your_jid,
                      const std::string& host_jid);
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

  // ConnectionToHost::HostEventCallback implementation.
  virtual void OnConnectionOpened(protocol::ConnectionToHost* conn);
  virtual void OnConnectionClosed(protocol::ConnectionToHost* conn);
  virtual void OnConnectionFailed(protocol::ConnectionToHost* conn);

  // ClientStub implementation.
  virtual void NotifyResolution(const protocol::NotifyResolutionRequest* msg,
                                Task* done);
  virtual void BeginSessionResponse(const protocol::LocalLoginStatus* msg,
                                    Task* done);

  // VideoStub implementation.
  virtual void ProcessVideoPacket(const VideoPacket* packet, Task* done);
  virtual int GetPendingPackets();

 private:
  struct QueuedVideoPacket {
    QueuedVideoPacket(const VideoPacket* packet, Task* done)
        : packet(packet), done(done) {
    }
    const VideoPacket* packet;
    Task* done;
  };

  MessageLoop* message_loop();

  // Initializes connection.
  void Initialize();

  // Convenience method for modifying the state on this object's message loop.
  void SetConnectionState(ConnectionState s);

  // If a packet is not being processed, dispatches a single message from the
  // |received_packets_| queue.
  void DispatchPacket();

  void OnPacketDone();

  // The following are not owned by this class.
  ClientConfig config_;
  ClientContext* context_;
  protocol::ConnectionToHost* connection_;
  ChromotingView* view_;
  RectangleUpdateDecoder* rectangle_decoder_;
  InputHandler* input_handler_;

  // If non-NULL, this is called when the client is done.
  CancelableTask* client_done_;

  ConnectionState state_;

  // Contains all video packets that have been received, but have not yet been
  // processed.
  //
  // Used to serialize sending of messages to the client.
  std::list<QueuedVideoPacket> received_packets_;

  // True if a message is being processed. Can be used to determine if it is
  // safe to dispatch another message.
  bool packet_being_processed_;

  DISALLOW_COPY_AND_ASSIGN(ChromotingClient);
};

}  // namespace remoting

DISABLE_RUNNABLE_METHOD_REFCOUNT(remoting::ChromotingClient);

#endif  // REMOTING_CLIENT_CHROMOTING_CLIENT_H
