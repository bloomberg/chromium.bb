// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/metrics/public/cpp/ukm_entry_builder.h"

#include <memory>

#include "base/metrics/metrics_hashes.h"
#include "services/metrics/public/interfaces/ukm_interface.mojom.h"

namespace ukm {

UkmEntryBuilder::UkmEntryBuilder(
    const UkmEntryBuilder::AddEntryCallback& callback,
    ukm::SourceId source_id,
    const char* event_name)
    : add_entry_callback_(callback), entry_(mojom::UkmEntry::New()) {
  entry_->source_id = source_id;
  entry_->event_hash = base::HashMetricName(event_name);
}

UkmEntryBuilder::~UkmEntryBuilder() {
  add_entry_callback_.Run(std::move(entry_));
}

void UkmEntryBuilder::AddMetric(const char* metric_name, int64_t value) {
  entry_->metrics.emplace_back(
      mojom::UkmMetric::New(base::HashMetricName(metric_name), value));
}

}  // namespace ukm
