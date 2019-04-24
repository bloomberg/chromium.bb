// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/common/drm_overlay_manager.h"

#include "base/bind.h"
#include "base/callback.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/ozone/public/overlay_surface_candidate.h"

namespace ui {
namespace {

constexpr gfx::AcceleratedWidget kWidget = 1;

class TestDrmOverlayManager : public DrmOverlayManager {
 public:
  TestDrmOverlayManager() = default;
  ~TestDrmOverlayManager() override = default;

  using DrmOverlayManager::UpdateCacheForOverlayCandidates;

  std::vector<std::vector<OverlaySurfaceCandidate>>& requests() {
    return requests_;
  }

  // DrmOverlayManager:
  void SendOverlayValidationRequest(
      const std::vector<OverlaySurfaceCandidate>& candidates,
      gfx::AcceleratedWidget widget) override {
    requests_.push_back(candidates);
  }

 private:
  std::vector<std::vector<OverlaySurfaceCandidate>> requests_;
};

OverlaySurfaceCandidate CreateCandidate(const gfx::Rect& rect,
                                        int plane_z_order) {
  ui::OverlaySurfaceCandidate candidate;
  candidate.transform = gfx::OVERLAY_TRANSFORM_NONE;
  candidate.format = gfx::BufferFormat::YUV_420_BIPLANAR;
  candidate.plane_z_order = plane_z_order;
  candidate.buffer_size = rect.size();
  candidate.display_rect = gfx::RectF(rect);
  candidate.crop_rect = gfx::RectF(rect);
  return candidate;
}

}  // namespace

TEST(DrmOverlayManagerTest, CacheLogic) {
  TestDrmOverlayManager manager;

  // Candidates for output surface and single-on-top quad.
  std::vector<OverlaySurfaceCandidate> candidates = {
      CreateCandidate(gfx::Rect(0, 0, 100, 100), 0),
      CreateCandidate(gfx::Rect(10, 10, 20, 20), 1)};

  // The first three times that CheckOverlaySupport() is called for an overlay
  // configuration it won't send a validation request.
  for (int i = 0; i < 3; ++i) {
    manager.CheckOverlaySupport(&candidates, kWidget);
    EXPECT_FALSE(candidates[0].overlay_handled);
    EXPECT_FALSE(candidates[1].overlay_handled);
    EXPECT_EQ(manager.requests().size(), 0u);
  }

  // The fourth call with the same overlay configuration should trigger a
  // request to validate the configuration. Still assume the overlay
  // configuration won't work until we get a response.
  manager.CheckOverlaySupport(&candidates, kWidget);
  EXPECT_FALSE(candidates[0].overlay_handled);
  EXPECT_FALSE(candidates[1].overlay_handled);
  EXPECT_EQ(manager.requests().size(), 1u);

  // While waiting for a response we shouldn't send the same request.
  manager.CheckOverlaySupport(&candidates, kWidget);
  EXPECT_FALSE(candidates[0].overlay_handled);
  EXPECT_FALSE(candidates[1].overlay_handled);
  ASSERT_EQ(manager.requests().size(), 1u);

  // Receive response that the overlay configuration will work.
  manager.UpdateCacheForOverlayCandidates(
      manager.requests().front(),
      std::vector<OverlayStatus>(candidates.size(), OVERLAY_STATUS_ABLE));
  manager.requests().clear();

  // CheckOverlaySupport() should now indicate the overlay configuration will
  // work.
  manager.CheckOverlaySupport(&candidates, kWidget);
  EXPECT_TRUE(candidates[0].overlay_handled);
  EXPECT_TRUE(candidates[1].overlay_handled);
  EXPECT_EQ(manager.requests().size(), 0u);
}

}  // namespace ui
