// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_BASE_LEGACYMETRICS_CLIENT_H_
#define FUCHSIA_BASE_LEGACYMETRICS_CLIENT_H_

#include <fuchsia/legacymetrics/cpp/fidl.h>

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/timer/timer.h"
#include "fuchsia/base/legacymetrics_user_event_recorder.h"

namespace cr_fuchsia {

// Used to report events & histogram data to the
// fuchsia.legacymetrics.MetricsRecorder service.
// LegacyMetricsClient must be Start()ed on an IO-capable sequence.
// Cannot be used in conjunction with other metrics reporting services.
// Must be constructed, used, and destroyed on the same sequence.
class LegacyMetricsClient {
 public:
  // Maximum number of Events to send to Record() at a time, so as to not exceed
  // the 64KB FIDL maximum message size.
  static constexpr size_t kMaxBatchSize = 50;

  using ReportAdditionalMetricsCallback = base::RepeatingCallback<void(
      base::OnceCallback<void(std::vector<fuchsia::legacymetrics::Event>)>)>;

  LegacyMetricsClient();
  ~LegacyMetricsClient();

  explicit LegacyMetricsClient(const LegacyMetricsClient&) = delete;
  LegacyMetricsClient& operator=(const LegacyMetricsClient&) = delete;

  // Starts buffering data and schedules metric reporting after every
  // |report_interval|.
  void Start(base::TimeDelta report_interval);

  // Sets an asynchronous |callback| to be invoked just prior to reporting,
  // allowing users to asynchronously gather and provide additional custom
  // metrics. |callback| will receive the list of metrics when they are ready.
  // Reporting is paused until |callback| is fulfilled.
  // If used, then this method must be called before calling Start().
  void SetReportAdditionalMetricsCallback(
      ReportAdditionalMetricsCallback callback);

 private:
  void ScheduleNextReport();
  void StartReport();
  void Report(std::vector<fuchsia::legacymetrics::Event> additional_metrics);
  void OnMetricsRecorderDisconnected(zx_status_t status);

  // Incrementally sends the contents of |buffer| to |metrics_recorder|, and
  // invokes |done_cb| when finished.
  void DrainBuffer(std::vector<fuchsia::legacymetrics::Event> buffer);

  base::TimeDelta report_interval_;
  ReportAdditionalMetricsCallback report_additional_callback_;
  std::unique_ptr<LegacyMetricsUserActionRecorder> user_events_recorder_;

  fuchsia::legacymetrics::MetricsRecorderPtr metrics_recorder_;
  base::RetainingOneShotTimer timer_;
  SEQUENCE_CHECKER(sequence_checker_);

  // Prevents use-after-free if |report_additional_callback_| is invoked after
  // |this| is destroyed.
  base::WeakPtrFactory<LegacyMetricsClient> weak_factory_{this};
};

}  // namespace cr_fuchsia

#endif  // FUCHSIA_BASE_LEGACYMETRICS_CLIENT_H_
