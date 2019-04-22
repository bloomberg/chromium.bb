// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "third_party/blink/renderer/core/content_capture/content_capture_task_histogram_reporter.h"

namespace blink {

// static
constexpr char ContentCaptureTaskHistogramReporter::kCaptureContentTime[];
constexpr char ContentCaptureTaskHistogramReporter::kCaptureOneContentTime[];
constexpr char ContentCaptureTaskHistogramReporter::kCaptureContentDelayTime[];
constexpr char ContentCaptureTaskHistogramReporter::kSendContentTime[];
constexpr char ContentCaptureTaskHistogramReporter::kSentContentCount[];

ContentCaptureTaskHistogramReporter::ContentCaptureTaskHistogramReporter()
    : capture_content_delay_time_histogram_(kCaptureContentDelayTime,
                                            500,
                                            30000,
                                            50),
      capture_content_time_histogram_(kCaptureContentTime, 0, 50000, 50),
      capture_one_content_time_histogram_(kCaptureOneContentTime, 0, 50000, 50),
      send_content_time_histogram_(kSendContentTime, 0, 50000, 50),
      sent_content_count_histogram_(kSentContentCount, 0, 10000, 50) {}

ContentCaptureTaskHistogramReporter::~ContentCaptureTaskHistogramReporter() =
    default;

void ContentCaptureTaskHistogramReporter::OnContentChanged() {
  if (content_change_time_)
    return;
  content_change_time_ = WTF::CurrentTimeTicks();
}

void ContentCaptureTaskHistogramReporter::OnCaptureContentStarted() {
  capture_content_start_time_ = WTF::CurrentTimeTicks();
}

void ContentCaptureTaskHistogramReporter::OnCaptureContentEnded(
    size_t captured_content_count) {
  if (!captured_content_count) {
    // We captured nothing for the recorded content change, reset the time to
    // start again.
    content_change_time_.reset();
    return;
  }
  // Gives content_change_time_ to the change occurred while sending the
  // content.
  captured_content_change_time_ = std::move(content_change_time_);
  base::TimeDelta delta = WTF::CurrentTimeTicks() - capture_content_start_time_;
  capture_content_time_histogram_.CountMicroseconds(delta);
  capture_one_content_time_histogram_.CountMicroseconds(delta /
                                                        captured_content_count);
}

void ContentCaptureTaskHistogramReporter::OnSendContentStarted() {
  send_content_start_time_ = WTF::CurrentTimeTicks();
}

void ContentCaptureTaskHistogramReporter::OnSendContentEnded(
    size_t sent_content_count) {
  TimeTicks now = WTF::CurrentTimeTicks();
  if (captured_content_change_time_) {
    TimeTicks content_change_time = captured_content_change_time_.value();
    captured_content_change_time_.reset();
    capture_content_delay_time_histogram_.CountMilliseconds(
        now - content_change_time);
  }
  if (!sent_content_count)
    return;
  send_content_time_histogram_.CountMicroseconds(now -
                                                 send_content_start_time_);
}

void ContentCaptureTaskHistogramReporter::RecordsSentContentCountPerDocument(
    size_t sent_content_count) {
  sent_content_count_histogram_.Count(sent_content_count);
}

}  // namespace blink
