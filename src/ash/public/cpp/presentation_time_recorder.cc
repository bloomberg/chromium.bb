// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/presentation_time_recorder.h"

#include <ostream>

#include "base/callback.h"
#include "ui/gfx/presentation_feedback.h"

namespace ash {

namespace {

bool report_immediately_for_test = false;

std::string ToFlagString(uint32_t flags) {
  std::string tmp;
  if (flags & gfx::PresentationFeedback::kVSync)
    tmp += "V,";
  if (flags & gfx::PresentationFeedback::kFailure)
    tmp += "F,";
  if (flags & gfx::PresentationFeedback::kHWClock)
    tmp += "HCL,";
  if (flags & gfx::PresentationFeedback::kHWCompletion)
    tmp += "HCO,";
  if (flags & gfx::PresentationFeedback::kZeroCopy)
    tmp += "Z";
  return tmp;
}

base::HistogramBase* CreateTimesHistogram(const char* name) {
  return base::Histogram::FactoryTimeGet(
      name, base::TimeDelta::FromMilliseconds(1),
      base::TimeDelta::FromMilliseconds(200), 50,
      base::HistogramBase::kUmaTargetedHistogramFlag);
}

}  // namespace

PresentationTimeRecorder::PresentationTimeRecorder(ui::Compositor* compositor)
    : compositor_(compositor), weak_ptr_factory_(this) {
  compositor_->AddObserver(this);
  VLOG(1) << "Start Recording Frame Time";
}

PresentationTimeRecorder::~PresentationTimeRecorder() {
  VLOG(1) << "Finished Recording FrameTime: average latency="
          << average_latency_ms() << "ms, max latency=" << max_latency_ms()
          << "ms, failure_ratio=" << failure_ratio();
  if (compositor_)
    compositor_->RemoveObserver(this);
}

bool PresentationTimeRecorder::RequestNext() {
  if (!compositor_)
    return false;

  if (state_ == REQUESTED)
    return false;

  const base::TimeTicks now = base::TimeTicks::Now();

  VLOG(1) << "Start Next (" << request_count_
          << ") state=" << (state_ == COMMITTED ? "Committed" : "Presented");
  state_ = REQUESTED;

  if (report_immediately_for_test) {
    state_ = COMMITTED;
    gfx::PresentationFeedback feedback;
    OnPresented(request_count_++, now, feedback);
    return true;
  }

  compositor_->RequestPresentationTimeForNextFrame(
      base::BindOnce(&PresentationTimeRecorder::OnPresented,
                     weak_ptr_factory_.GetWeakPtr(), request_count_++, now));
  return true;
}

void PresentationTimeRecorder::OnCompositingDidCommit(
    ui::Compositor* compositor) {
  state_ = COMMITTED;
}

void PresentationTimeRecorder::OnCompositingShuttingDown(
    ui::Compositor* compositor) {
  DCHECK_EQ(compositor_, compositor);
  compositor_->RemoveObserver(this);
  compositor_ = nullptr;
}

void PresentationTimeRecorder::OnPresented(
    int count,
    base::TimeTicks requested_time,
    const gfx::PresentationFeedback& feedback) {
  if (state_ == COMMITTED)
    state_ = PRESENTED;

  if (feedback.flags & gfx::PresentationFeedback::kFailure) {
    failure_count_++;
    VLOG(1) << "PresentationFailed (" << count << "):"
            << ", flags=" << ToFlagString(feedback.flags);
    return;
  }

  const base::TimeDelta delta = feedback.timestamp - requested_time;
  if (delta.InMilliseconds() > max_latency_ms_)
    max_latency_ms_ = delta.InMilliseconds();

  success_count_++;
  total_latency_ms_ += delta.InMilliseconds();
  ReportTime(delta);
  VLOG(1) << "OnPresented (" << count << "):" << delta.InMilliseconds()
          << ",flags=" << ToFlagString(feedback.flags);
}

// static
void PresentationTimeRecorder::SetReportPresentationTimeImmediatelyForTest(
    bool enable) {
  report_immediately_for_test = enable;
}

// static
bool PresentationTimeRecorder::GetReportPresentationTimeImmediatelyForTest() {
  return report_immediately_for_test;
}

PresentationTimeHistogramRecorder::PresentationTimeHistogramRecorder(
    ui::Compositor* compositor,
    const char* presentation_time_histogram_name,
    const char* max_latency_histogram_name)
    : PresentationTimeRecorder(compositor),
      presentation_time_histogram_(
          CreateTimesHistogram(presentation_time_histogram_name)),
      max_latency_histogram_name_(max_latency_histogram_name) {}

PresentationTimeHistogramRecorder::~PresentationTimeHistogramRecorder() {
  if (success_count() > 0) {
    CreateTimesHistogram(max_latency_histogram_name_.c_str())
        ->AddTimeMillisecondsGranularity(
            base::TimeDelta::FromMilliseconds(max_latency_ms()));
  }
}

// PresentationTimeRecorder:
void PresentationTimeHistogramRecorder::ReportTime(base::TimeDelta delta) {
  presentation_time_histogram_->AddTimeMillisecondsGranularity(delta);
}

}  // namespace ash
