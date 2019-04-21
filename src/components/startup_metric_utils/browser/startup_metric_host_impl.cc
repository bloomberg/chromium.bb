// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/startup_metric_utils/browser/startup_metric_host_impl.h"

#include <memory>

#include "components/startup_metric_utils/browser/startup_metric_utils.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace startup_metric_utils {

StartupMetricHostImpl::StartupMetricHostImpl() = default;

StartupMetricHostImpl::~StartupMetricHostImpl() = default;

// static
void StartupMetricHostImpl::Create(
    mojom::StartupMetricHostRequest request) {
  mojo::MakeStrongBinding(std::make_unique<StartupMetricHostImpl>(),
                          std::move(request));
}

void StartupMetricHostImpl::RecordRendererMainEntryTime(
    base::TimeTicks renderer_main_entry_time) {
  startup_metric_utils::RecordRendererMainEntryTime(renderer_main_entry_time);
}

}  // namespace startup_metric_utils
