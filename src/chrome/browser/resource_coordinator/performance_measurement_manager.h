// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_PERFORMANCE_MEASUREMENT_MANAGER_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_PERFORMANCE_MEASUREMENT_MANAGER_H_

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "base/sequence_checker.h"
#include "chrome/browser/resource_coordinator/page_signal_receiver.h"

namespace resource_coordinator {

class RenderProcessProbe;

// This class observes the PageAlmostIdle signal from the PageSignalGenerator
// and uses it to fire off performance measurements for tabs that have reached
// idle state.
class PerformanceMeasurementManager : public PageSignalObserver {
 public:
  PerformanceMeasurementManager(PageSignalReceiver* receiver,
                                RenderProcessProbe* render_process_probe);
  ~PerformanceMeasurementManager() override;

  // PageSignalObserver overrides.
  void OnPageAlmostIdle(
      content::WebContents* web_contents,
      const PageNavigationIdentity& page_navigation_id) override;

 private:
  ScopedObserver<PageSignalReceiver, PerformanceMeasurementManager>
      scoped_observer_;
  PageSignalReceiver* page_signal_receiver_;
  RenderProcessProbe* render_process_probe_;

  DISALLOW_COPY_AND_ASSIGN(PerformanceMeasurementManager);
};

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_PERFORMANCE_MEASUREMENT_MANAGER_H_
