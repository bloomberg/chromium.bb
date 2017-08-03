// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_METRICS_PUBLIC_CPP_UKM_ENTRY_BUILDER_H
#define SERVICES_METRICS_PUBLIC_CPP_UKM_ENTRY_BUILDER_H

#include <string>

#include "base/macros.h"
#include "services/metrics/public/cpp/metrics_export.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/metrics/public/interfaces/ukm_interface.mojom.h"

namespace ukm {

// The builder that builds UkmEntry and adds it to UkmRecorder.
// The example usage is:
//
// {
//   unique_ptr<UkmEntryBuilder> builder =
//       ukm_recorder->GetEntryBuilder(source_id, "PageLoad");
//   builder->AddMetric("NavigationStart", navigation_start_time);
//   builder->AddMetric("ResponseStart", response_start_time);
//   builder->AddMetric("FirstPaint", first_paint_time);
//   builder->AddMetric("FirstContentfulPaint", fcp_time);
// }
//
// When there exists an added metric, the builder will automatically add the
// UkmEntry to UkmService upon destruction when going out of scope.
class METRICS_EXPORT UkmEntryBuilder {
 public:
  using AddEntryCallback = base::Callback<void(mojom::UkmEntryPtr)>;
  UkmEntryBuilder(const AddEntryCallback& callback,
                  ukm::SourceId source_id,
                  const char* event_name);
  ~UkmEntryBuilder();

  // Add metric to the entry. A metric contains a metric name and value.
  void AddMetric(const char* metric_name, int64_t value);

 private:
  AddEntryCallback add_entry_callback_;
  mojom::UkmEntryPtr entry_;

  DISALLOW_COPY_AND_ASSIGN(UkmEntryBuilder);
};

}  // namespace ukm

#endif  // SERVICES_METRICS_PUBLIC_CPP_UKM_ENTRY_BUILDER_H
