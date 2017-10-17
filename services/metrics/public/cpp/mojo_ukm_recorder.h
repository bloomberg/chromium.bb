// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_METRICS_PUBLIC_CPP_MOJO_UKM_RECORDER_H_
#define SERVICES_METRICS_PUBLIC_CPP_MOJO_UKM_RECORDER_H_

#include "services/metrics/public/cpp/metrics_export.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/interfaces/ukm_interface.mojom.h"

namespace ukm {

/**
 * A helper wrapper that lets UKM data be recorded on other processes with the
 * same interface that is used in the browser process.
 *
 * Usage Example:
 *
 *  ukm::mojom::UkmRecorderInterfacePtr interface;
 *  content::RenderThread::Get()->GetConnector()->BindInterface(
 *      content::mojom::kBrowserServiceName, mojo::MakeRequest(&interface));
 *  ukm::MojoUkmRecorder ukm_recorder(std::move(interface));
 *  ukm::builders::MyEvent(source_id)
 *      .SetMyMetric(metric_value)
 *      .Record(ukm_recorder);
 */
class METRICS_EXPORT MojoUkmRecorder : public UkmRecorder {
 public:
  explicit MojoUkmRecorder(mojom::UkmRecorderInterfacePtr interface);
  ~MojoUkmRecorder() override;

  // UkmRecorder:
  void UpdateSourceURL(SourceId source_id, const GURL& url) override;

 private:
  // UkmRecorder:
  void AddEntry(mojom::UkmEntryPtr entry) override;

  mojom::UkmRecorderInterfacePtr interface_;

  DISALLOW_COPY_AND_ASSIGN(MojoUkmRecorder);
};

}  // namespace ukm

#endif  // SERVICES_METRICS_PUBLIC_CPP_MOJO_UKM_RECORDER_H_
