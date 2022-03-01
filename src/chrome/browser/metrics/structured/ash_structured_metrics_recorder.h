// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_STRUCTURED_ASH_STRUCTURED_METRICS_RECORDER_H_
#define CHROME_BROWSER_METRICS_STRUCTURED_ASH_STRUCTURED_METRICS_RECORDER_H_

#include "chromeos/crosapi/mojom/structured_metrics_service.mojom.h"
#include "components/metrics/structured/event.h"
#include "components/metrics/structured/event_base.h"
#include "components/metrics/structured/structured_metrics_client.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace metrics {
namespace structured {

// Recording delegate for Ash Chrome.
//
// Although the main service also lives in Ash, this recorder sends events using
// a mojo pipe. Instantiating a mojo pipe adds little overhead and provides lots
// of benefits out of the box (ie message buffer).
class AshStructuredMetricsRecorder
    : public StructuredMetricsClient::RecordingDelegate {
 public:
  AshStructuredMetricsRecorder();
  AshStructuredMetricsRecorder(const AshStructuredMetricsRecorder& recorder) =
      delete;
  AshStructuredMetricsRecorder& operator=(
      const AshStructuredMetricsRecorder& recorder) = delete;
  ~AshStructuredMetricsRecorder() override;

  // Sets up the recorder. This should be called after CrosApi is initialized,
  // which is done in PreProfileInit() of the browser process setup.
  void Initialize();

  // RecordingDelegate:
  void RecordEvent(Event&& event) override;
  void Record(EventBase&& event_base) override;
  bool IsReadyToRecord() const override;

 private:
  mojo::Remote<crosapi::mojom::StructuredMetricsService> remote_;

  bool is_initialized_ = false;
};

}  // namespace structured
}  // namespace metrics

#endif  // CHROME_BROWSER_METRICS_STRUCTURED_ASH_STRUCTURED_METRICS_RECORDER_H_
