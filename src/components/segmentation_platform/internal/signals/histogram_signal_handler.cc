// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/signals/histogram_signal_handler.h"

#include "base/callback_helpers.h"
#include "base/metrics/metrics_hashes.h"
#include "components/segmentation_platform/internal/database/signal_database.h"

namespace segmentation_platform {

HistogramSignalHandler::HistogramSignalHandler(SignalDatabase* signal_database)
    : db_(signal_database), metrics_enabled_(false) {}

HistogramSignalHandler::~HistogramSignalHandler() = default;

void HistogramSignalHandler::SetRelevantHistograms(
    const std::set<std::pair<std::string, proto::SignalType>>& histograms) {
  histogram_observers_.clear();
  for (const auto& pair : histograms) {
    const auto& histogram_name = pair.first;
    proto::SignalType signal_type = pair.second;
    auto histogram_observer = std::make_unique<
        base::StatisticsRecorder::ScopedHistogramSampleObserver>(
        histogram_name,
        base::BindRepeating(&HistogramSignalHandler::OnHistogramSample,
                            weak_ptr_factory_.GetWeakPtr(), signal_type));
    histogram_observers_[histogram_name] = std::move(histogram_observer);
  }
}

void HistogramSignalHandler::EnableMetrics(bool enable_metrics) {
  if (metrics_enabled_ == enable_metrics)
    return;

  metrics_enabled_ = enable_metrics;
}

void HistogramSignalHandler::OnHistogramSample(
    proto::SignalType signal_type,
    const char* histogram_name,
    uint64_t name_hash,
    base::HistogramBase::Sample sample) {
  if (!metrics_enabled_)
    return;

  db_->WriteSample(signal_type, name_hash, sample, base::DoNothing());
}

}  // namespace segmentation_platform
