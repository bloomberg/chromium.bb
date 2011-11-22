// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/rtp_video_reader.h"

#include "base/bind.h"
#include "base/task.h"
#include "remoting/base/constants.h"
#include "remoting/proto/video.pb.h"
#include "remoting/protocol/session.h"

namespace remoting {
namespace protocol {

namespace {
const int kMaxPacketsInQueue = 1024;
const int kReceiverReportsIntervalMs = 1000;
}  // namespace

RtpVideoReader::PacketsQueueEntry::PacketsQueueEntry()
    : received(false),
      packet(NULL) {
}

RtpVideoReader::RtpVideoReader(base::MessageLoopProxy* message_loop)
    : session_(NULL),
      initialized_(false),
      rtcp_writer_(message_loop),
      last_sequence_number_(0),
      video_stub_(NULL) {
}

RtpVideoReader::~RtpVideoReader() {
  if (session_) {
    session_->CancelChannelCreation(kVideoRtpChannelName);
    session_->CancelChannelCreation(kVideoRtcpChannelName);
  }
  ResetQueue();
}

void RtpVideoReader::Init(protocol::Session* session,
                          VideoStub* video_stub,
                          const InitializedCallback& callback) {
  session_ = session;
  initialized_callback_ = callback;
  video_stub_ = video_stub;

  session_->CreateDatagramChannel(
      kVideoRtpChannelName,
      base::Bind(&RtpVideoReader::OnChannelReady,
                 base::Unretained(this), true));
  session_->CreateDatagramChannel(
      kVideoRtcpChannelName,
      base::Bind(&RtpVideoReader::OnChannelReady,
                 base::Unretained(this), false));
}

bool RtpVideoReader::is_connected() {
  return rtp_channel_.get() && rtcp_channel_.get();
}

void RtpVideoReader::OnChannelReady(bool rtp, net::Socket* socket) {
  if (!socket) {
    if (!initialized_) {
      initialized_ = true;
      initialized_callback_.Run(false);
    }
    return;
  }

  if (rtp) {
    DCHECK(!rtp_channel_.get());
    rtp_channel_.reset(socket);
    rtp_reader_.Init(socket, base::Bind(&RtpVideoReader::OnRtpPacket,
                                        base::Unretained(this)));
  } else {
    DCHECK(!rtcp_channel_.get());
    rtcp_channel_.reset(socket);
    rtcp_writer_.Init(socket);
  }

  if (rtp_channel_.get() && rtcp_channel_.get()) {
    DCHECK(!initialized_);
    initialized_ = true;
    initialized_callback_.Run(true);
  }
}

void RtpVideoReader::ResetQueue() {
  for (PacketsQueue::iterator it = packets_queue_.begin();
       it != packets_queue_.end(); ++it) {
    delete it->packet;
  }
  packets_queue_.assign(kMaxPacketsInQueue, PacketsQueueEntry());
}

void RtpVideoReader::OnRtpPacket(const RtpPacket* rtp_packet) {
  uint32 sequence_number = rtp_packet->extended_sequence_number();
  int32 relative_number = sequence_number - last_sequence_number_;
  int packet_index;

  if (packets_queue_.empty()) {
    // This is the first packet we've received. Setup the queue.
    ResetQueue();
    last_sequence_number_ = sequence_number;
    packet_index = packets_queue_.size() - 1;
  } else if (relative_number > 0) {
    if (relative_number > kMaxPacketsInQueue) {
      // Sequence number jumped too much for some reason. Reset the queue.
      ResetQueue();
    } else {
      packets_queue_.resize(packets_queue_.size() + relative_number);
      // Cleanup old packets, so that we don't have more than
      // |kMaxPacketsInQueue| packets.
      while (static_cast<int>(packets_queue_.size()) > kMaxPacketsInQueue) {
        delete packets_queue_.front().packet;
        packets_queue_.pop_front();
      }
    }
    last_sequence_number_ = sequence_number;
    packet_index = packets_queue_.size() - 1;
  } else {
    packet_index = packets_queue_.size() - 1 + relative_number;
    if (packet_index < 0) {
      // The packet is too old. Just drop it.
      delete rtp_packet;
      return;
    }
  }

  CHECK_LT(packet_index, static_cast<int>(packets_queue_.size()));

  if (packets_queue_[packet_index].received) {
    VLOG(1) << "Received duplicate packet with sequence number "
            << sequence_number;
    delete rtp_packet;
    return;
  }

  packets_queue_[packet_index].packet = rtp_packet;
  packets_queue_[packet_index].received = true;

  CheckFullPacket(packets_queue_.begin() + packet_index);
}

void RtpVideoReader::CheckFullPacket(const PacketsQueue::iterator& pos) {
  if (pos->packet->vp8_descriptor().fragmentation_info ==
      Vp8Descriptor::NOT_FRAGMENTED) {
    // The packet is not fragmented.
    RebuildVideoPacket(pos, pos);
    return;
  }

  PacketsQueue::iterator first = pos;
  while (first > packets_queue_.begin() && first->packet &&
         first->packet->vp8_descriptor().fragmentation_info !=
         Vp8Descriptor::FIRST_FRAGMENT) {
    first--;
  }
  if (!first->packet || first->packet->vp8_descriptor().fragmentation_info !=
      Vp8Descriptor::FIRST_FRAGMENT) {
    // We don't have first fragment.
    return;
  }

  PacketsQueue::iterator last = pos;
  while (last < (packets_queue_.end() - 1) && last->packet &&
         last->packet->vp8_descriptor().fragmentation_info !=
         Vp8Descriptor::LAST_FRAGMENT) {
    last++;
  }
  if (!last->packet || last->packet->vp8_descriptor().fragmentation_info !=
      Vp8Descriptor::LAST_FRAGMENT) {
    // We don't have last fragment.
    return;
  }

  // We've found first and last fragments, and we have all fragments in the
  // middle, so we can rebuild fill packet.
  RebuildVideoPacket(first, last);
}

void RtpVideoReader::RebuildVideoPacket(const PacketsQueue::iterator& first,
                                        const PacketsQueue::iterator& last) {
  VideoPacket* packet = new VideoPacket();

  // Set flags.
  if (first->packet->vp8_descriptor().frame_beginning)
    packet->set_flags(packet->flags() | VideoPacket::FIRST_PACKET);

  if (last->packet->header().marker)
    packet->set_flags(packet->flags() | VideoPacket::LAST_PACKET);

  packet->set_timestamp(first->packet->header().timestamp);

  // Rebuild packet content from the fragments.
  // TODO(sergeyu): Use CompoundBuffer inside of VideoPacket, so that we don't
  // need to memcopy any data.
  CompoundBuffer content;
  for (PacketsQueue::iterator it = first; it <= last; ++it) {
    content.Append(it->packet->payload());

    // Delete packet because we don't need it anymore.
    delete it->packet;
    it->packet = NULL;
    // Here we keep |received| flag set to true, so that duplicate RTP
    // packets will be ignored.
  }

  packet->mutable_data()->resize(content.total_bytes());
  content.CopyTo(const_cast<char*>(packet->mutable_data()->data()),
                 packet->data().size());

  // Set format.
  packet->mutable_format()->set_encoding(VideoPacketFormat::ENCODING_VP8);

  video_stub_->ProcessVideoPacket(
      packet, base::Bind(&DeletePointer<VideoPacket>, packet));

  SendReceiverReportIf();
}

void RtpVideoReader::SendReceiverReportIf() {
  base::Time now = base::Time::Now();

  // Send receiver report only if we haven't sent any bofore, or
  // enough time has passed since the last report.
  if (last_receiver_report_.is_null() ||
      (now - last_receiver_report_).InMilliseconds() >
      kReceiverReportsIntervalMs) {
    RtcpReceiverReport report;
    rtp_reader_.GetReceiverReport(&report);
    rtcp_writer_.SendReport(report);

    last_receiver_report_ = now;
  }
}

}  // namespace protocol
}  // namespace remoting
