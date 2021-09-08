// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_METRICS_HISTOGRAMS_MONITOR_H_
#define CONTENT_BROWSER_METRICS_HISTOGRAMS_MONITOR_H_

#include <map>

#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "content/common/content_export.h"

namespace content {

// This class handles the monitoring feature of chrome://histograms page,
// which allows the page to be updated with histograms logged since
// the monitoring started.
class CONTENT_EXPORT HistogramsMonitor {
 public:
  HistogramsMonitor();
  ~HistogramsMonitor();

  HistogramsMonitor(const HistogramsMonitor&) = delete;
  HistogramsMonitor& operator=(const HistogramsMonitor&) = delete;

  // Fetches and records a snapshot of the current histograms,
  // as the baseline to compare against in subsequent calls to GetDiff().
  void StartMonitoring(const std::string& query);

  // Gets the histogram diffs between the current histograms and the snapshot
  // recorded in StartMonitoring().
  base::ListValue GetDiff();

 private:
  // Imports histograms from the StatisticsRecorder.
  // Also contacts all processes, and gets them to upload to the browser any/all
  // changes to histograms.
  void FetchHistograms();

  // Gets the difference between the histograms argument and the stored snapshot
  // recorded in StartMonitoring().
  base::ListValue GetDiffInternal(
      const base::StatisticsRecorder::Histograms& histograms);

  std::string query_;
  std::map<std::string, std::unique_ptr<base::HistogramSamples>>
      histograms_snapshot_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_METRICS_HISTOGRAMS_MONITOR_H_
