// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/chromoting_client.h"

#include "base/bind.h"
#include "remoting/client/chromoting_view.h"
#include "remoting/client/client_context.h"
#include "remoting/client/rectangle_update_decoder.h"
#include "remoting/proto/audio.pb.h"
#include "remoting/protocol/authentication_method.h"
#include "remoting/protocol/connection_to_host.h"
#include "remoting/protocol/negotiating_authenticator.h"
#include "remoting/protocol/session_config.h"
#include "remoting/protocol/transport.h"

namespace remoting {

using protocol::AuthenticationMethod;

ChromotingClient::QueuedVideoPacket::QueuedVideoPacket(
    scoped_ptr<VideoPacket> packet, const base::Closure& done)
    : packet(packet.release()), done(done) {
}

ChromotingClient::QueuedVideoPacket::~QueuedVideoPacket() {
}

ChromotingClient::ChromotingClient(
    const ClientConfig& config,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    protocol::ConnectionToHost* connection,
    ChromotingView* view,
    RectangleUpdateDecoder* rectangle_decoder)
    : config_(config),
      task_runner_(task_runner),
      connection_(connection),
      view_(view),
      rectangle_decoder_(rectangle_decoder),
      packet_being_processed_(false),
      last_sequence_number_(0),
      weak_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
}

ChromotingClient::~ChromotingClient() {
}

void ChromotingClient::Start(
    scoped_refptr<XmppProxy> xmpp_proxy,
    scoped_ptr<protocol::TransportFactory> transport_factory) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  scoped_ptr<protocol::Authenticator> authenticator(
      protocol::NegotiatingAuthenticator::CreateForClient(
          config_.authentication_tag,
          config_.shared_secret, config_.authentication_methods));

  // Create a WeakPtr to ourself for to use for all posted tasks.
  weak_ptr_ = weak_factory_.GetWeakPtr();

  connection_->Connect(xmpp_proxy, config_.local_jid, config_.host_jid,
                       config_.host_public_key, transport_factory.Pass(),
                       authenticator.Pass(), this, this, this, this, this);

  view_->Initialize();
}

void ChromotingClient::Stop(const base::Closure& shutdown_task) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  // Drop all pending packets.
  while(!received_packets_.empty()) {
    delete received_packets_.front().packet;
    received_packets_.front().done.Run();
    received_packets_.pop_front();
  }

  connection_->Disconnect(base::Bind(&ChromotingClient::OnDisconnected,
                                     weak_ptr_, shutdown_task));
}

void ChromotingClient::OnDisconnected(const base::Closure& shutdown_task) {
  view_->TearDown();

  shutdown_task.Run();
}

ChromotingStats* ChromotingClient::GetStats() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  return &stats_;
}

void ChromotingClient::InjectClipboardEvent(
    const protocol::ClipboardEvent& event) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  view_->GetClipboardStub()->InjectClipboardEvent(event);
}

void ChromotingClient::SetCursorShape(
    const protocol::CursorShapeInfo& cursor_shape) {
  view_->GetCursorShapeStub()->SetCursorShape(cursor_shape);
}

void ChromotingClient::ProcessVideoPacket(scoped_ptr<VideoPacket> packet,
                                          const base::Closure& done) {
  DCHECK(task_runner_->BelongsToCurrentThread());

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

  received_packets_.push_back(QueuedVideoPacket(packet.Pass(), done));
  if (!packet_being_processed_)
    DispatchPacket();
}

int ChromotingClient::GetPendingVideoPackets() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  return received_packets_.size();
}

void ChromotingClient::ProcessAudioPacket(scoped_ptr<AudioPacket> packet,
                                          const base::Closure& done) {
  // TODO(kxing): Playback audio.
  done.Run();
}

void ChromotingClient::DispatchPacket() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  CHECK(!packet_being_processed_);

  if (received_packets_.empty()) {
    // Nothing to do!
    return;
  }

  scoped_ptr<VideoPacket> packet(received_packets_.front().packet);
  received_packets_.front().packet = NULL;
  packet_being_processed_ = true;

  // Measure the latency between the last packet being received and presented.
  bool last_packet = (packet->flags() & VideoPacket::LAST_PACKET) != 0;
  base::Time decode_start;
  if (last_packet)
    decode_start = base::Time::Now();

  rectangle_decoder_->DecodePacket(
      packet.Pass(),
      base::Bind(&ChromotingClient::OnPacketDone, base::Unretained(this),
                 last_packet, decode_start));
}

void ChromotingClient::OnConnectionState(
    protocol::ConnectionToHost::State state,
    protocol::ErrorCode error) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  VLOG(1) << "ChromotingClient::OnConnectionState(" << state << ")";
  if (state == protocol::ConnectionToHost::CONNECTED)
    Initialize();
  view_->SetConnectionState(state, error);
}

void ChromotingClient::OnPacketDone(bool last_packet,
                                    base::Time decode_start) {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(FROM_HERE, base::Bind(
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
  DCHECK(task_runner_->BelongsToCurrentThread());

  // Initialize the decoder.
  rectangle_decoder_->Initialize(connection_->config());
}

}  // namespace remoting
