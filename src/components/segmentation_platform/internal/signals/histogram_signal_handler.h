// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SIGNALS_HISTOGRAM_SIGNAL_HANDLER_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SIGNALS_HISTOGRAM_SIGNAL_HANDLER_H_

#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/statistics_recorder.h"
#include "components/segmentation_platform/internal/proto/types.pb.h"

namespace segmentation_platform {

class SignalDatabase;

// Responsible for listening to histogram sample collection events and
// persisting them to the internal database for future processing.
class HistogramSignalHandler {
 public:
  explicit HistogramSignalHandler(SignalDatabase* signal_database);
  virtual ~HistogramSignalHandler();

  // Disallow copy/assign.
  HistogramSignalHandler(const HistogramSignalHandler&) = delete;
  HistogramSignalHandler& operator=(const HistogramSignalHandler&) = delete;

  // Called to notify about a set of histograms which the segmentation models
  // care about.
  virtual void SetRelevantHistograms(
      const std::set<std::pair<std::string, proto::SignalType>>& histograms);

  // Called to enable or disable metrics collection for segmentation platform.
  virtual void EnableMetrics(bool enable_metrics);

 private:
  void OnHistogramSample(proto::SignalType signal_type,
                         const char* histogram_name,
                         uint64_t name_hash,
                         base::HistogramBase::Sample sample);

  // The database storing relevant histogram samples.
  raw_ptr<SignalDatabase> db_;

  // Whether or not the segmentation platform should record metrics events.
  bool metrics_enabled_;

  // Tracks the histogram names we are currently listening to along with their
  // corresponding observers.
  std::map<
      std::string,
      std::unique_ptr<base::StatisticsRecorder::ScopedHistogramSampleObserver>>
      histogram_observers_;

  base::WeakPtrFactory<HistogramSignalHandler> weak_ptr_factory_{this};
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SIGNALS_HISTOGRAM_SIGNAL_HANDLER_H_
