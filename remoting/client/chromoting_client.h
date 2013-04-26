// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ChromotingClient is the controller for the Client implementation.

#ifndef REMOTING_CLIENT_CHROMOTING_CLIENT_H_
#define REMOTING_CLIENT_CHROMOTING_CLIENT_H_

#include <string>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "remoting/client/client_config.h"
#include "remoting/client/chromoting_stats.h"
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

class AudioDecodeScheduler;
class AudioPlayer;
class ClientContext;
class ClientUserInterface;
class FrameConsumerProxy;
class FrameProducer;
class RectangleUpdateDecoder;

class ChromotingClient : public protocol::ConnectionToHost::HostEventCallback,
                         public protocol::ClientStub {
 public:
  ChromotingClient(const ClientConfig& config,
                   ClientContext* client_context,
                   protocol::ConnectionToHost* connection,
                   ClientUserInterface* user_interface,
                   scoped_refptr<FrameConsumerProxy> frame_consumer,
                   scoped_ptr<AudioPlayer> audio_player);

  virtual ~ChromotingClient();

  // Start/stop the client. Must be called on the main thread.
  void Start(scoped_refptr<XmppProxy> xmpp_proxy,
             scoped_ptr<protocol::TransportFactory> transport_factory);
  void Stop(const base::Closure& shutdown_task);

  FrameProducer* GetFrameProducer();

  // Return the stats recorded by this client.
  ChromotingStats* GetStats();

  // ClientStub implementation.
  virtual void SetCapabilities(
      const protocol::Capabilities& capabilities) OVERRIDE;

  // ClipboardStub implementation for receiving clipboard data from host.
  virtual void InjectClipboardEvent(
      const protocol::ClipboardEvent& event) OVERRIDE;

  // CursorShapeStub implementation for receiving cursor shape updates.
  virtual void SetCursorShape(
      const protocol::CursorShapeInfo& cursor_shape) OVERRIDE;

  // ConnectionToHost::HostEventCallback implementation.
  virtual void OnConnectionState(
      protocol::ConnectionToHost::State state,
      protocol::ErrorCode error) OVERRIDE;
  virtual void OnConnectionReady(bool ready) OVERRIDE;

 private:
  // Called when the connection is authenticated.
  void OnAuthenticated();

  // Called when all channels are connected.
  void OnChannelsConnected();

  void OnDisconnected(const base::Closure& shutdown_task);

  // The following are not owned by this class.
  ClientConfig config_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  protocol::ConnectionToHost* connection_;
  ClientUserInterface* user_interface_;
  scoped_refptr<RectangleUpdateDecoder> rectangle_decoder_;

  scoped_ptr<AudioDecodeScheduler> audio_decode_scheduler_;

  // If non-NULL, this is called when the client is done.
  base::Closure client_done_;

  // The set of all capabilities supported by the host.
  std::string host_capabilities_;

  // True if |protocol::Capabilities| message has been received.
  bool host_capabilities_received_;

  // Record the statistics of the connection.
  ChromotingStats stats_;

  // WeakPtr used to avoid tasks accessing the client after it is deleted.
  base::WeakPtrFactory<ChromotingClient> weak_factory_;
  base::WeakPtr<ChromotingClient> weak_ptr_;

  DISALLOW_COPY_AND_ASSIGN(ChromotingClient);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_CHROMOTING_CLIENT_H_
