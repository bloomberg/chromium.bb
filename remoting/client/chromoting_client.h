// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ChromotingClient is the controller for the Client implementation.

#ifndef REMOTING_CLIENT_CHROMOTING_CLIENT_H_
#define REMOTING_CLIENT_CHROMOTING_CLIENT_H_

#include <list>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time.h"
#include "remoting/client/client_config.h"
#include "remoting/client/chromoting_stats.h"
#include "remoting/client/chromoting_view.h"
#include "remoting/protocol/audio_stub.h"
#include "remoting/protocol/client_stub.h"
#include "remoting/protocol/clipboard_stub.h"
#include "remoting/protocol/connection_to_host.h"
#include "remoting/protocol/input_stub.h"
#include "remoting/protocol/video_stub.h"
#include "remoting/jingle_glue/xmpp_proxy.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace remoting {

namespace protocol {
class TransportFactory;
}  // namespace protocol

class AudioPlayer;
class RectangleUpdateDecoder;

// TODO(sergeyu): Move VideoStub implementation to RectangleUpdateDecoder.
class ChromotingClient : public protocol::ConnectionToHost::HostEventCallback,
                         public protocol::ClientStub,
                         public protocol::VideoStub,
                         public protocol::AudioStub {
 public:
  // Objects passed in are not owned by this class.
  ChromotingClient(const ClientConfig& config,
                   scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                   protocol::ConnectionToHost* connection,
                   ChromotingView* view,
                   RectangleUpdateDecoder* rectangle_decoder,
                   AudioPlayer* audio_player);

  virtual ~ChromotingClient();

  // Start/stop the client. Must be called on the main thread.
  void Start(scoped_refptr<XmppProxy> xmpp_proxy,
             scoped_ptr<protocol::TransportFactory> transport_factory);
  void Stop(const base::Closure& shutdown_task);

  // Return the stats recorded by this client.
  ChromotingStats* GetStats();

  // ClipboardStub implementation for receiving clipboard data from host.
  virtual void InjectClipboardEvent(const protocol::ClipboardEvent& event)
      OVERRIDE;

  // CursorShapeStub implementation for receiving cursor shape updates.
  virtual void SetCursorShape(const protocol::CursorShapeInfo& cursor_shape)
      OVERRIDE;

  // ConnectionToHost::HostEventCallback implementation.
  virtual void OnConnectionState(
      protocol::ConnectionToHost::State state,
      protocol::ErrorCode error) OVERRIDE;

  // VideoStub implementation.
  virtual void ProcessVideoPacket(scoped_ptr<VideoPacket> packet,
                                  const base::Closure& done) OVERRIDE;
  virtual int GetPendingVideoPackets() OVERRIDE;

  // AudioStub implementation.
  virtual void ProcessAudioPacket(scoped_ptr<AudioPacket> packet,
                                  const base::Closure& done) OVERRIDE;

 private:
  struct QueuedVideoPacket {
    QueuedVideoPacket(scoped_ptr<VideoPacket> packet,
                      const base::Closure& done);
    ~QueuedVideoPacket();
    VideoPacket* packet;
    base::Closure done;
  };

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
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  protocol::ConnectionToHost* connection_;
  ChromotingView* view_;
  RectangleUpdateDecoder* rectangle_decoder_;
  AudioPlayer* audio_player_;

  // If non-NULL, this is called when the client is done.
  base::Closure client_done_;

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

  // WeakPtr used to avoid tasks accessing the client after it is deleted.
  base::WeakPtrFactory<ChromotingClient> weak_factory_;
  base::WeakPtr<ChromotingClient> weak_ptr_;

  DISALLOW_COPY_AND_ASSIGN(ChromotingClient);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_CHROMOTING_CLIENT_H_
