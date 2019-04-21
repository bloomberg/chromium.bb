// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/ukm_recorder_interface.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_binding.h"

namespace metrics {

namespace {

// MetricsMojoService is a simple Service which provides metrics interfaces.
// This should be embedded in the browser process, which will create
// appropriate delegates for UkmRecorder::Get().
class MetricsMojoService : public service_manager::Service {
 public:
  explicit MetricsMojoService(service_manager::mojom::ServiceRequest request)
      : service_binding_(this, std::move(request)) {
    registry_.AddInterface(
        base::Bind(&UkmRecorderInterface::Create, ukm::UkmRecorder::Get()));
  }

  ~MetricsMojoService() final = default;

 private:
  // service_manager::Service:.
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle handle) override {
    registry_.BindInterface(interface_name, std::move(handle));
  }

  service_manager::ServiceBinding service_binding_;
  service_manager::BinderRegistry registry_;

  DISALLOW_COPY_AND_ASSIGN(MetricsMojoService);
};

}  // namespace

std::unique_ptr<service_manager::Service> CreateMetricsService(
    service_manager::mojom::ServiceRequest request) {
  return std::make_unique<MetricsMojoService>(std::move(request));
}

}  // namespace metrics
