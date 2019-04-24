/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/scenario/quality_stats.h"

#include <utility>

#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "rtc_base/checks.h"
#include "rtc_base/event.h"

namespace webrtc {
namespace test {

VideoQualityAnalyzer::VideoQualityAnalyzer(
    std::unique_ptr<RtcEventLogOutput> writer,
    std::function<void(const VideoFrameQualityInfo&)> frame_info_handler)
    : writer_(std::move(writer)), task_queue_("VideoAnalyzer") {
  if (writer_) {
    PrintHeaders();
    frame_info_handlers_.push_back(
        [this](const VideoFrameQualityInfo& info) { PrintFrameInfo(info); });
  }
  if (frame_info_handler)
    frame_info_handlers_.push_back(frame_info_handler);
}

VideoQualityAnalyzer::~VideoQualityAnalyzer() {
  rtc::Event event;
  task_queue_.PostTask([&event] { event.Set(); });
  event.Wait(rtc::Event::kForever);
}

void VideoQualityAnalyzer::OnCapturedFrame(const VideoFrame& frame) {
  VideoFrame copy = frame;
  task_queue_.PostTask([this, copy]() mutable {
    captured_frames_.push_back(std::move(copy));
  });
}

void VideoQualityAnalyzer::OnDecodedFrame(const VideoFrame& frame) {
  VideoFrame decoded = frame;
  RTC_CHECK(frame.ntp_time_ms());
  RTC_CHECK(frame.timestamp());
  task_queue_.PostTask([this, decoded] {
    // TODO(srte): Add detection and handling of lost frames.
    RTC_CHECK(!captured_frames_.empty());
    VideoFrame captured = std::move(captured_frames_.front());
    captured_frames_.pop_front();
    VideoFrameQualityInfo decoded_info =
        VideoFrameQualityInfo{Timestamp::us(captured.timestamp_us()),
                              Timestamp::ms(decoded.timestamp() / 90.0),
                              Timestamp::ms(decoded.render_time_ms()),
                              decoded.width(),
                              decoded.height(),
                              I420PSNR(&captured, &decoded)};
    for (auto& handler : frame_info_handlers_)
      handler(decoded_info);
  });
}

bool VideoQualityAnalyzer::Active() const {
  return !frame_info_handlers_.empty();
}

void VideoQualityAnalyzer::PrintHeaders() {
  writer_->Write("capt recv_capt render width height psnr\n");
}

void VideoQualityAnalyzer::PrintFrameInfo(const VideoFrameQualityInfo& sample) {
  LogWriteFormat(writer_.get(), "%.3f %.3f %.3f %i %i %.3f\n",
                 sample.capture_time.seconds<double>(),
                 sample.received_capture_time.seconds<double>(),
                 sample.render_time.seconds<double>(), sample.width,
                 sample.height, sample.psnr);
}

void VideoQualityStats::HandleFrameInfo(VideoFrameQualityInfo sample) {
  total++;
  if (sample.render_time.IsInfinite()) {
    ++lost;
  } else {
    ++valid;
    end_to_end_seconds.AddSample(
        (sample.render_time - sample.capture_time).seconds<double>());
    psnr.AddSample(sample.psnr);
  }
}

ForwardingCapturedFrameTap::ForwardingCapturedFrameTap(
    Clock* clock,
    VideoQualityAnalyzer* analyzer,
    rtc::VideoSourceInterface<VideoFrame>* source)
    : clock_(clock), analyzer_(analyzer), source_(source) {}

ForwardingCapturedFrameTap::~ForwardingCapturedFrameTap() {}

void ForwardingCapturedFrameTap::OnFrame(const VideoFrame& frame) {
  RTC_CHECK(sink_);
  VideoFrame copy = frame;
  if (frame.ntp_time_ms() == 0)
    copy.set_ntp_time_ms(clock_->CurrentNtpInMilliseconds());
  copy.set_timestamp(copy.ntp_time_ms() * 90);
  analyzer_->OnCapturedFrame(copy);
  sink_->OnFrame(copy);
}
void ForwardingCapturedFrameTap::OnDiscardedFrame() {
  RTC_CHECK(sink_);
  discarded_count_++;
  sink_->OnDiscardedFrame();
}

void ForwardingCapturedFrameTap::AddOrUpdateSink(
    VideoSinkInterface<VideoFrame>* sink,
    const rtc::VideoSinkWants& wants) {
  sink_ = sink;
  source_->AddOrUpdateSink(this, wants);
}
void ForwardingCapturedFrameTap::RemoveSink(
    VideoSinkInterface<VideoFrame>* sink) {
  source_->RemoveSink(this);
  sink_ = nullptr;
}

DecodedFrameTap::DecodedFrameTap(VideoQualityAnalyzer* analyzer)
    : analyzer_(analyzer) {}

void DecodedFrameTap::OnFrame(const VideoFrame& frame) {
  analyzer_->OnDecodedFrame(frame);
}

}  // namespace test
}  // namespace webrtc
