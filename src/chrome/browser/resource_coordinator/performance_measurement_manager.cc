// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/performance_measurement_manager.h"

#include "chrome/browser/resource_coordinator/render_process_probe.h"
#include "chrome/browser/resource_coordinator/tab_load_tracker.h"

namespace resource_coordinator {

using LoadingState = TabLoadTracker::LoadingState;

PerformanceMeasurementManager::PerformanceMeasurementManager(
    PageSignalReceiver* page_signal_receiver,
    RenderProcessProbe* render_process_probe)
    : scoped_observer_(this),
      page_signal_receiver_(page_signal_receiver),
      render_process_probe_(render_process_probe) {
  scoped_observer_.Add(page_signal_receiver);
}

PerformanceMeasurementManager::~PerformanceMeasurementManager() = default;

void PerformanceMeasurementManager::OnPageAlmostIdle(
    content::WebContents* web_contents,
    const PageNavigationIdentity& page_navigation_id) {
  if (page_signal_receiver_->GetNavigationIDForWebContents(web_contents) ==
      page_navigation_id.navigation_id) {
    render_process_probe_->StartSingleGather();
  }
}

}  // namespace resource_coordinator
