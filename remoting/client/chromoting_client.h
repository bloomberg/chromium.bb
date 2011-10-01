// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ChromotingClient is the controller for the Client implementation.

#ifndef REMOTING_CLIENT_CHROMOTING_CLIENT_H_
#define REMOTING_CLIENT_CHROMOTING_CLIENT_H_

#include <list>

#include "base/task.h"
#include "base/time.h"
#include "remoting/base/scoped_thread_proxy.h"
#include "remoting/client/client_config.h"
#include "remoting/client/chromoting_stats.h"
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
                   Task* client_done);
  virtual ~ChromotingClient();

  void Start(scoped_refptr<XmppProxy> xmpp_proxy);
  void Stop(const base::Closure& shutdown_task);
  void ClientDone();

  // Return the stats recorded by this client.
  ChromotingStats* GetStats();

  // Signals that the associated view may need updating.
  virtual void Repaint();

  // ConnectionToHost::HostEventCallback implementation.
  virtual void OnConnectionState(
      protocol::ConnectionToHost::State state,
      protocol::ConnectionToHost::Error error) OVERRIDE;

  // ClientStub implementation.
  virtual void BeginSessionResponse(const protocol::LocalLoginStatus* msg,
                                    Task* done) OVERRIDE;

  // VideoStub implementation.
  virtual void ProcessVideoPacket(const VideoPacket* packet,
                                  Task* done) OVERRIDE;
  virtual int GetPendingPackets() OVERRIDE;

 private:
  struct QueuedVideoPacket {
    QueuedVideoPacket(const VideoPacket* packet, Task* done)
        : packet(packet), done(done) {
    }
    const VideoPacket* packet;
    Task* done;
  };

  base::MessageLoopProxy* message_loop();

  // Initializes connection.
  void Initialize();

  // If a packet is not being processed, dispatches a single message from the
  // |received_packets_| queue.
  void DispatchPacket();

  // Callback method when a VideoPacket is processed.
  // If |last_packet| is true then |decode_start| contains the timestamp when
  // the packet will start to be processed.
  void OnPacketDone(bool last_packet, base::Time decode_start);

  void OnDisconnected(const base::Closure& shutdown_task);

  // The following are not owned by this class.
  ClientConfig config_;
  ClientContext* context_;
  protocol::ConnectionToHost* connection_;
  ChromotingView* view_;
  RectangleUpdateDecoder* rectangle_decoder_;
  InputHandler* input_handler_;

  // If non-NULL, this is called when the client is done.
  Task* client_done_;

  // Contains all video packets that have been received, but have not yet been
  // processed.
  //
  // Used to serialize sending of messages to the client.
  std::list<QueuedVideoPacket> received_packets_;

  // True if a message is being processed. Can be used to determine if it is
  // safe to dispatch another message.
  bool packet_being_processed_;

  // Record the statistics of the connection.
  ChromotingStats stats_;

  // Keep track of the last sequence number bounced back from the host.
  int64 last_sequence_number_;

  ScopedThreadProxy thread_proxy_;

  DISALLOW_COPY_AND_ASSIGN(ChromotingClient);
};

}  // namespace remoting

DISABLE_RUNNABLE_METHOD_REFCOUNT(remoting::ChromotingClient);

#endif  // REMOTING_CLIENT_CHROMOTING_CLIENT_H_
