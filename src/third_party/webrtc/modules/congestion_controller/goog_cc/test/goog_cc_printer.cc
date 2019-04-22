/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/congestion_controller/goog_cc/test/goog_cc_printer.h"

#include <math.h>

#include "absl/types/optional.h"
#include "modules/congestion_controller/goog_cc/alr_detector.h"
#include "modules/congestion_controller/goog_cc/delay_based_bwe.h"
#include "modules/congestion_controller/goog_cc/trendline_estimator.h"
#include "modules/remote_bitrate_estimator/aimd_rate_control.h"
#include "rtc_base/checks.h"

namespace webrtc {

GoogCcStatePrinter::GoogCcStatePrinter() = default;
GoogCcStatePrinter::~GoogCcStatePrinter() = default;

void GoogCcStatePrinter::Attach(GoogCcNetworkController* controller) {
  controller_ = controller;
}

bool GoogCcStatePrinter::Attached() const {
  return controller_ != nullptr;
}

void GoogCcStatePrinter::PrintHeaders(RtcEventLogOutput* out) {
  out->Write(
      "rate_control_state stable_estimate alr_state"
      " trendline trendline_modified_offset trendline_offset_threshold");
}

void GoogCcStatePrinter::PrintValues(RtcEventLogOutput* out) {
  RTC_CHECK(controller_);
  auto* detector = controller_->delay_based_bwe_->delay_detector_.get();
  auto* trendline_estimator = reinterpret_cast<TrendlineEstimator*>(detector);
  LogWriteFormat(
      out, "%i %f %i %.6lf %.6lf %.6lf",
      controller_->delay_based_bwe_->rate_control_.rate_control_state_,
      controller_->delay_based_bwe_->rate_control_.link_capacity_.estimate_kbps_
              .value_or(NAN) *
          1000 / 8,
      controller_->alr_detector_->alr_started_time_ms_.has_value(),
      trendline_estimator->prev_trend_,
      trendline_estimator->prev_modified_trend_,
      trendline_estimator->threshold_);
}

NetworkControlUpdate GoogCcStatePrinter::GetState(Timestamp at_time) const {
  RTC_CHECK(controller_);
  return controller_->GetNetworkState(at_time);
}

GoogCcDebugFactory::GoogCcDebugFactory(RtcEventLog* event_log,
                                       GoogCcStatePrinter* printer)
    : GoogCcNetworkControllerFactory(event_log), printer_(printer) {}

std::unique_ptr<NetworkControllerInterface> GoogCcDebugFactory::Create(
    NetworkControllerConfig config) {
  RTC_CHECK(controller_ == nullptr);
  auto controller = GoogCcNetworkControllerFactory::Create(config);
  controller_ = static_cast<GoogCcNetworkController*>(controller.get());
  printer_->Attach(controller_);
  return controller;
}

GoogCcFeedbackDebugFactory::GoogCcFeedbackDebugFactory(
    RtcEventLog* event_log,
    GoogCcStatePrinter* printer)
    : GoogCcFeedbackNetworkControllerFactory(event_log), printer_(printer) {}

std::unique_ptr<NetworkControllerInterface> GoogCcFeedbackDebugFactory::Create(
    NetworkControllerConfig config) {
  RTC_CHECK(controller_ == nullptr);
  auto controller = GoogCcFeedbackNetworkControllerFactory::Create(config);
  controller_ = static_cast<GoogCcNetworkController*>(controller.get());
  printer_->Attach(controller_);
  return controller;
}

}  // namespace webrtc
