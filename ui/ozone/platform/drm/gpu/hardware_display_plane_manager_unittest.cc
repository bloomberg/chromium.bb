// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/ozone/platform/drm/gpu/crtc_controller.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_controller.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_plane.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_plane_manager.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_plane_manager_legacy.h"
#include "ui/ozone/platform/drm/gpu/overlay_plane.h"
#include "ui/ozone/platform/drm/gpu/scanout_buffer.h"
#include "ui/ozone/platform/drm/test/mock_drm_device.h"

namespace {

struct FakePlaneInfo {
  uint32_t id;
  uint32_t allowed_crtc_mask;
};

const FakePlaneInfo kOnePlanePerCrtc[] = {{10, 1}, {20, 2}};
const FakePlaneInfo kTwoPlanesPerCrtc[] = {{10, 1}, {11, 1}, {20, 2}, {21, 2}};
const FakePlaneInfo kOnePlanePerCrtcWithShared[] = {{10, 1}, {20, 2}, {50, 3}};

class FakeScanoutBuffer : public ui::ScanoutBuffer {
 public:
  FakeScanoutBuffer() {}

  // ui::ScanoutBuffer:
  uint32_t GetFramebufferId() const override { return 1; }
  uint32_t GetHandle() const override { return 0; }
  gfx::Size GetSize() const override { return gfx::Size(1, 1); }

 protected:
  ~FakeScanoutBuffer() override {}
};

class FakePlaneManager : public ui::HardwareDisplayPlaneManager {
 public:
  FakePlaneManager() : plane_count_(0) {}
  ~FakePlaneManager() override {}

  // Normally we'd use DRM to figure out the controller configuration. But we
  // can't use DRM in unit tests, so we just create a fake configuration.
  void InitForTest(const FakePlaneInfo* planes,
                   size_t count,
                   const std::vector<uint32_t>& crtcs) {
    crtcs_ = crtcs;
    for (size_t i = 0; i < count; i++) {
      planes_.push_back(new ui::HardwareDisplayPlane(
          planes[i].id, planes[i].allowed_crtc_mask));
    }
    // The real HDPM uses sorted planes, so sort them for consistency.
    std::sort(planes_.begin(), planes_.end(),
              [](ui::HardwareDisplayPlane* l, ui::HardwareDisplayPlane* r) {
                return l->plane_id() < r->plane_id();
              });
  }

  bool Commit(ui::HardwareDisplayPlaneList* plane_list, bool is_sync) override {
    return false;
  }

  bool SetPlaneData(ui::HardwareDisplayPlaneList* plane_list,
                    ui::HardwareDisplayPlane* hw_plane,
                    const ui::OverlayPlane& overlay,
                    uint32_t crtc_id,
                    const gfx::Rect& src_rect,
                    ui::CrtcController* crtc) override {
    // Check that the chosen plane is a legal choice for the crtc.
    EXPECT_NE(-1, LookupCrtcIndex(crtc_id));
    EXPECT_TRUE(hw_plane->CanUseForCrtc(LookupCrtcIndex(crtc_id)));
    EXPECT_FALSE(hw_plane->in_use());
    plane_count_++;
    return true;
  }

  int plane_count() const { return plane_count_; }

 private:
  DISALLOW_COPY_AND_ASSIGN(FakePlaneManager);

  int plane_count_;
};

class HardwareDisplayPlaneManagerTest : public testing::Test {
 public:
  HardwareDisplayPlaneManagerTest() {}

  void SetUp() override;

 protected:
  scoped_ptr<FakePlaneManager> plane_manager_;
  ui::HardwareDisplayPlaneList state_;
  std::vector<uint32_t> default_crtcs_;
  scoped_refptr<ui::ScanoutBuffer> fake_buffer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HardwareDisplayPlaneManagerTest);
};

void HardwareDisplayPlaneManagerTest::SetUp() {
  fake_buffer_ = new FakeScanoutBuffer();
  plane_manager_.reset(new FakePlaneManager());
  default_crtcs_.push_back(100);
  default_crtcs_.push_back(200);
}

