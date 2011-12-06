// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/chromoting_client.h"

#include "base/bind.h"
#include "remoting/client/chromoting_view.h"
#include "remoting/client/client_context.h"
#include "remoting/client/input_handler.h"
#include "remoting/client/rectangle_update_decoder.h"
#include "remoting/protocol/connection_to_host.h"
#include "remoting/protocol/session_config.h"

namespace remoting {

ChromotingClient::QueuedVideoPacket::QueuedVideoPacket(
    const VideoPacket* packet, const base::Closure& done)
    : packet(packet), done(done) {
}

ChromotingClient::QueuedVideoPacket::~QueuedVideoPacket() {
}

ChromotingClient::ChromotingClient(const ClientConfig& config,
                                   ClientContext* context,
                                   protocol::ConnectionToHost* connection,
                                   ChromotingView* view,
                                   RectangleUpdateDecoder* rectangle_decoder,
                                   InputHandler* input_handler,
                                   const base::Closure& client_done)
    : config_(config),
      context_(context),
      connection_(connection),
      view_(view),
      rectangle_decoder_(rectangle_decoder),
      input_handler_(input_handler),
      client_done_(client_done),
      packet_being_processed_(false),
      last_sequence_number_(0),
      thread_proxy_(context_->network_message_loop()) {
}

ChromotingClient::~ChromotingClient() {
}

void ChromotingClient::Start(scoped_refptr<XmppProxy> xmpp_proxy) {
  DCHECK(message_loop()->BelongsToCurrentThread());

  connection_->Connect(xmpp_proxy, config_.local_jid, config_.host_jid,
                       config_.host_public_key, config_.access_code,
                       this, this, this);

  if (!view_->Initialize()) {
    ClientDone();
  }
}

void ChromotingClient::Stop(const base::Closure& shutdown_task) {
  if (!message_loop()->BelongsToCurrentThread()) {
    message_loop()->PostTask(
        FROM_HERE, base::Bind(&ChromotingClient::Stop,
                              base::Unretained(this), shutdown_task));
    return;
  }

  // Drop all pending packets.
  while(!received_packets_.empty()) {
    received_packets_.front().done.Run();
    received_packets_.pop_front();
  }

  connection_->Disconnect(base::Bind(&ChromotingClient::OnDisconnected,
                                     base::Unretained(this), shutdown_task));
}

void ChromotingClient::OnDisconnected(const base::Closure& shutdown_task) {
  view_->TearDown();

  shutdown_task.Run();
}

void ChromotingClient::ClientDone() {
  if (!client_done_.is_null()) {
    message_loop()->PostTask(FROM_HERE, client_done_);
    client_done_.Reset();
  }
}

ChromotingStats* ChromotingClient::GetStats() {
  return &stats_;
}

void ChromotingClient::Repaint() {
  DCHECK(message_loop()->BelongsToCurrentThread());
  view_->Paint();
}

void ChromotingClient::ProcessVideoPacket(const VideoPacket* packet,
                                          const base::Closure& done) {
  DCHECK(message_loop()->BelongsToCurrentThread());

  // If the video packet is empty then drop it. Empty packets are used to
  // maintain activity on the network.
  if (!packet->has_data() || packet->data().size() == 0) {
    done.Run();
    return;
  }

  // Add one frame to the counter.
  stats_.video_frame_rate()->Record(1);

  // Record other statistics received from host.
  stats_.video_bandwidth()->Record(packet->data().size());
  if (packet->has_capture_time_ms())
    stats_.video_capture_ms()->Record(packet->capture_time_ms());
  if (packet->has_encode_time_ms())
    stats_.video_encode_ms()->Record(packet->encode_time_ms());
  if (packet->has_client_sequence_number() &&
      packet->client_sequence_number() > last_sequence_number_) {
    last_sequence_number_ = packet->client_sequence_number();
    base::TimeDelta round_trip_latency =
        base::Time::Now() -
        base::Time::FromInternalValue(packet->client_sequence_number());
    stats_.round_trip_ms()->Record(round_trip_latency.InMilliseconds());
  }

  received_packets_.push_back(QueuedVideoPacket(packet, done));
  if (!packet_being_processed_)
    DispatchPacket();
}

int ChromotingClient::GetPendingPackets() {
  return received_packets_.size();
}

void ChromotingClient::DispatchPacket() {
  DCHECK(message_loop()->BelongsToCurrentThread());
  CHECK(!packet_being_processed_);

  if (received_packets_.empty()) {
    // Nothing to do!
    return;
  }

  const VideoPacket* packet = received_packets_.front().packet;
  packet_being_processed_ = true;

  // Measure the latency between the last packet being received and presented.
  bool last_packet = (packet->flags() & VideoPacket::LAST_PACKET) != 0;
  base::Time decode_start;
  if (last_packet)
    decode_start = base::Time::Now();

  rectangle_decoder_->DecodePacket(
      packet, base::Bind(&ChromotingClient::OnPacketDone,
                         base::Unretained(this), last_packet, decode_start));
}

void ChromotingClient::OnConnectionState(
    protocol::ConnectionToHost::State state,
    protocol::ConnectionToHost::Error error) {
  DCHECK(message_loop()->BelongsToCurrentThread());
  VLOG(1) << "ChromotingClient::OnConnectionState(" << state << ")";
  if (state == protocol::ConnectionToHost::CONNECTED)
    Initialize();
  view_->SetConnectionState(state, error);
}

base::MessageLoopProxy* ChromotingClient::message_loop() {
  return context_->network_message_loop();
}

void ChromotingClient::OnPacketDone(bool last_packet,
                                    base::Time decode_start) {
  if (!message_loop()->BelongsToCurrentThread()) {
    thread_proxy_.PostTask(FROM_HERE, base::Bind(
        &ChromotingClient::OnPacketDone, base::Unretained(this),
        last_packet, decode_start));
    return;
  }

  // Record the latency between the final packet being received and
  // presented.
  if (last_packet) {
    stats_.video_decode_ms()->Record(
        (base::Time::Now() - decode_start).InMilliseconds());
  }

  received_packets_.front().done.Run();
  received_packets_.pop_front();

  packet_being_processed_ = false;

  // Process the next video packet.
  DispatchPacket();
}

void ChromotingClient::Initialize() {
  if (!message_loop()->BelongsToCurrentThread()) {
    thread_proxy_.PostTask(FROM_HERE, base::Bind(
        &ChromotingClient::Initialize, base::Unretained(this)));
    return;
  }

  // Initialize the decoder.
  rectangle_decoder_->Initialize(connection_->config());

  // Schedule the input handler to process the event queue.
  input_handler_->Initialize();
}

}  // namespace remoting
