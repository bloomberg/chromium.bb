// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/bind.h"
#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/ozone/platform/drm/gpu/crtc_controller.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_controller.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_plane.h"
#include "ui/ozone/platform/drm/gpu/mock_drm_device.h"
#include "ui/ozone/platform/drm/gpu/mock_scanout_buffer.h"

namespace {

// Create a basic mode for a 6x4 screen.
const drmModeModeInfo kDefaultMode =
    {0, 6, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, {'\0'}};

constexpr uint32_t kCrtcIdBase = 1;
constexpr uint32_t kPrimaryCrtc = kCrtcIdBase;
constexpr uint32_t kSecondaryCrtc = kCrtcIdBase + 1;
constexpr uint32_t kPrimaryConnector = 10;
constexpr uint32_t kSecondaryConnector = 11;

const gfx::Size kDefaultModeSize(kDefaultMode.hdisplay, kDefaultMode.vdisplay);
const gfx::Size kOverlaySize(kDefaultMode.hdisplay / 2,
                             kDefaultMode.vdisplay / 2);
const gfx::SizeF kDefaultModeSizeF(1.0, 1.0);

}  // namespace

class HardwareDisplayControllerTest : public testing::Test {
 public:
  HardwareDisplayControllerTest() : page_flips_(0) {}
  ~HardwareDisplayControllerTest() override {}

  void SetUp() override;
  void TearDown() override;

  void InitializeDrmDevice(bool use_atomic);
  void SchedulePageFlip(ui::DrmOverlayPlaneList planes);
  void OnSubmission(gfx::SwapResult swap_result,
                    std::unique_ptr<gfx::GpuFence> out_fence);
  void OnPresentation(const gfx::PresentationFeedback& feedback);

 protected:
  std::unique_ptr<ui::HardwareDisplayController> controller_;
  scoped_refptr<ui::MockDrmDevice> drm_;

  int page_flips_;
  gfx::SwapResult last_swap_result_;
  gfx::PresentationFeedback last_presentation_feedback_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HardwareDisplayControllerTest);
};

void HardwareDisplayControllerTest::SetUp() {
  page_flips_ = 0;
  last_swap_result_ = gfx::SwapResult::SWAP_FAILED;

  drm_ = new ui::MockDrmDevice;
  InitializeDrmDevice(/* use_atomic= */ false);

  controller_.reset(new ui::HardwareDisplayController(
      std::unique_ptr<ui::CrtcController>(
          new ui::CrtcController(drm_.get(), kPrimaryCrtc, kPrimaryConnector)),
      gfx::Point()));
}

void HardwareDisplayControllerTest::TearDown() {
  controller_.reset();
  drm_ = nullptr;
}

void HardwareDisplayControllerTest::InitializeDrmDevice(bool use_atomic) {
  constexpr uint32_t kTypePropId = 300;
  constexpr uint32_t kInFormatsPropId = 301;
  constexpr uint32_t kInFormatsBlobPropId = 400;

  std::vector<ui::MockDrmDevice::CrtcProperties> crtc_properties(2);
  std::vector<ui::MockDrmDevice::PlaneProperties> plane_properties;
  std::map<uint32_t, std::string> property_names = {
      // Add all required properties.
      {200, "CRTC_ID"},
      {201, "CRTC_X"},
      {202, "CRTC_Y"},
      {203, "CRTC_W"},
      {204, "CRTC_H"},
      {205, "FB_ID"},
      {206, "SRC_X"},
      {207, "SRC_Y"},
      {208, "SRC_W"},
      {209, "SRC_H"},
      // Add some optional properties we use for convenience.
      {kTypePropId, "type"},
      {kInFormatsPropId, "IN_FORMATS"},
  };

  for (size_t i = 0; i < crtc_properties.size(); ++i) {
    crtc_properties[i].id = kCrtcIdBase + i;

    for (size_t j = 0; j < 2; ++j) {
      const uint32_t offset = plane_properties.size();

      ui::MockDrmDevice::PlaneProperties plane;
      plane.id = 100 + offset;
      plane.crtc_mask = 1 << i;
      for (const auto& pair : property_names) {
        uint32_t value = 0;
        if (pair.first == kTypePropId)
          value = j == 0 ? DRM_PLANE_TYPE_PRIMARY : DRM_PLANE_TYPE_OVERLAY;
        else if (pair.first == kInFormatsPropId)
          value = kInFormatsBlobPropId;

        plane.properties.push_back({.id = pair.first, .value = value});
      };

      drm_->SetPropertyBlob(ui::MockDrmDevice::AllocateInFormatsBlob(
          kInFormatsBlobPropId, {DRM_FORMAT_XRGB8888}, {}));

      plane_properties.emplace_back(std::move(plane));
    }
  }

  drm_->InitializeState(crtc_properties, plane_properties, property_names,
                        use_atomic);
}

