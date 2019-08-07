// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_HIT_TEST_REGION_OBSERVER_H_
#define CONTENT_PUBLIC_TEST_HIT_TEST_REGION_OBSERVER_H_

#include <memory>
#include <vector>

#include "base/optional.h"
#include "base/run_loop.h"
#include "components/viz/common/hit_test/aggregated_hit_test_region.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/host/hit_test/hit_test_region_observer.h"

namespace content {
class RenderFrameHost;
class WebContents;

// TODO(jonross): Remove these once Viz Hit Testing is on by default and the
// legacy content::browser_test_utils fallbacks are no longer needed.
//
// When Viz Hit Testing is available, waits until hit test data for
// |child_frame| has been submitted, see WaitForHitTestData. Otherwise waits
// until the cc::Surface associated with |child_frame| has been activated.
void WaitForHitTestDataOrChildSurfaceReady(RenderFrameHost* child_frame);
void WaitForHitTestDataOrGuestSurfaceReady(WebContents* guest_web_contents);

// TODO(jonross): Move this to components/viz/host/hit_test/ as a standalone
// HitTestDataWaiter (is-a HitTestRegionObserver) once Viz HitTesting is on by
// default, and there are no longer dependancies upon content.
//
// Test API which observes the arrival of hit test data within a Viz host.
//
// HitTestRegionObserver is bound to a viz::FrameSinkId for which it observers
// changes in hit test data.
class HitTestRegionObserver : public viz::HitTestRegionObserver {
 public:
  explicit HitTestRegionObserver(const viz::FrameSinkId& frame_sink_id);
  ~HitTestRegionObserver() override;

  // Waits until the hit testing data for |frame_sink_id_| has arrvied. However
  // if there is existing hit test data for |frame_sink_id_| this will not wait
  // for new data to be submitted.
  //
  // TODO(jonross): Update this so that it can also be used to wait for updated
  // data to arrive.
  void WaitForHitTestData();

  const std::vector<viz::AggregatedHitTestRegion>& GetHitTestData();

 private:
  // viz::HitTestRegionObserver:
  void OnAggregatedHitTestRegionListUpdated(
      const viz::FrameSinkId& frame_sink_id,
      const std::vector<viz::AggregatedHitTestRegion>& hit_test_data) override;

  viz::FrameSinkId const frame_sink_id_;
  std::unique_ptr<base::RunLoop> run_loop_;

  DISALLOW_COPY_AND_ASSIGN(HitTestRegionObserver);
};

// Test API which can wait until there is a change in hit test data for a
// particular FrameSinkId. The change can be one of the following:
//   1. Change in root to target transform.
//   1. Target that wasn't hit testable became hit testable.
//   2. Target that was hit testable became not hit testable.
class HitTestTransformChangeObserver : public viz::HitTestRegionObserver {
 public:
  explicit HitTestTransformChangeObserver(
      const viz::FrameSinkId& frame_sink_id);
  ~HitTestTransformChangeObserver() override;

  // Waits until hit testing data for |frame_sink_id_| changes. If hit test data
  // for |frame_sink_id_| has already changed since it was cached then this will
  // return immediately. The cached transform will be updated when this returns
  // so it can be called later.
  void WaitForHitTestDataChange();

 private:
  // viz::HitTestRegionObserver:
  void OnAggregatedHitTestRegionListUpdated(
      const viz::FrameSinkId& frame_sink_id,
      const std::vector<viz::AggregatedHitTestRegion>& hit_test_data) override;

  const viz::FrameSinkId target_frame_sink_id_;
  base::Optional<gfx::Transform> cached_transform_;
  std::unique_ptr<base::RunLoop> run_loop_;

  DISALLOW_COPY_AND_ASSIGN(HitTestTransformChangeObserver);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_HIT_TEST_REGION_OBSERVER_H_