TEST_F(HardwareDisplayPlaneManagerTest, SinglePlaneAssignment) {
  ui::OverlayPlaneList assigns;
  assigns.push_back(ui::OverlayPlane(fake_buffer_));
  plane_manager_->InitForTest(kOnePlanePerCrtc, arraysize(kOnePlanePerCrtc),
                              default_crtcs_);
  EXPECT_TRUE(plane_manager_->AssignOverlayPlanes(&state_, assigns,
                                                  default_crtcs_[0], nullptr));
  EXPECT_EQ(1, plane_manager_->plane_count());
}

TEST_F(HardwareDisplayPlaneManagerTest, BadCrtc) {
  ui::OverlayPlaneList assigns;
  assigns.push_back(ui::OverlayPlane(fake_buffer_));
  plane_manager_->InitForTest(kOnePlanePerCrtc, arraysize(kOnePlanePerCrtc),
                              default_crtcs_);
  EXPECT_FALSE(
      plane_manager_->AssignOverlayPlanes(&state_, assigns, 1, nullptr));
}

TEST_F(HardwareDisplayPlaneManagerTest, MultiplePlaneAssignment) {
  ui::OverlayPlaneList assigns;
  assigns.push_back(ui::OverlayPlane(fake_buffer_));
  assigns.push_back(ui::OverlayPlane(fake_buffer_));
  plane_manager_->InitForTest(kTwoPlanesPerCrtc, arraysize(kTwoPlanesPerCrtc),
                              default_crtcs_);
  EXPECT_TRUE(plane_manager_->AssignOverlayPlanes(&state_, assigns,
                                                  default_crtcs_[0], nullptr));
  EXPECT_EQ(2, plane_manager_->plane_count());
}

TEST_F(HardwareDisplayPlaneManagerTest, NotEnoughPlanes) {
  ui::OverlayPlaneList assigns;
  assigns.push_back(ui::OverlayPlane(fake_buffer_));
  assigns.push_back(ui::OverlayPlane(fake_buffer_));
  plane_manager_->InitForTest(kOnePlanePerCrtc, arraysize(kOnePlanePerCrtc),
                              default_crtcs_);

  EXPECT_FALSE(plane_manager_->AssignOverlayPlanes(&state_, assigns,
                                                   default_crtcs_[0], nullptr));
}

TEST_F(HardwareDisplayPlaneManagerTest, MultipleCrtcs) {
  ui::OverlayPlaneList assigns;
  assigns.push_back(ui::OverlayPlane(fake_buffer_));
  plane_manager_->InitForTest(kOnePlanePerCrtc, arraysize(kOnePlanePerCrtc),
                              default_crtcs_);

  EXPECT_TRUE(plane_manager_->AssignOverlayPlanes(&state_, assigns,
                                                  default_crtcs_[0], nullptr));
  EXPECT_TRUE(plane_manager_->AssignOverlayPlanes(&state_, assigns,
                                                  default_crtcs_[1], nullptr));
  EXPECT_EQ(2, plane_manager_->plane_count());
}

TEST_F(HardwareDisplayPlaneManagerTest, MultiplePlanesAndCrtcs) {
  ui::OverlayPlaneList assigns;
  assigns.push_back(ui::OverlayPlane(fake_buffer_));
  assigns.push_back(ui::OverlayPlane(fake_buffer_));
  plane_manager_->InitForTest(kTwoPlanesPerCrtc, arraysize(kTwoPlanesPerCrtc),
                              default_crtcs_);
  EXPECT_TRUE(plane_manager_->AssignOverlayPlanes(&state_, assigns,
                                                  default_crtcs_[0], nullptr));
  EXPECT_TRUE(plane_manager_->AssignOverlayPlanes(&state_, assigns,
                                                  default_crtcs_[1], nullptr));
  EXPECT_EQ(4, plane_manager_->plane_count());
}