void HardwareDisplayControllerTest::SchedulePageFlip(
    ui::DrmOverlayPlaneList planes) {
  controller_->SchedulePageFlip(
      std::move(planes),
      base::BindOnce(&HardwareDisplayControllerTest::OnSubmission,
                     base::Unretained(this)),
      base::BindOnce(&HardwareDisplayControllerTest::OnPresentation,
                     base::Unretained(this)));
}

void HardwareDisplayControllerTest::OnSubmission(
    gfx::SwapResult result,
    std::unique_ptr<gfx::GpuFence> out_fence) {
  last_swap_result_ = result;
}

void HardwareDisplayControllerTest::OnPresentation(
    const gfx::PresentationFeedback& feedback) {
  page_flips_++;
  last_presentation_feedback_ = feedback;
}

TEST_F(HardwareDisplayControllerTest, CheckModesettingResult) {
  ui::DrmOverlayPlane plane(scoped_refptr<ui::ScanoutBuffer>(
                                new ui::MockScanoutBuffer(kDefaultModeSize)),
                            nullptr);

  EXPECT_TRUE(controller_->Modeset(plane, kDefaultMode));
  EXPECT_FALSE(plane.buffer->HasOneRef());
}

TEST_F(HardwareDisplayControllerTest, CheckStateAfterPageFlip) {
  ui::DrmOverlayPlane plane1(scoped_refptr<ui::ScanoutBuffer>(
                                 new ui::MockScanoutBuffer(kDefaultModeSize)),
                             nullptr);

  EXPECT_TRUE(controller_->Modeset(plane1, kDefaultMode));

  ui::DrmOverlayPlane plane2(scoped_refptr<ui::ScanoutBuffer>(
                                 new ui::MockScanoutBuffer(kDefaultModeSize)),
                             nullptr);
  std::vector<ui::DrmOverlayPlane> planes;
  planes.push_back(plane2.Clone());

  SchedulePageFlip(std::move(planes));

  drm_->RunCallbacks();
  EXPECT_TRUE(plane1.buffer->HasOneRef());
  EXPECT_FALSE(plane2.buffer->HasOneRef());

  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, last_swap_result_);
  EXPECT_EQ(1, page_flips_);
  EXPECT_EQ(1, drm_->get_page_flip_call_count());
  EXPECT_EQ(0, drm_->get_overlay_flip_call_count());
}

TEST_F(HardwareDisplayControllerTest, CheckStateIfModesetFails) {
  drm_->set_set_crtc_expectation(false);

  ui::DrmOverlayPlane plane(scoped_refptr<ui::ScanoutBuffer>(
                                new ui::MockScanoutBuffer(kDefaultModeSize)),
                            nullptr);

  EXPECT_FALSE(controller_->Modeset(plane, kDefaultMode));
}

TEST_F(HardwareDisplayControllerTest, CheckStateIfPageFlipFails) {
  drm_->set_page_flip_expectation(false);

  ui::DrmOverlayPlane plane1(scoped_refptr<ui::ScanoutBuffer>(
                                 new ui::MockScanoutBuffer(kDefaultModeSize)),
                             nullptr);

  EXPECT_TRUE(controller_->Modeset(plane1, kDefaultMode));

  ui::DrmOverlayPlane plane2(scoped_refptr<ui::ScanoutBuffer>(
                                 new ui::MockScanoutBuffer(kDefaultModeSize)),
                             nullptr);
  std::vector<ui::DrmOverlayPlane> planes;
  planes.push_back(plane2.Clone());
  EXPECT_DEATH_IF_SUPPORTED(SchedulePageFlip(std::move(planes)),
                            "SchedulePageFlip failed");
}

