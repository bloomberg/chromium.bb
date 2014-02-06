// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/plugin/media_source_video_renderer.h"

#include <string.h>

#include "base/callback_helpers.h"
#include "base/logging.h"
#include "remoting/proto/video.pb.h"
#include "remoting/protocol/session_config.h"
#include "third_party/libwebm/source/mkvmuxer.hpp"

namespace remoting {

static int kFrameIntervalNs = 1000000;

class MediaSourceVideoRenderer::VideoWriter : public mkvmuxer::IMkvWriter {
 public:
  typedef std::vector<uint8_t> DataBuffer;

  VideoWriter(const webrtc::DesktopSize& frame_size);
  virtual ~VideoWriter();

  const webrtc::DesktopSize& size() { return frame_size_; }
  int64_t last_frame_timestamp() { return timecode_ - kFrameIntervalNs; }

  // IMkvWriter interface.
  virtual mkvmuxer::int32 Write(const void* buf, mkvmuxer::uint32 len) OVERRIDE;
  virtual mkvmuxer::int64 Position() const OVERRIDE;
  virtual mkvmuxer::int32 Position(mkvmuxer::int64 position) OVERRIDE;
  virtual bool Seekable() const OVERRIDE;
  virtual void ElementStartNotify(mkvmuxer::uint64 element_id,
                                  mkvmuxer::int64 position) OVERRIDE;

  scoped_ptr<DataBuffer> OnVideoFrame(const std::string& video_data);

 private:
  webrtc::DesktopSize frame_size_;
  scoped_ptr<DataBuffer> output_data_;
  int64_t position_;
  scoped_ptr<mkvmuxer::Segment> segment_;
  int64_t timecode_;
};

MediaSourceVideoRenderer::VideoWriter::VideoWriter(
    const webrtc::DesktopSize& frame_size)
    : frame_size_(frame_size),
      position_(0),
      timecode_(0) {
  segment_.reset(new mkvmuxer::Segment());
  segment_->Init(this);
  segment_->set_mode(mkvmuxer::Segment::kLive);
  segment_->AddVideoTrack(frame_size_.width(), frame_size_.height(), 1);
  mkvmuxer::SegmentInfo* const info = segment_->GetSegmentInfo();
  info->set_writing_app("ChromotingViewer");
  info->set_muxing_app("ChromotingViewer");
}

MediaSourceVideoRenderer::VideoWriter::~VideoWriter() {}

mkvmuxer::int32 MediaSourceVideoRenderer::VideoWriter::Write(
    const void* buf,
    mkvmuxer::uint32 len) {
  output_data_->insert(output_data_->end(),
                       reinterpret_cast<const char*>(buf),
                       reinterpret_cast<const char*>(buf) + len);
  position_ += len;
  return 0;
}

mkvmuxer::int64 MediaSourceVideoRenderer::VideoWriter::Position() const {
  return position_;
}

mkvmuxer::int32 MediaSourceVideoRenderer::VideoWriter::Position(
    mkvmuxer::int64 position) {
  return -1;
}

bool MediaSourceVideoRenderer::VideoWriter::Seekable() const {
  return false;
}

void MediaSourceVideoRenderer::VideoWriter::ElementStartNotify(
    mkvmuxer::uint64 element_id,
    mkvmuxer::int64 position) {
}

scoped_ptr<MediaSourceVideoRenderer::VideoWriter::DataBuffer>
MediaSourceVideoRenderer::VideoWriter::OnVideoFrame(
    const std::string& video_data) {
  DCHECK(!output_data_);

  output_data_.reset(new DataBuffer());
  bool first_frame = (timecode_ == 0);
  segment_->AddFrame(reinterpret_cast<const uint8_t*>(video_data.data()),
                     video_data.size(), 1, timecode_, first_frame);
  timecode_ += kFrameIntervalNs;
  return output_data_.Pass();
}

MediaSourceVideoRenderer::MediaSourceVideoRenderer(Delegate* delegate)
    : delegate_(delegate),
      latest_sequence_number_(0) {
}

MediaSourceVideoRenderer::~MediaSourceVideoRenderer() {}

void MediaSourceVideoRenderer::Initialize(
    const protocol::SessionConfig& config) {
  DCHECK_EQ(config.video_config().codec, protocol::ChannelConfig::CODEC_VP8);
}

ChromotingStats* MediaSourceVideoRenderer::GetStats() {
  return &stats_;
}

void MediaSourceVideoRenderer::ProcessVideoPacket(
    scoped_ptr<VideoPacket> packet,
    const base::Closure& done) {
  base::ScopedClosureRunner done_runner(done);

  // Don't need to do anything if the packet is empty. Host sends empty video
  // packets when the screen is not changing.
  if (!packet->data().size())
    return;

  // Update statistics.
  stats_.video_frame_rate()->Record(1);
  stats_.video_bandwidth()->Record(packet->data().size());
  if (packet->has_capture_time_ms())
    stats_.video_capture_ms()->Record(packet->capture_time_ms());
  if (packet->has_encode_time_ms())
    stats_.video_encode_ms()->Record(packet->encode_time_ms());
  if (packet->has_client_sequence_number() &&
      packet->client_sequence_number() > latest_sequence_number_) {
    latest_sequence_number_ = packet->client_sequence_number();
    base::TimeDelta round_trip_latency =
        base::Time::Now() -
        base::Time::FromInternalValue(packet->client_sequence_number());
    stats_.round_trip_ms()->Record(round_trip_latency.InMilliseconds());
  }

  bool media_source_needs_reset = false;

  webrtc::DesktopSize frame_size(packet->format().screen_width(),
                                 packet->format().screen_height());
  if (!writer_ ||
      (!writer_->size().equals(frame_size) && !frame_size.is_empty())) {
    media_source_needs_reset = true;
    writer_.reset(new VideoWriter(frame_size));
    delegate_->OnMediaSourceReset("video/webm; codecs=\"vp8\"");
  }

  webrtc::DesktopVector frame_dpi(packet->format().x_dpi(),
                                  packet->format().y_dpi());
  if (media_source_needs_reset || !frame_dpi_.equals(frame_dpi)) {
    frame_dpi_ = frame_dpi;
    delegate_->OnMediaSourceSize(frame_size, frame_dpi);
  }

  // Update the desktop shape region.
  webrtc::DesktopRegion desktop_shape;
  if (packet->has_use_desktop_shape()) {
    for (int i = 0; i < packet->desktop_shape_rects_size(); ++i) {
      Rect remoting_rect = packet->desktop_shape_rects(i);
      desktop_shape.AddRect(webrtc::DesktopRect::MakeXYWH(
          remoting_rect.x(), remoting_rect.y(),
          remoting_rect.width(), remoting_rect.height()));
    }
  } else {
    // Fallback for the case when the host didn't include the desktop shape.
    desktop_shape =
        webrtc::DesktopRegion(webrtc::DesktopRect::MakeSize(frame_size));
  }

  if (!desktop_shape_.Equals(desktop_shape)) {
    desktop_shape_.Swap(&desktop_shape);
    delegate_->OnMediaSourceShape(desktop_shape_);
  }

  scoped_ptr<VideoWriter::DataBuffer> buffer =
      writer_->OnVideoFrame(packet->data());
  delegate_->OnMediaSourceData(&(*(buffer->begin())), buffer->size());
}

}  // namespace remoting
