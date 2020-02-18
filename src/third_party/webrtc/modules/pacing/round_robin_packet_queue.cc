/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/pacing/round_robin_packet_queue.h"

#include <algorithm>
#include <cstdint>
#include <utility>

#include "rtc_base/checks.h"

namespace webrtc {
namespace {
static constexpr DataSize kMaxLeadingSize = DataSize::Bytes<1400>();
}

RoundRobinPacketQueue::QueuedPacket::QueuedPacket(const QueuedPacket& rhs) =
    default;
RoundRobinPacketQueue::QueuedPacket::~QueuedPacket() = default;

RoundRobinPacketQueue::QueuedPacket::QueuedPacket(
    int priority,
    Timestamp enqueue_time,
    uint64_t enqueue_order,
    std::multiset<Timestamp>::iterator enqueue_time_it,
    std::unique_ptr<RtpPacketToSend> packet)
    : priority_(priority),
      enqueue_time_(enqueue_time),
      enqueue_order_(enqueue_order),
      is_retransmission_(packet->packet_type() ==
                         RtpPacketToSend::Type::kRetransmission),
      enqueue_time_it_(enqueue_time_it),
      owned_packet_(packet.release()) {}

bool RoundRobinPacketQueue::QueuedPacket::operator<(
    const RoundRobinPacketQueue::QueuedPacket& other) const {
  if (priority_ != other.priority_)
    return priority_ > other.priority_;
  if (is_retransmission_ != other.is_retransmission_)
    return other.is_retransmission_;

  return enqueue_order_ > other.enqueue_order_;
}

int RoundRobinPacketQueue::QueuedPacket::Priority() const {
  return priority_;
}

RtpPacketToSend::Type RoundRobinPacketQueue::QueuedPacket::Type() const {
  return *owned_packet_->packet_type();
}

uint32_t RoundRobinPacketQueue::QueuedPacket::Ssrc() const {
  return owned_packet_->Ssrc();
}

Timestamp RoundRobinPacketQueue::QueuedPacket::EnqueueTime() const {
  return enqueue_time_;
}

bool RoundRobinPacketQueue::QueuedPacket::IsRetransmission() const {
  return Type() == RtpPacketToSend::Type::kRetransmission;
}

uint64_t RoundRobinPacketQueue::QueuedPacket::EnqueueOrder() const {
  return enqueue_order_;
}

DataSize RoundRobinPacketQueue::QueuedPacket::Size(bool count_overhead) const {
  return DataSize::bytes(count_overhead ? owned_packet_->size()
                                        : owned_packet_->payload_size() +
                                              owned_packet_->padding_size());
}

RtpPacketToSend* RoundRobinPacketQueue::QueuedPacket::RtpPacket() const {
  return owned_packet_;
}

std::multiset<Timestamp>::iterator
RoundRobinPacketQueue::QueuedPacket::EnqueueTimeIterator() const {
  return enqueue_time_it_;
}

void RoundRobinPacketQueue::QueuedPacket::SubtractPauseTime(
    TimeDelta pause_time_sum) {
  enqueue_time_ -= pause_time_sum;
}

RoundRobinPacketQueue::Stream::Stream() : size(DataSize::Zero()), ssrc(0) {}
RoundRobinPacketQueue::Stream::Stream(const Stream& stream) = default;
RoundRobinPacketQueue::Stream::~Stream() = default;

bool IsEnabled(const WebRtcKeyValueConfig* field_trials, const char* name) {
  if (!field_trials) {
    return false;
  }
  return field_trials->Lookup(name).find("Enabled") == 0;
}

RoundRobinPacketQueue::RoundRobinPacketQueue(
    Timestamp start_time,
    const WebRtcKeyValueConfig* field_trials)
    : time_last_updated_(start_time),
      paused_(false),
      size_packets_(0),
      size_(DataSize::Zero()),
      max_size_(kMaxLeadingSize),
      queue_time_sum_(TimeDelta::Zero()),
      pause_time_sum_(TimeDelta::Zero()),
      send_side_bwe_with_overhead_(
          IsEnabled(field_trials, "WebRTC-SendSideBwe-WithOverhead")) {}

RoundRobinPacketQueue::~RoundRobinPacketQueue() {
  // Make sure to release any packets owned by raw pointer in QueuedPacket.
  while (!Empty()) {
    Pop();
  }
}

void RoundRobinPacketQueue::Push(int priority,
                                 Timestamp enqueue_time,
                                 uint64_t enqueue_order,
                                 std::unique_ptr<RtpPacketToSend> packet) {
  RTC_DCHECK(packet->packet_type().has_value());
  Push(QueuedPacket(priority, enqueue_time, enqueue_order,
                    enqueue_times_.insert(enqueue_time), std::move(packet)));
}

std::unique_ptr<RtpPacketToSend> RoundRobinPacketQueue::Pop() {
  RTC_DCHECK(!Empty());
  Stream* stream = GetHighestPriorityStream();
  const QueuedPacket& queued_packet = stream->packet_queue.top();

  stream_priorities_.erase(stream->priority_it);

  // Calculate the total amount of time spent by this packet in the queue
  // while in a non-paused state. Note that the |pause_time_sum_ms_| was
  // subtracted from |packet.enqueue_time_ms| when the packet was pushed, and
  // by subtracting it now we effectively remove the time spent in in the
  // queue while in a paused state.
  TimeDelta time_in_non_paused_state =
      time_last_updated_ - queued_packet.EnqueueTime() - pause_time_sum_;
  queue_time_sum_ -= time_in_non_paused_state;

  RTC_CHECK(queued_packet.EnqueueTimeIterator() != enqueue_times_.end());
  enqueue_times_.erase(queued_packet.EnqueueTimeIterator());

  // Update |bytes| of this stream. The general idea is that the stream that
  // has sent the least amount of bytes should have the highest priority.
  // The problem with that is if streams send with different rates, in which
  // case a "budget" will be built up for the stream sending at the lower
  // rate. To avoid building a too large budget we limit |bytes| to be within
  // kMaxLeading bytes of the stream that has sent the most amount of bytes.
  DataSize packet_size = queued_packet.Size(send_side_bwe_with_overhead_);
  stream->size =
      std::max(stream->size + packet_size, max_size_ - kMaxLeadingSize);
  max_size_ = std::max(max_size_, stream->size);

  size_ -= packet_size;
  size_packets_ -= 1;
  RTC_CHECK(size_packets_ > 0 || queue_time_sum_ == TimeDelta::Zero());

  std::unique_ptr<RtpPacketToSend> rtp_packet(queued_packet.RtpPacket());
  stream->packet_queue.pop();

  // If there are packets left to be sent, schedule the stream again.
  RTC_CHECK(!IsSsrcScheduled(stream->ssrc));
  if (stream->packet_queue.empty()) {
    stream->priority_it = stream_priorities_.end();
  } else {
    int priority = stream->packet_queue.top().Priority();
    stream->priority_it = stream_priorities_.emplace(
        StreamPrioKey(priority, stream->size), stream->ssrc);
  }

  return rtp_packet;
}

bool RoundRobinPacketQueue::Empty() const {
  RTC_CHECK((!stream_priorities_.empty() && size_packets_ > 0) ||
            (stream_priorities_.empty() && size_packets_ == 0));
  return stream_priorities_.empty();
}

size_t RoundRobinPacketQueue::SizeInPackets() const {
  return size_packets_;
}

DataSize RoundRobinPacketQueue::Size() const {
  return size_;
}

bool RoundRobinPacketQueue::NextPacketIsAudio() const {
  if (stream_priorities_.empty()) {
    return false;
  }
  uint32_t ssrc = stream_priorities_.begin()->second;

  auto stream_info_it = streams_.find(ssrc);
  return stream_info_it->second.packet_queue.top().Type() ==
         RtpPacketToSend::Type::kAudio;
}

Timestamp RoundRobinPacketQueue::OldestEnqueueTime() const {
  if (Empty())
    return Timestamp::MinusInfinity();
  RTC_CHECK(!enqueue_times_.empty());
  return *enqueue_times_.begin();
}

void RoundRobinPacketQueue::UpdateQueueTime(Timestamp now) {
  RTC_CHECK_GE(now, time_last_updated_);
  if (now == time_last_updated_)
    return;

  TimeDelta delta = now - time_last_updated_;

  if (paused_) {
    pause_time_sum_ += delta;
  } else {
    queue_time_sum_ += TimeDelta::us(delta.us() * size_packets_);
  }

  time_last_updated_ = now;
}

void RoundRobinPacketQueue::SetPauseState(bool paused, Timestamp now) {
  if (paused_ == paused)
    return;
  UpdateQueueTime(now);
  paused_ = paused;
}

TimeDelta RoundRobinPacketQueue::AverageQueueTime() const {
  if (Empty())
    return TimeDelta::Zero();
  return queue_time_sum_ / size_packets_;
}

void RoundRobinPacketQueue::Push(QueuedPacket packet) {
  auto stream_info_it = streams_.find(packet.Ssrc());
  if (stream_info_it == streams_.end()) {
    stream_info_it = streams_.emplace(packet.Ssrc(), Stream()).first;
    stream_info_it->second.priority_it = stream_priorities_.end();
    stream_info_it->second.ssrc = packet.Ssrc();
  }

  Stream* stream = &stream_info_it->second;

  if (stream->priority_it == stream_priorities_.end()) {
    // If the SSRC is not currently scheduled, add it to |stream_priorities_|.
    RTC_CHECK(!IsSsrcScheduled(stream->ssrc));
    stream->priority_it = stream_priorities_.emplace(
        StreamPrioKey(packet.Priority(), stream->size), packet.Ssrc());
  } else if (packet.Priority() < stream->priority_it->first.priority) {
    // If the priority of this SSRC increased, remove the outdated StreamPrioKey
    // and insert a new one with the new priority. Note that |priority_| uses
    // lower ordinal for higher priority.
    stream_priorities_.erase(stream->priority_it);
    stream->priority_it = stream_priorities_.emplace(
        StreamPrioKey(packet.Priority(), stream->size), packet.Ssrc());
  }
  RTC_CHECK(stream->priority_it != stream_priorities_.end());

  // In order to figure out how much time a packet has spent in the queue while
  // not in a paused state, we subtract the total amount of time the queue has
  // been paused so far, and when the packet is popped we subtract the total
  // amount of time the queue has been paused at that moment. This way we
  // subtract the total amount of time the packet has spent in the queue while
  // in a paused state.
  UpdateQueueTime(packet.EnqueueTime());
  packet.SubtractPauseTime(pause_time_sum_);

  size_packets_ += 1;
  size_ += packet.Size(send_side_bwe_with_overhead_);

  stream->packet_queue.push(packet);
}

RoundRobinPacketQueue::Stream*
RoundRobinPacketQueue::GetHighestPriorityStream() {
  RTC_CHECK(!stream_priorities_.empty());
  uint32_t ssrc = stream_priorities_.begin()->second;

  auto stream_info_it = streams_.find(ssrc);
  RTC_CHECK(stream_info_it != streams_.end());
  RTC_CHECK(stream_info_it->second.priority_it == stream_priorities_.begin());
  RTC_CHECK(!stream_info_it->second.packet_queue.empty());
  return &stream_info_it->second;
}

bool RoundRobinPacketQueue::IsSsrcScheduled(uint32_t ssrc) const {
  for (const auto& scheduled_stream : stream_priorities_) {
    if (scheduled_stream.second == ssrc)
      return true;
  }
  return false;
}

}  // namespace webrtc