TEST_F(HardwareDisplayControllerTest, CheckOverlayPresent) {
  ui::DrmOverlayPlane plane1(scoped_refptr<ui::ScanoutBuffer>(
                                 new ui::MockScanoutBuffer(kDefaultModeSize)),
                             nullptr);
  ui::DrmOverlayPlane plane2(
      scoped_refptr<ui::ScanoutBuffer>(new ui::MockScanoutBuffer(kOverlaySize)),
      1, gfx::OVERLAY_TRANSFORM_NONE, gfx::Rect(kOverlaySize),
      gfx::RectF(kDefaultModeSizeF), true, nullptr);

  EXPECT_TRUE(controller_->Modeset(plane1, kDefaultMode));

  std::vector<ui::DrmOverlayPlane> planes;
  planes.push_back(plane1.Clone());
  planes.push_back(plane2.Clone());

  SchedulePageFlip(std::move(planes));
  drm_->RunCallbacks();
  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, last_swap_result_);
  EXPECT_EQ(1, page_flips_);
  EXPECT_EQ(1, drm_->get_page_flip_call_count());
  EXPECT_EQ(1, drm_->get_overlay_flip_call_count());
}

TEST_F(HardwareDisplayControllerTest, CheckOverlayTestMode) {
  ui::DrmOverlayPlane plane1(scoped_refptr<ui::ScanoutBuffer>(
                                 new ui::MockScanoutBuffer(kDefaultModeSize)),
                             nullptr);
  ui::DrmOverlayPlane plane2(
      scoped_refptr<ui::ScanoutBuffer>(new ui::MockScanoutBuffer(kOverlaySize)),
      1, gfx::OVERLAY_TRANSFORM_NONE, gfx::Rect(kOverlaySize),
      gfx::RectF(kDefaultModeSizeF), true, nullptr);

  EXPECT_TRUE(controller_->Modeset(plane1, kDefaultMode));

  std::vector<ui::DrmOverlayPlane> planes;
  planes.push_back(plane1.Clone());
  planes.push_back(plane2.Clone());

  SchedulePageFlip(ui::DrmOverlayPlane::Clone(planes));

  // A test call shouldn't cause new flips, but should succeed.
  EXPECT_TRUE(controller_->TestPageFlip(planes));
  drm_->RunCallbacks();
  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, last_swap_result_);
  EXPECT_EQ(1, page_flips_);
  EXPECT_EQ(1, drm_->get_page_flip_call_count());
  EXPECT_EQ(1, drm_->get_overlay_flip_call_count());

  // Regular flips should continue on normally.
  SchedulePageFlip(ui::DrmOverlayPlane::Clone(planes));
  drm_->RunCallbacks();
  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, last_swap_result_);
  EXPECT_EQ(2, page_flips_);
  EXPECT_EQ(2, drm_->get_page_flip_call_count());
  EXPECT_EQ(2, drm_->get_overlay_flip_call_count());
}

TEST_F(HardwareDisplayControllerTest, AcceptUnderlays) {
  ui::DrmOverlayPlane plane1(scoped_refptr<ui::ScanoutBuffer>(
                                 new ui::MockScanoutBuffer(kDefaultModeSize)),
                             nullptr);
  ui::DrmOverlayPlane plane2(scoped_refptr<ui::ScanoutBuffer>(
                                 new ui::MockScanoutBuffer(kDefaultModeSize)),
                             -1, gfx::OVERLAY_TRANSFORM_NONE,
                             gfx::Rect(kDefaultModeSize),
                             gfx::RectF(kDefaultModeSizeF), true, nullptr);

  EXPECT_TRUE(controller_->Modeset(plane1, kDefaultMode));

  std::vector<ui::DrmOverlayPlane> planes;
  planes.push_back(plane1.Clone());
  planes.push_back(plane2.Clone());

  SchedulePageFlip(std::move(planes));
  drm_->RunCallbacks();
  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, last_swap_result_);
  EXPECT_EQ(1, page_flips_);
}