TEST_F(HardwareDisplayPlaneManagerTest, MultipleFrames) {
  ui::OverlayPlaneList assigns;
  assigns.push_back(ui::OverlayPlane(fake_buffer_));
  plane_manager_->InitForTest(kTwoPlanesPerCrtc, arraysize(kTwoPlanesPerCrtc),
                              default_crtcs_);

  EXPECT_TRUE(plane_manager_->AssignOverlayPlanes(&state_, assigns,
                                                  default_crtcs_[0], nullptr));
  EXPECT_EQ(1, plane_manager_->plane_count());
  // Pretend we committed the frame.
  state_.committed = true;
  state_.plane_list.swap(state_.old_plane_list);
  ui::HardwareDisplayPlane* old_plane = state_.old_plane_list[0];
  // The same plane should be used.
  EXPECT_TRUE(plane_manager_->AssignOverlayPlanes(&state_, assigns,
                                                  default_crtcs_[0], nullptr));
  EXPECT_EQ(2, plane_manager_->plane_count());
  EXPECT_EQ(state_.plane_list[0], old_plane);
}

TEST_F(HardwareDisplayPlaneManagerTest, MultipleFramesDifferentPlanes) {
  ui::OverlayPlaneList assigns;
  assigns.push_back(ui::OverlayPlane(fake_buffer_));
  plane_manager_->InitForTest(kTwoPlanesPerCrtc, arraysize(kTwoPlanesPerCrtc),
                              default_crtcs_);

  EXPECT_TRUE(plane_manager_->AssignOverlayPlanes(&state_, assigns,
                                                  default_crtcs_[0], nullptr));
  EXPECT_EQ(1, plane_manager_->plane_count());
  // The other plane should be used.
  EXPECT_TRUE(plane_manager_->AssignOverlayPlanes(&state_, assigns,
                                                  default_crtcs_[0], nullptr));
  EXPECT_EQ(2, plane_manager_->plane_count());
  EXPECT_NE(state_.plane_list[0], state_.plane_list[1]);
}

TEST_F(HardwareDisplayPlaneManagerTest, SharedPlanes) {
  ui::OverlayPlaneList assigns;
  assigns.push_back(ui::OverlayPlane(fake_buffer_));
  assigns.push_back(ui::OverlayPlane(fake_buffer_));
  plane_manager_->InitForTest(kOnePlanePerCrtcWithShared,
                              arraysize(kOnePlanePerCrtcWithShared),
                              default_crtcs_);

  EXPECT_TRUE(plane_manager_->AssignOverlayPlanes(&state_, assigns,
                                                  default_crtcs_[1], nullptr));
  EXPECT_EQ(2, plane_manager_->plane_count());
  // The shared plane is now unavailable for use by the other CRTC.
  EXPECT_FALSE(plane_manager_->AssignOverlayPlanes(&state_, assigns,
                                                   default_crtcs_[0], nullptr));
}

TEST(HardwareDisplayPlaneManagerLegacyTest, UnusedPlanesAreReleased) {
  std::vector<uint32_t> crtcs;
  crtcs.push_back(100);
  scoped_refptr<ui::MockDrmDevice> drm = new ui::MockDrmDevice(false, crtcs, 2);
  ui::OverlayPlaneList assigns;
  scoped_refptr<FakeScanoutBuffer> fake_buffer = new FakeScanoutBuffer();
  assigns.push_back(ui::OverlayPlane(fake_buffer));
  assigns.push_back(ui::OverlayPlane(fake_buffer));
  ui::HardwareDisplayPlaneList hdpl;
  ui::CrtcController crtc(drm, crtcs[0], 0);
  EXPECT_TRUE(drm->plane_manager()->AssignOverlayPlanes(&hdpl, assigns,
                                                        crtcs[0], &crtc));
  EXPECT_TRUE(drm->plane_manager()->Commit(&hdpl, false));
  assigns.clear();
  assigns.push_back(ui::OverlayPlane(fake_buffer));
  EXPECT_TRUE(drm->plane_manager()->AssignOverlayPlanes(&hdpl, assigns,
                                                        crtcs[0], &crtc));
  EXPECT_EQ(0, drm->get_overlay_clear_call_count());
  EXPECT_TRUE(drm->plane_manager()->Commit(&hdpl, false));
  EXPECT_EQ(1, drm->get_overlay_clear_call_count());
}

}  // namespace
