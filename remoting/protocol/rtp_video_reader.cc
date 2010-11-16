// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/rtp_video_reader.h"

#include "base/task.h"
#include "remoting/proto/video.pb.h"
#include "remoting/protocol/session.h"

namespace remoting {
namespace protocol {

namespace {
const int kMaxPacketsInQueue = 1024;
}  // namespace

RtpVideoReader::RtpVideoReader()
    : last_sequence_number_(0) {
}

RtpVideoReader::~RtpVideoReader() {
  ResetQueue();
}

void RtpVideoReader::Init(protocol::Session* session, VideoStub* video_stub) {
  rtp_reader_.Init(session->video_rtp_channel(),
                   NewCallback(this, &RtpVideoReader::OnRtpPacket));
  video_stub_ = video_stub;
}

void RtpVideoReader::Close() {
  rtp_reader_.Close();
}

void RtpVideoReader::ResetQueue() {
  for (PacketsQueue::iterator it = packets_queue_.begin();
       it != packets_queue_.end(); ++it) {
    delete *it;
  }
  packets_queue_.clear();
}

void RtpVideoReader::OnRtpPacket(const RtpPacket* rtp_packet) {
  uint32 sequence_number = rtp_packet->header().sequence_number;
  int32 relative_number = sequence_number - last_sequence_number_;
  int packet_index;
  if (relative_number > 0) {
    if (relative_number > kMaxPacketsInQueue) {
      // Sequence number jumped too much for some reason. Reset the queue.
      ResetQueue();
      packets_queue_.resize(1);
    } else {
      packets_queue_.resize(packets_queue_.size() + relative_number);
      // Cleanup old packets, so that we don't have more than
      // |kMaxPacketsInQueue| packets.
      while (static_cast<int>(packets_queue_.size()) > kMaxPacketsInQueue) {
        delete packets_queue_.front();
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
  if (packets_queue_[packet_index]) {
    LOG(WARNING) << "Received duplicate packet with sequence number "
                 << sequence_number;
    delete packets_queue_[packet_index];
  }
  packets_queue_[packet_index] = rtp_packet;

  CheckFullPacket(packets_queue_.begin() + packet_index);
}

void RtpVideoReader::CheckFullPacket(PacketsQueue::iterator pos) {
  if ((*pos)->vp8_descriptor().fragmentation_info ==
      Vp8Descriptor::NOT_FRAGMENTED) {
    // The packet is not fragmented.
    RebuildVideoPacket(pos, pos);
    return;
  }

  PacketsQueue::iterator first = pos;
  while (first > packets_queue_.begin() && (*first) &&
         (*first)->vp8_descriptor().fragmentation_info !=
         Vp8Descriptor::FIRST_FRAGMENT) {
    first--;
  }
  if (!(*first) || (*first)->vp8_descriptor().fragmentation_info !=
      Vp8Descriptor::FIRST_FRAGMENT) {
    // We don't have first fragment.
    return;
  }

  PacketsQueue::iterator last = pos;
  while (last < (packets_queue_.end() - 1) && (*last) &&
         (*last)->vp8_descriptor().fragmentation_info !=
         Vp8Descriptor::LAST_FRAGMENT) {
    last++;
  }
  if (!(*last) || (*last)->vp8_descriptor().fragmentation_info !=
      Vp8Descriptor::LAST_FRAGMENT) {
    // We don't have last fragment.
    return;
  }

  // We've found first and last fragments, and we have all fragments in the
  // middle, so we can rebuild fill packet.
  RebuildVideoPacket(first, last);
}

void RtpVideoReader::RebuildVideoPacket(PacketsQueue::iterator first,
                                        PacketsQueue::iterator last) {
  VideoPacket* packet = new VideoPacket();

  // Set flags.
  if ((*first)->vp8_descriptor().frame_beginning)
    packet->set_flags(packet->flags() | VideoPacket::FIRST_PACKET);

  if ((*last)->header().marker)
    packet->set_flags(packet->flags() | VideoPacket::LAST_PACKET);

  // Rebuild packet content from the fragments.
  // TODO(sergeyu): Use CompoundBuffer inside of VideoPacket, so that we don't
  // need to memcopy any data.
  CompoundBuffer content;
  for (PacketsQueue::iterator it = first; it <= last; ++it) {
    content.Append((*it)->payload());

    // Delete packet because we don't need it anymore.
    delete *it;
    *it = NULL;
  }

  packet->mutable_data()->resize(content.total_bytes());
  content.CopyTo(const_cast<char*>(packet->mutable_data()->data()),
                 packet->data().size());

  // Set format.
  packet->mutable_format()->set_encoding(VideoPacketFormat::ENCODING_VP8);

  video_stub_->ProcessVideoPacket(packet, new DeleteTask<VideoPacket>(packet));
}

}  // namespace protocol
}  // namespace remoting