TEST_F(HardwareDisplayControllerTest, PageflipMirroredControllers) {
  controller_->AddCrtc(std::unique_ptr<ui::CrtcController>(
      new ui::CrtcController(drm_.get(), kSecondaryCrtc, kSecondaryConnector)));

  ui::DrmOverlayPlane plane1(scoped_refptr<ui::ScanoutBuffer>(
                                 new ui::MockScanoutBuffer(kDefaultModeSize)),
                             nullptr);
  EXPECT_TRUE(controller_->Modeset(plane1, kDefaultMode));
  EXPECT_EQ(2, drm_->get_set_crtc_call_count());

  ui::DrmOverlayPlane plane2(scoped_refptr<ui::ScanoutBuffer>(
                                 new ui::MockScanoutBuffer(kDefaultModeSize)),
                             nullptr);
  std::vector<ui::DrmOverlayPlane> planes;
  planes.push_back(plane2.Clone());
  SchedulePageFlip(std::move(planes));
  drm_->RunCallbacks();
  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, last_swap_result_);
  EXPECT_EQ(1, page_flips_);
  EXPECT_EQ(2, drm_->get_page_flip_call_count());
  EXPECT_EQ(1, page_flips_);
}

TEST_F(HardwareDisplayControllerTest, PlaneStateAfterRemoveCrtc) {
  controller_->AddCrtc(std::unique_ptr<ui::CrtcController>(
      new ui::CrtcController(drm_.get(), kSecondaryCrtc, kSecondaryConnector)));

  ui::DrmOverlayPlane plane1(scoped_refptr<ui::ScanoutBuffer>(
                                 new ui::MockScanoutBuffer(kDefaultModeSize)),
                             nullptr);
  EXPECT_TRUE(controller_->Modeset(plane1, kDefaultMode));
  std::vector<ui::DrmOverlayPlane> planes;
  planes.push_back(plane1.Clone());
  SchedulePageFlip(ui::DrmOverlayPlane::Clone(planes));
  drm_->RunCallbacks();
  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, last_swap_result_);
  EXPECT_EQ(1, page_flips_);

  const ui::HardwareDisplayPlane* primary_crtc_plane = nullptr;
  const ui::HardwareDisplayPlane* secondary_crtc_plane = nullptr;
  for (const auto& plane : drm_->plane_manager()->planes()) {
    if (plane->in_use() && plane->owning_crtc() == kPrimaryCrtc)
      primary_crtc_plane = plane.get();
    if (plane->in_use() && plane->owning_crtc() == kSecondaryCrtc)
      secondary_crtc_plane = plane.get();
  }

  ASSERT_NE(nullptr, primary_crtc_plane);
  ASSERT_NE(nullptr, secondary_crtc_plane);
  EXPECT_EQ(kPrimaryCrtc, primary_crtc_plane->owning_crtc());
  EXPECT_EQ(kSecondaryCrtc, secondary_crtc_plane->owning_crtc());

  // Removing the crtc should not free the plane or change ownership.
  std::unique_ptr<ui::CrtcController> crtc =
      controller_->RemoveCrtc(drm_, kPrimaryCrtc);
  EXPECT_TRUE(primary_crtc_plane->in_use());
  EXPECT_EQ(kPrimaryCrtc, primary_crtc_plane->owning_crtc());
  EXPECT_TRUE(secondary_crtc_plane->in_use());
  EXPECT_EQ(kSecondaryCrtc, secondary_crtc_plane->owning_crtc());

  // Check that controller doesn't affect the state of removed plane in
  // subsequent page flip.
  SchedulePageFlip(ui::DrmOverlayPlane::Clone(planes));
  drm_->RunCallbacks();
  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, last_swap_result_);
  EXPECT_EQ(2, page_flips_);
  EXPECT_TRUE(primary_crtc_plane->in_use());
  EXPECT_EQ(kPrimaryCrtc, primary_crtc_plane->owning_crtc());
  EXPECT_TRUE(secondary_crtc_plane->in_use());
  EXPECT_EQ(kSecondaryCrtc, secondary_crtc_plane->owning_crtc());
}

