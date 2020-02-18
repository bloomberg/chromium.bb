// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "net/third_party/quiche/src/quic/core/quic_stream_id_manager.h"

#include <string>

#include "net/third_party/quiche/src/quic/core/quic_connection.h"
#include "net/third_party/quiche/src/quic/core/quic_constants.h"
#include "net/third_party/quiche/src/quic/core/quic_session.h"
#include "net/third_party/quiche/src/quic/core/quic_utils.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_flag_utils.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_flags.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_logging.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_str_cat.h"

namespace quic {

#define ENDPOINT \
  (perspective_ == Perspective::IS_SERVER ? " Server: " : " Client: ")

QuicStreamIdManager::QuicStreamIdManager(
    DelegateInterface* delegate,
    bool unidirectional,
    Perspective perspective,
    QuicTransportVersion transport_version,
    QuicStreamCount num_expected_static_streams,
    QuicStreamCount max_allowed_outgoing_streams,
    QuicStreamCount max_allowed_incoming_streams)
    : delegate_(delegate),
      unidirectional_(unidirectional),
      perspective_(perspective),
      transport_version_(transport_version),
      num_expected_static_streams_(num_expected_static_streams),
      is_config_negotiated_(false),
      outgoing_max_streams_(max_allowed_outgoing_streams),
      next_outgoing_stream_id_(GetFirstOutgoingStreamId()),
      outgoing_stream_count_(0),
      using_default_max_streams_(true),
      incoming_actual_max_streams_(max_allowed_incoming_streams),
      // Advertised max starts at actual because it's communicated in the
      // handshake.
      incoming_advertised_max_streams_(max_allowed_incoming_streams),
      incoming_initial_max_open_streams_(max_allowed_incoming_streams),
      incoming_stream_count_(0),
      largest_peer_created_stream_id_(
          QuicUtils::GetInvalidStreamId(transport_version)),
      max_streams_window_(0),
      pending_max_streams_(false),
      pending_streams_blocked_(
          QuicUtils::GetInvalidStreamId(transport_version)) {
  CalculateIncomingMaxStreamsWindow();
}

QuicStreamIdManager::~QuicStreamIdManager() {}

bool QuicStreamIdManager::OnMaxStreamsFrame(const QuicMaxStreamsFrame& frame) {
  // Ensure that the frame has the correct directionality.
  DCHECK_EQ(frame.unidirectional, unidirectional_);
  QUIC_CODE_COUNT_N(quic_max_streams_received, 2, 2);

  // Set the limit to be exactly the stream count in the frame.
  // Also informs the higher layers that they can create more
  // streams if the limit is increased.
  return SetMaxOpenOutgoingStreams(frame.stream_count);
}

// The peer sends a streams blocked frame when it can not open any more
// streams because it has runs into the limit.
bool QuicStreamIdManager::OnStreamsBlockedFrame(
    const QuicStreamsBlockedFrame& frame) {
  // Ensure that the frame has the correct directionality.
  DCHECK_EQ(frame.unidirectional, unidirectional_);
  QUIC_CODE_COUNT_N(quic_streams_blocked_received, 2, 2);

  if (frame.stream_count > incoming_advertised_max_streams_) {
    // Peer thinks it can send more streams that we've told it.
    // This is a protocol error.
    // TODO(fkastenholz): revise when proper IETF Connection Close support is
    // done.
    QUIC_CODE_COUNT(quic_streams_blocked_too_big);
    delegate_->OnError(QUIC_STREAMS_BLOCKED_ERROR,
                       "Invalid stream count specified");
    return false;
  }
  if (frame.stream_count < incoming_actual_max_streams_) {
    // Peer thinks it's blocked on a stream count that is less than our current
    // max. Inform the peer of the correct stream count. Sending a MAX_STREAMS
    // frame in this case is not controlled by the window.
    SendMaxStreamsFrame();
  }
  QUIC_CODE_COUNT(quic_streams_blocked_id_correct);
  return true;
}

// Used when configuration has been done and we have an initial
// maximum stream count from the peer.
bool QuicStreamIdManager::SetMaxOpenOutgoingStreams(size_t max_open_streams) {
  if (max_open_streams < num_expected_static_streams_) {
    QUIC_DLOG(ERROR) << "Received max streams " << max_open_streams << " < "
                     << num_expected_static_streams_;
    delegate_->OnError(QUIC_MAX_STREAMS_ERROR,
                       unidirectional_
                           ? "New unidirectional stream limit is too low."
                           : "New bidirectional stream limit is too low.");

    return false;
  }
  if (using_default_max_streams_) {
    // This is the first MAX_STREAMS/transport negotiation we've received. Treat
    // this a bit differently than later ones. The difference is that
    // outgoing_max_streams_ is currently an estimate. The MAX_STREAMS frame or
    // transport negotiation is authoritative and can reduce
    // outgoing_max_streams_ -- so long as outgoing_max_streams_ is not set to
    // be less than the number of existing outgoing streams. If that happens,
    // close the connection.
    if (max_open_streams < outgoing_stream_count_) {
      delegate_->OnError(QUIC_MAX_STREAMS_ERROR,
                         "Stream limit less than existing stream count");
      return false;
    }
    using_default_max_streams_ = false;
  } else if (max_open_streams <= outgoing_max_streams_) {
    // Is not the 1st MAX_STREAMS or negotiation.
    // Only update the stream count if it would increase the limit.
    // If it decreases the limit, or doesn't change it, then do not update.
    // Note that this handles the case of receiving a count of 0 in the frame
    return true;
  }

  // This implementation only supports 32 bit Stream IDs, so limit max streams
  // if it would exceed the max 32 bits can express.
  outgoing_max_streams_ = std::min<size_t>(
      max_open_streams,
      QuicUtils::GetMaxStreamCount(unidirectional_, perspective_));

  // Inform the higher layers that the stream limit has increased and that
  // new streams may be created.
  delegate_->OnCanCreateNewOutgoingStream(unidirectional_);

  return true;
}

void QuicStreamIdManager::SetMaxOpenIncomingStreams(size_t max_open_streams) {
  QuicStreamCount implementation_max =
      QuicUtils::GetMaxStreamCount(unidirectional_, perspective());
  QuicStreamCount new_max = std::min(
      implementation_max, static_cast<QuicStreamCount>(max_open_streams));
  if (new_max < incoming_stream_count_) {
    delegate_->OnError(QUIC_MAX_STREAMS_ERROR,
                       "Stream limit less than existing stream count");
    return;
  }
  incoming_actual_max_streams_ = new_max;
  incoming_advertised_max_streams_ = new_max;
  incoming_initial_max_open_streams_ =
      std::min(max_open_streams, static_cast<size_t>(implementation_max));
  CalculateIncomingMaxStreamsWindow();
}

void QuicStreamIdManager::MaybeSendMaxStreamsFrame() {
  if ((incoming_advertised_max_streams_ - incoming_stream_count_) >
      max_streams_window_) {
    // window too large, no advertisement
    return;
  }
  SendMaxStreamsFrame();
}

void QuicStreamIdManager::SendMaxStreamsFrame() {
  if (!is_config_negotiated_) {
    // The config has not yet been negotiated, so we can not send the
    // MAX STREAMS frame yet. Record that we would have sent one and then
    // return. A new frame will be generated once the configuration is
    // received.
    pending_max_streams_ = true;
    return;
  }
  incoming_advertised_max_streams_ = incoming_actual_max_streams_;
  delegate_->SendMaxStreams(incoming_advertised_max_streams_, unidirectional_);
}

void QuicStreamIdManager::OnStreamClosed(QuicStreamId stream_id) {
  DCHECK_NE(QuicUtils::IsBidirectionalStreamId(stream_id), unidirectional_);
  if (!IsIncomingStream(stream_id)) {
    // Nothing to do for outgoing streams.
    return;
  }
  // If the stream is inbound, we can increase the actual stream limit and maybe
  // advertise the new limit to the peer.  Have to check to make sure that we do
  // not exceed the maximum.
  if (incoming_actual_max_streams_ ==
      QuicUtils::GetMaxStreamCount(unidirectional_, perspective())) {
    // Reached the maximum stream id value that the implementation
    // supports. Nothing can be done here.
    return;
  }
  // One stream closed ... another can be opened.
  incoming_actual_max_streams_++;
  MaybeSendMaxStreamsFrame();
}

QuicStreamId QuicStreamIdManager::GetNextOutgoingStreamId() {
  // TODO(fkastenholz): Should we close the connection?
  QUIC_BUG_IF(outgoing_stream_count_ >= outgoing_max_streams_)
      << "Attempt to allocate a new outgoing stream that would exceed the "
         "limit ("
      << outgoing_max_streams_ << ")";
  QuicStreamId id = next_outgoing_stream_id_;
  next_outgoing_stream_id_ += QuicUtils::StreamIdDelta(transport_version());
  outgoing_stream_count_++;
  return id;
}

bool QuicStreamIdManager::CanOpenNextOutgoingStream() {
  DCHECK(VersionHasIetfQuicFrames(transport_version()));
  if (outgoing_stream_count_ < outgoing_max_streams_) {
    return true;
  }
  // Next stream ID would exceed the limit, need to inform the peer.

  if (!is_config_negotiated_) {
    // The config is not negotiated, so we can not send the STREAMS_BLOCKED
    // frame yet. Record that we would have sent one, and what the limit was
    // when we were blocked, and return.
    pending_streams_blocked_ = outgoing_max_streams_;
    return false;
  }
  delegate_->SendStreamsBlocked(outgoing_max_streams_, unidirectional_);
  QUIC_CODE_COUNT(quic_reached_outgoing_stream_id_limit);
  return false;
}

// Stream_id is the id of a new incoming stream. Check if it can be
// created (doesn't violate limits, etc).
bool QuicStreamIdManager::MaybeIncreaseLargestPeerStreamId(
    const QuicStreamId stream_id) {
  // |stream_id| must be an incoming stream of the right directionality.
  DCHECK_NE(QuicUtils::IsBidirectionalStreamId(stream_id), unidirectional_);
  DCHECK_NE(
      QuicUtils::IsServerInitiatedStreamId(transport_version(), stream_id),
      perspective() == Perspective::IS_SERVER);
  available_streams_.erase(stream_id);

  if (largest_peer_created_stream_id_ !=
          QuicUtils::GetInvalidStreamId(transport_version()) &&
      stream_id <= largest_peer_created_stream_id_) {
    return true;
  }

  QuicStreamCount stream_count_increment;
  if (largest_peer_created_stream_id_ !=
      QuicUtils::GetInvalidStreamId(transport_version())) {
    stream_count_increment = (stream_id - largest_peer_created_stream_id_) /
                             QuicUtils::StreamIdDelta(transport_version());
  } else {
    // Largest_peer_created_stream_id is the invalid ID,
    // which means that the peer has not created any stream IDs.
    // The "+1" is because the first stream ID has not yet
    // been used. For example, if the FirstIncoming ID is 1
    // and stream_id is 1, then we want the increment to be 1.
    stream_count_increment = ((stream_id - GetFirstIncomingStreamId()) /
                              QuicUtils::StreamIdDelta(transport_version())) +
                             1;
  }

  // If already at, or over, the limit, close the connection/etc.
  if (((incoming_stream_count_ + stream_count_increment) >
       incoming_advertised_max_streams_) ||
      ((incoming_stream_count_ + stream_count_increment) <
       incoming_stream_count_)) {
    // This stream would exceed the limit. do not increase.
    QUIC_DLOG(INFO) << ENDPOINT
                    << "Failed to create a new incoming stream with id:"
                    << stream_id << ", reaching MAX_STREAMS limit: "
                    << incoming_advertised_max_streams_ << ".";
    delegate_->OnError(
        QUIC_INVALID_STREAM_ID,
        QuicStrCat("Stream id ", stream_id, " would exceed stream count limit ",
                   incoming_advertised_max_streams_));
    return false;
  }

  QuicStreamId id = GetFirstIncomingStreamId();
  if (largest_peer_created_stream_id_ !=
      QuicUtils::GetInvalidStreamId(transport_version())) {
    id = largest_peer_created_stream_id_ +
         QuicUtils::StreamIdDelta(transport_version());
  }

  for (; id < stream_id; id += QuicUtils::StreamIdDelta(transport_version())) {
    available_streams_.insert(id);
  }
  incoming_stream_count_ += stream_count_increment;
  largest_peer_created_stream_id_ = stream_id;
  return true;
}

bool QuicStreamIdManager::IsAvailableStream(QuicStreamId id) const {
  DCHECK_NE(QuicUtils::IsBidirectionalStreamId(id), unidirectional_);
  if (!IsIncomingStream(id)) {
    // Stream IDs under next_ougoing_stream_id_ are either open or previously
    // open but now closed.
    return id >= next_outgoing_stream_id_;
  }
  // For peer created streams, we also need to consider available streams.
  return largest_peer_created_stream_id_ ==
             QuicUtils::GetInvalidStreamId(transport_version()) ||
         id > largest_peer_created_stream_id_ ||
         QuicContainsKey(available_streams_, id);
}

bool QuicStreamIdManager::IsIncomingStream(QuicStreamId id) const {
  DCHECK_NE(QuicUtils::IsBidirectionalStreamId(id), unidirectional_);
  // The 0x1 bit in the stream id indicates whether the stream id is
  // server- or client- initiated. Next_OUTGOING_stream_id_ has that bit
  // set based on whether this node is a server or client. Thus, if the stream
  // id in question has the 0x1 bit set opposite of next_OUTGOING_stream_id_,
  // then that stream id is incoming -- it is for streams initiated by the peer.
  return (id & 0x1) != (next_outgoing_stream_id_ & 0x1);
}

QuicStreamId QuicStreamIdManager::GetFirstOutgoingStreamId() const {
  return (unidirectional_) ? QuicUtils::GetFirstUnidirectionalStreamId(
                                 transport_version(), perspective())
                           : QuicUtils::GetFirstBidirectionalStreamId(
                                 transport_version(), perspective());
}

QuicStreamId QuicStreamIdManager::GetFirstIncomingStreamId() const {
  return (unidirectional_) ? QuicUtils::GetFirstUnidirectionalStreamId(
                                 transport_version(), peer_perspective())
                           : QuicUtils::GetFirstBidirectionalStreamId(
                                 transport_version(), peer_perspective());
}

Perspective QuicStreamIdManager::perspective() const {
  return perspective_;
}

Perspective QuicStreamIdManager::peer_perspective() const {
  return QuicUtils::InvertPerspective(perspective());
}

QuicTransportVersion QuicStreamIdManager::transport_version() const {
  return transport_version_;
}

size_t QuicStreamIdManager::available_incoming_streams() {
  return incoming_advertised_max_streams_ - incoming_stream_count_;
}

void QuicStreamIdManager::CalculateIncomingMaxStreamsWindow() {
  max_streams_window_ = incoming_actual_max_streams_ / kMaxStreamsWindowDivisor;
  if (max_streams_window_ == 0) {
    max_streams_window_ = 1;
  }
}

void QuicStreamIdManager::OnConfigNegotiated() {
  is_config_negotiated_ = true;
  // If a STREAMS_BLOCKED or MAX_STREAMS is pending, send it and clear
  // the pending state.
  if (pending_streams_blocked_ !=
      QuicUtils::GetInvalidStreamId(transport_version())) {
    if (pending_streams_blocked_ >= outgoing_max_streams_) {
      // There is a pending STREAMS_BLOCKED frame and the current limit does not
      // let new streams be formed. Regenerate and send the frame.
      delegate_->SendStreamsBlocked(outgoing_max_streams_, unidirectional_);
    }
    pending_streams_blocked_ =
        QuicUtils::GetInvalidStreamId(transport_version());
  }
  if (pending_max_streams_) {
    // Generate a MAX_STREAMS using the current stream limits.
    SendMaxStreamsFrame();
    pending_max_streams_ = false;
  }
}

}  // namespace quic