TEST_F(HardwareDisplayControllerTest, PlaneStateAfterDestroyingCrtc) {
  ui::DrmOverlayPlane plane1(scoped_refptr<ui::ScanoutBuffer>(
                                 new ui::MockScanoutBuffer(kDefaultModeSize)),
                             nullptr);
  EXPECT_TRUE(controller_->Modeset(plane1, kDefaultMode));
  std::vector<ui::DrmOverlayPlane> planes;
  planes.push_back(plane1.Clone());
  SchedulePageFlip(std::move(planes));
  drm_->RunCallbacks();
  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, last_swap_result_);
  EXPECT_EQ(1, page_flips_);

  const ui::HardwareDisplayPlane* owned_plane = nullptr;
  for (const auto& plane : drm_->plane_manager()->planes())
    if (plane->in_use())
      owned_plane = plane.get();
  ASSERT_TRUE(owned_plane != nullptr);
  EXPECT_EQ(kPrimaryCrtc, owned_plane->owning_crtc());
  std::unique_ptr<ui::CrtcController> crtc =
      controller_->RemoveCrtc(drm_, kPrimaryCrtc);
  // Destroying crtc should free the plane.
  crtc.reset();
  uint32_t crtc_nullid = 0;
  EXPECT_FALSE(owned_plane->in_use());
  EXPECT_EQ(crtc_nullid, owned_plane->owning_crtc());
}

TEST_F(HardwareDisplayControllerTest, PlaneStateAfterAddCrtc) {
  controller_->AddCrtc(std::unique_ptr<ui::CrtcController>(
      new ui::CrtcController(drm_.get(), kSecondaryCrtc, kSecondaryConnector)));

  ui::DrmOverlayPlane plane1(scoped_refptr<ui::ScanoutBuffer>(
                                 new ui::MockScanoutBuffer(kDefaultModeSize)),
                             nullptr);
  EXPECT_TRUE(controller_->Modeset(plane1, kDefaultMode));
  std::vector<ui::DrmOverlayPlane> planes;
  planes.push_back(plane1.Clone());
  SchedulePageFlip(ui::DrmOverlayPlane::Clone(planes));
  drm_->RunCallbacks();
  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, last_swap_result_);
  EXPECT_EQ(1, page_flips_);

  ui::HardwareDisplayPlane* primary_crtc_plane = nullptr;
  for (const auto& plane : drm_->plane_manager()->planes()) {
    if (plane->in_use() && kPrimaryCrtc == plane->owning_crtc())
      primary_crtc_plane = plane.get();
  }

  ASSERT_TRUE(primary_crtc_plane != nullptr);

  std::unique_ptr<ui::HardwareDisplayController> hdc_controller;
  hdc_controller.reset(new ui::HardwareDisplayController(
      controller_->RemoveCrtc(drm_, kPrimaryCrtc), controller_->origin()));
  SchedulePageFlip(ui::DrmOverlayPlane::Clone(planes));
  drm_->RunCallbacks();
  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, last_swap_result_);
  EXPECT_EQ(2, page_flips_);
  EXPECT_TRUE(primary_crtc_plane->in_use());
  EXPECT_EQ(kPrimaryCrtc, primary_crtc_plane->owning_crtc());

  // We reset state of plane here to test that the plane was actually added to
  // hdc_controller. In which case, the right state should be set to plane
  // after page flip call is handled by the controller.
  primary_crtc_plane->set_in_use(false);
  primary_crtc_plane->set_owning_crtc(0);
  hdc_controller->SchedulePageFlip(
      ui::DrmOverlayPlane::Clone(planes),
      base::BindOnce(&HardwareDisplayControllerTest::OnSubmission,
                     base::Unretained(this)),
      base::BindOnce(&HardwareDisplayControllerTest::OnPresentation,
                     base::Unretained(this)));
  drm_->RunCallbacks();
  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, last_swap_result_);
  EXPECT_EQ(3, page_flips_);
  EXPECT_TRUE(primary_crtc_plane->in_use());
  EXPECT_EQ(kPrimaryCrtc, primary_crtc_plane->owning_crtc());
}

TEST_F(HardwareDisplayControllerTest, ModesetWhilePageFlipping) {
  ui::DrmOverlayPlane plane1(scoped_refptr<ui::ScanoutBuffer>(
                                 new ui::MockScanoutBuffer(kDefaultModeSize)),
                             nullptr);
  EXPECT_TRUE(controller_->Modeset(plane1, kDefaultMode));
  std::vector<ui::DrmOverlayPlane> planes;
  planes.push_back(plane1.Clone());
  SchedulePageFlip(std::move(planes));

  EXPECT_TRUE(controller_->Modeset(plane1, kDefaultMode));
  drm_->RunCallbacks();
  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, last_swap_result_);
  EXPECT_EQ(1, page_flips_);
}

TEST_F(HardwareDisplayControllerTest, FailPageFlipping) {
  drm_->set_page_flip_expectation(false);

  ui::DrmOverlayPlane plane1(scoped_refptr<ui::ScanoutBuffer>(
                                 new ui::MockScanoutBuffer(kDefaultModeSize)),
                             nullptr);
  EXPECT_TRUE(controller_->Modeset(plane1, kDefaultMode));
  std::vector<ui::DrmOverlayPlane> planes;
  planes.push_back(plane1.Clone());
  EXPECT_DEATH_IF_SUPPORTED(SchedulePageFlip(std::move(planes)),
                            "SchedulePageFlip failed");
}

TEST_F(HardwareDisplayControllerTest, CheckNoPrimaryPlane) {
  ui::DrmOverlayPlane plane1(scoped_refptr<ui::ScanoutBuffer>(
                                 new ui::MockScanoutBuffer(kDefaultModeSize)),
                             1, gfx::OVERLAY_TRANSFORM_NONE,
                             gfx::Rect(kDefaultModeSize),
                             gfx::RectF(0, 0, 1, 1), true, nullptr);
  EXPECT_TRUE(controller_->Modeset(plane1, kDefaultMode));
  std::vector<ui::DrmOverlayPlane> planes;
  planes.push_back(plane1.Clone());
  SchedulePageFlip(std::move(planes));

  drm_->RunCallbacks();
  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, last_swap_result_);
  EXPECT_EQ(1, page_flips_);
}

TEST_F(HardwareDisplayControllerTest, AddCrtcMidPageFlip) {
  ui::DrmOverlayPlane plane1(scoped_refptr<ui::ScanoutBuffer>(
                                 new ui::MockScanoutBuffer(kDefaultModeSize)),
                             nullptr);
  EXPECT_TRUE(controller_->Modeset(plane1, kDefaultMode));
  std::vector<ui::DrmOverlayPlane> planes;
  planes.push_back(plane1.Clone());
  SchedulePageFlip(std::move(planes));

  controller_->AddCrtc(std::unique_ptr<ui::CrtcController>(
      new ui::CrtcController(drm_.get(), kSecondaryCrtc, kSecondaryConnector)));

  drm_->RunCallbacks();
  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, last_swap_result_);
  EXPECT_EQ(1, page_flips_);
}

TEST_F(HardwareDisplayControllerTest, RemoveCrtcMidPageFlip) {
  ui::DrmOverlayPlane plane1(scoped_refptr<ui::ScanoutBuffer>(
                                 new ui::MockScanoutBuffer(kDefaultModeSize)),
                             nullptr);
  EXPECT_TRUE(controller_->Modeset(plane1, kDefaultMode));
  std::vector<ui::DrmOverlayPlane> planes;
  planes.push_back(plane1.Clone());
  SchedulePageFlip(std::move(planes));

  controller_->RemoveCrtc(drm_, kPrimaryCrtc);

  drm_->RunCallbacks();
  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, last_swap_result_);
  EXPECT_EQ(1, page_flips_);
}

TEST_F(HardwareDisplayControllerTest, Disable) {
  // Page flipping overlays is only supported on atomic configurations.
  InitializeDrmDevice(/* use_atomic= */ true);

  ui::DrmOverlayPlane plane1(scoped_refptr<ui::ScanoutBuffer>(
                                 new ui::MockScanoutBuffer(kDefaultModeSize)),
                             nullptr);
  EXPECT_TRUE(controller_->Modeset(plane1, kDefaultMode));

  ui::DrmOverlayPlane plane2(
      new ui::MockScanoutBuffer(kOverlaySize), 1, gfx::OVERLAY_TRANSFORM_NONE,
      gfx::Rect(kOverlaySize), gfx::RectF(kDefaultModeSizeF), true, nullptr);
  std::vector<ui::DrmOverlayPlane> planes;
  planes.push_back(plane1.Clone());
  planes.push_back(plane2.Clone());

  SchedulePageFlip(std::move(planes));
  drm_->RunCallbacks();
  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, last_swap_result_);

  controller_->Disable();

  int planes_in_use = 0;
  for (const auto& plane : drm_->plane_manager()->planes()) {
    if (plane->in_use())
      planes_in_use++;
  }
  // Only the primary plane is in use.
  ASSERT_EQ(1, planes_in_use);
}
