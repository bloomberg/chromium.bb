// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <unistd.h>

#include <memory>

#include "base/files/file_util.h"
#include "base/files/platform_file.h"
#include "base/macros.h"
#include "base/posix/eintr_wrapper.h"
#include "base/test/scoped_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/gpu_fence_handle.h"
#include "ui/ozone/platform/drm/gpu/crtc_controller.h"
#include "ui/ozone/platform/drm/gpu/fake_plane_info.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_plane_atomic.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_plane_manager_atomic.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_plane_manager_legacy.h"
#include "ui/ozone/platform/drm/gpu/mock_drm_device.h"
#include "ui/ozone/platform/drm/gpu/mock_scanout_buffer.h"

namespace {

constexpr uint32_t kTypePropId = 200;
constexpr uint32_t kInFormatsPropId = 201;
constexpr uint32_t kInFormatsBlobPropId = 300;

const uint32_t kDummyFormat = 0;
const gfx::Size kDefaultBufferSize(2, 2);

class HardwareDisplayPlaneManagerTest : public testing::Test {
 public:
  HardwareDisplayPlaneManagerTest() {}

  void InitializeDrmState(size_t crtc_count, size_t planes_per_crtc);

  void SetUp() override;

 protected:
  ui::HardwareDisplayPlaneList state_;
  scoped_refptr<ui::ScanoutBuffer> fake_buffer_;
  scoped_refptr<ui::MockDrmDevice> fake_drm_;

  std::vector<ui::MockDrmDevice::CrtcProperties> crtc_properties_;
  std::vector<ui::MockDrmDevice::PlaneProperties> plane_properties_;
  std::map<uint32_t, std::string> property_names_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HardwareDisplayPlaneManagerTest);
};

void HardwareDisplayPlaneManagerTest::SetUp() {
  fake_buffer_ = new ui::MockScanoutBuffer(kDefaultBufferSize);

  fake_drm_ = new ui::MockDrmDevice(false);
  fake_drm_->SetPropertyBlob(ui::MockDrmDevice::AllocateInFormatsBlob(
      kInFormatsBlobPropId, {DRM_FORMAT_XRGB8888}, {}));
}

void HardwareDisplayPlaneManagerTest::InitializeDrmState(
    size_t crtc_count,
    size_t planes_per_crtc) {
  property_names_.insert({kTypePropId, "type"});
  property_names_.insert({kInFormatsPropId, "IN_FORMATS"});

  for (size_t i = 0; i < crtc_count; ++i) {
    ui::MockDrmDevice::CrtcProperties crtc_prop;
    crtc_prop.id = i + 1;
    // Start ID at 1 cause 0 is an invalid ID.
    crtc_properties_.emplace_back(std::move(crtc_prop));

    for (size_t j = 0; j < planes_per_crtc; ++j) {
      ui::MockDrmDevice::PlaneProperties plane_prop;
      plane_prop.id = 100 + i * planes_per_crtc + j;
      plane_prop.crtc_mask = 1 << i;
      plane_prop.properties = {
          {.id = kTypePropId,
           .value = j == 0 ? DRM_PLANE_TYPE_PRIMARY : DRM_PLANE_TYPE_OVERLAY},
          {.id = kInFormatsPropId, .value = kInFormatsBlobPropId},
      };

      plane_properties_.emplace_back(std::move(plane_prop));
    }
  }
}

TEST_F(HardwareDisplayPlaneManagerTest, SinglePlaneAssignment) {
  ui::OverlayPlaneList assigns;
  assigns.push_back(ui::OverlayPlane(fake_buffer_, nullptr));

  InitializeDrmState(/*crtc_count=*/2, /*planes_per_crtc=*/1);
  fake_drm_->InitializeState(crtc_properties_, plane_properties_,
                             property_names_, /* use_atomic= */ false);

  EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns, crtc_properties_[0].id, nullptr));
  EXPECT_EQ(1u, state_.plane_list.size());
}

TEST_F(HardwareDisplayPlaneManagerTest, BadCrtc) {
  ui::OverlayPlaneList assigns;
  assigns.push_back(ui::OverlayPlane(fake_buffer_, nullptr));

  InitializeDrmState(/*crtc_count=*/2, /*planes_per_crtc=*/1);
  fake_drm_->InitializeState(crtc_properties_, plane_properties_,
                             property_names_, /* use_atomic= */ false);

  EXPECT_FALSE(fake_drm_->plane_manager()->AssignOverlayPlanes(&state_, assigns,
                                                               0, nullptr));
}

TEST_F(HardwareDisplayPlaneManagerTest, MultiplePlaneAssignment) {
  ui::OverlayPlaneList assigns;
  assigns.push_back(ui::OverlayPlane(fake_buffer_, nullptr));
  assigns.push_back(ui::OverlayPlane(fake_buffer_, nullptr));

  InitializeDrmState(/*crtc_count=*/2, /*planes_per_crtc=*/2);
  fake_drm_->InitializeState(crtc_properties_, plane_properties_,
                             property_names_, /* use_atomic= */ false);

  EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns, crtc_properties_[0].id, nullptr));
  EXPECT_EQ(2u, state_.plane_list.size());
}

TEST_F(HardwareDisplayPlaneManagerTest, NotEnoughPlanes) {
  ui::OverlayPlaneList assigns;
  assigns.push_back(ui::OverlayPlane(fake_buffer_, nullptr));
  assigns.push_back(ui::OverlayPlane(fake_buffer_, nullptr));

  InitializeDrmState(/*crtc_count=*/2, /*planes_per_crtc=*/1);
  fake_drm_->InitializeState(crtc_properties_, plane_properties_,
                             property_names_, /* use_atomic= */ false);

  EXPECT_FALSE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns, crtc_properties_[0].id, nullptr));
}

TEST_F(HardwareDisplayPlaneManagerTest, MultipleCrtcs) {
  ui::OverlayPlaneList assigns;
  assigns.push_back(ui::OverlayPlane(fake_buffer_, nullptr));

  InitializeDrmState(/*crtc_count=*/2, /*planes_per_crtc=*/1);
  fake_drm_->InitializeState(crtc_properties_, plane_properties_,
                             property_names_, /* use_atomic= */ false);

  EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns, crtc_properties_[0].id, nullptr));
  EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns, crtc_properties_[1].id, nullptr));
  EXPECT_EQ(2u, state_.plane_list.size());
}

TEST_F(HardwareDisplayPlaneManagerTest, MultiplePlanesAndCrtcs) {
  ui::OverlayPlaneList assigns;
  assigns.push_back(ui::OverlayPlane(fake_buffer_, nullptr));
  assigns.push_back(ui::OverlayPlane(fake_buffer_, nullptr));

  InitializeDrmState(/*crtc_count=*/2, /*planes_per_crtc=*/2);
  fake_drm_->InitializeState(crtc_properties_, plane_properties_,
                             property_names_, /* use_atomic= */ false);

  EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns, crtc_properties_[0].id, nullptr));
  EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns, crtc_properties_[1].id, nullptr));
  EXPECT_EQ(4u, state_.plane_list.size());
}

TEST_F(HardwareDisplayPlaneManagerTest, MultipleFrames) {
  ui::OverlayPlaneList assigns;
  assigns.push_back(ui::OverlayPlane(fake_buffer_, nullptr));

  InitializeDrmState(/*crtc_count=*/2, /*planes_per_crtc=*/2);
  fake_drm_->InitializeState(crtc_properties_, plane_properties_,
                             property_names_, /* use_atomic= */ false);

  EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns, crtc_properties_[0].id, nullptr));
  EXPECT_EQ(1u, state_.plane_list.size());
  // Pretend we committed the frame.
  state_.plane_list.swap(state_.old_plane_list);
  fake_drm_->plane_manager()->BeginFrame(&state_);
  ui::HardwareDisplayPlane* old_plane = state_.old_plane_list[0];
  // The same plane should be used.
  EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns, crtc_properties_[0].id, nullptr));
  EXPECT_EQ(1u, state_.plane_list.size());
  EXPECT_EQ(state_.plane_list[0], old_plane);
}

TEST_F(HardwareDisplayPlaneManagerTest, MultipleFramesDifferentPlanes) {
  ui::OverlayPlaneList assigns;
  assigns.push_back(ui::OverlayPlane(fake_buffer_, nullptr));

  InitializeDrmState(/*crtc_count=*/2, /*planes_per_crtc=*/2);
  fake_drm_->InitializeState(crtc_properties_, plane_properties_,
                             property_names_, /* use_atomic= */ false);

  EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns, crtc_properties_[0].id, nullptr));
  EXPECT_EQ(1u, state_.plane_list.size());
  // The other plane should be used.
  EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns, crtc_properties_[0].id, nullptr));
  EXPECT_EQ(2u, state_.plane_list.size());
  EXPECT_NE(state_.plane_list[0], state_.plane_list[1]);
}

TEST_F(HardwareDisplayPlaneManagerTest, SharedPlanes) {
  ui::OverlayPlaneList assigns;
  scoped_refptr<ui::MockScanoutBuffer> buffer =
      new ui::MockScanoutBuffer(gfx::Size(1, 1));

  assigns.push_back(ui::OverlayPlane(fake_buffer_, nullptr));
  assigns.push_back(ui::OverlayPlane(buffer, nullptr));

  InitializeDrmState(/*crtc_count=*/2, /*planes_per_crtc=*/1);
  ui::MockDrmDevice::PlaneProperties plane_prop;
  plane_prop.id = 102;
  plane_prop.crtc_mask = (1 << 0) | (1 << 1);
  plane_prop.properties = {
      {.id = kTypePropId, .value = DRM_PLANE_TYPE_OVERLAY},
      {.id = kInFormatsPropId, .value = kInFormatsBlobPropId},
  };
  plane_properties_.emplace_back(std::move(plane_prop));
  fake_drm_->InitializeState(crtc_properties_, plane_properties_,
                             property_names_, /* use_atomic= */ false);

  EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns, crtc_properties_[1].id, nullptr));
  EXPECT_EQ(2u, state_.plane_list.size());
  // The shared plane is now unavailable for use by the other CRTC.
  EXPECT_FALSE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns, crtc_properties_[0].id, nullptr));
}

TEST_F(HardwareDisplayPlaneManagerTest, CheckFramebufferFormatMatch) {
  ui::OverlayPlaneList assigns;
  scoped_refptr<ui::MockScanoutBuffer> buffer =
      new ui::MockScanoutBuffer(kDefaultBufferSize, kDummyFormat);
  assigns.push_back(ui::OverlayPlane(buffer, nullptr));

  InitializeDrmState(/*crtc_count=*/2, /*planes_per_crtc=*/1);
  fake_drm_->InitializeState(crtc_properties_, plane_properties_,
                             property_names_, /* use_atomic= */ false);

  fake_drm_->plane_manager()->BeginFrame(&state_);
  // This should return false as plane manager creates planes which support
  // DRM_FORMAT_XRGB8888 while buffer returns kDummyFormat as its pixelFormat.
  EXPECT_FALSE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns, crtc_properties_[0].id, nullptr));
  assigns.clear();
  scoped_refptr<ui::MockScanoutBuffer> xrgb_buffer =
      new ui::MockScanoutBuffer(kDefaultBufferSize);
  assigns.push_back(ui::OverlayPlane(xrgb_buffer, nullptr));
  fake_drm_->plane_manager()->BeginFrame(&state_);
  EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns, crtc_properties_[0].id, nullptr));
  fake_drm_->plane_manager()->BeginFrame(&state_);
  EXPECT_FALSE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns, crtc_properties_[0].id, nullptr));
}

TEST_F(HardwareDisplayPlaneManagerTest, UnusedPlanesAreReleased) {
  InitializeDrmState(/*crtc_count=*/2, /*planes_per_crtc=*/2);
  fake_drm_->InitializeState(crtc_properties_, plane_properties_,
                             property_names_, /* use_atomic= */ false);

  ui::OverlayPlaneList assigns;
  scoped_refptr<ui::MockScanoutBuffer> primary_buffer =
      new ui::MockScanoutBuffer(kDefaultBufferSize);
  scoped_refptr<ui::MockScanoutBuffer> overlay_buffer =
      new ui::MockScanoutBuffer(gfx::Size(1, 1));
  assigns.push_back(ui::OverlayPlane(primary_buffer, nullptr));
  assigns.push_back(ui::OverlayPlane(overlay_buffer, nullptr));
  ui::HardwareDisplayPlaneList hdpl;
  ui::CrtcController crtc(fake_drm_, crtc_properties_[0].id, 0);
  fake_drm_->plane_manager()->BeginFrame(&hdpl);
  EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &hdpl, assigns, crtc_properties_[0].id, &crtc));
  EXPECT_TRUE(fake_drm_->plane_manager()->Commit(&hdpl, false));
  assigns.clear();
  assigns.push_back(ui::OverlayPlane(primary_buffer, nullptr));
  fake_drm_->plane_manager()->BeginFrame(&hdpl);
  EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &hdpl, assigns, crtc_properties_[0].id, &crtc));
  EXPECT_EQ(0, fake_drm_->get_overlay_clear_call_count());
  EXPECT_TRUE(fake_drm_->plane_manager()->Commit(&hdpl, false));
  EXPECT_EQ(1, fake_drm_->get_overlay_clear_call_count());
}

class FakeFenceFD {
 public:
  FakeFenceFD();

  gfx::GpuFence* GetGpuFence() const;
  void Signal() const;

 private:
  base::ScopedFD read_fd;
  base::ScopedFD write_fd;
  std::unique_ptr<gfx::GpuFence> gpu_fence;
};

FakeFenceFD::FakeFenceFD() {
  int fds[2];
  base::CreateLocalNonBlockingPipe(fds);
  read_fd = base::ScopedFD(fds[0]);
  write_fd = base::ScopedFD(fds[1]);

  gfx::GpuFenceHandle handle;
  handle.type = gfx::GpuFenceHandleType::kAndroidNativeFenceSync;
  handle.native_fd =
      base::FileDescriptor(HANDLE_EINTR(dup(read_fd.get())), true);
  gpu_fence = std::make_unique<gfx::GpuFence>(handle);
}

gfx::GpuFence* FakeFenceFD::GetGpuFence() const {
  return gpu_fence.get();
}

void FakeFenceFD::Signal() const {
  base::WriteFileDescriptor(write_fd.get(), "a", 1);
}

class HardwareDisplayPlaneManagerPlanesReadyTest : public testing::Test {
 protected:
  HardwareDisplayPlaneManagerPlanesReadyTest() = default;

  void UseLegacyManager();
  void UseAtomicManager();
  void RequestPlanesReady(const ui::OverlayPlaneList& planes);

  std::unique_ptr<ui::HardwareDisplayPlaneManager> plane_manager_;
  bool callback_called = false;
  base::test::ScopedTaskEnvironment task_env_{
      base::test::ScopedTaskEnvironment::MainThreadType::DEFAULT,
      base::test::ScopedTaskEnvironment::ExecutionMode::QUEUED};
  const scoped_refptr<ui::ScanoutBuffer> scanout_buffer{
      new ui::MockScanoutBuffer(kDefaultBufferSize)};
  const FakeFenceFD fake_fence_fd1;
  const FakeFenceFD fake_fence_fd2;

  const ui::OverlayPlaneList planes_without_fences_{
      ui::OverlayPlane(scanout_buffer, nullptr),
      ui::OverlayPlane(scanout_buffer, nullptr)};
  const ui::OverlayPlaneList planes_with_fences_{
      ui::OverlayPlane(scanout_buffer, fake_fence_fd1.GetGpuFence()),
      ui::OverlayPlane(scanout_buffer, fake_fence_fd2.GetGpuFence())};

 private:
  DISALLOW_COPY_AND_ASSIGN(HardwareDisplayPlaneManagerPlanesReadyTest);
};

void HardwareDisplayPlaneManagerPlanesReadyTest::RequestPlanesReady(
    const ui::OverlayPlaneList& planes) {
  auto set_true = [](bool* b) { *b = true; };
  plane_manager_->RequestPlanesReadyCallback(
      planes, base::BindOnce(set_true, &callback_called));
}

void HardwareDisplayPlaneManagerPlanesReadyTest::UseLegacyManager() {
  plane_manager_ = std::make_unique<ui::HardwareDisplayPlaneManagerLegacy>();
}

void HardwareDisplayPlaneManagerPlanesReadyTest::UseAtomicManager() {
  plane_manager_ = std::make_unique<ui::HardwareDisplayPlaneManagerAtomic>();
}

TEST_F(HardwareDisplayPlaneManagerPlanesReadyTest,
       LegacyWithoutFencesIsAsynchronousWithoutFenceWait) {
  UseLegacyManager();
  RequestPlanesReady(planes_without_fences_);

  EXPECT_FALSE(callback_called);

  task_env_.RunUntilIdle();

  EXPECT_TRUE(callback_called);
}

TEST_F(HardwareDisplayPlaneManagerPlanesReadyTest,
       LegacyWithFencesIsAsynchronousWithFenceWait) {
  UseLegacyManager();
  RequestPlanesReady(planes_with_fences_);

  EXPECT_FALSE(callback_called);

  fake_fence_fd1.Signal();
  fake_fence_fd2.Signal();

  EXPECT_FALSE(callback_called);

  task_env_.RunUntilIdle();

  EXPECT_TRUE(callback_called);
}

TEST_F(HardwareDisplayPlaneManagerPlanesReadyTest,
       AtomicWithoutFencesIsAsynchronousWithoutFenceWait) {
  UseAtomicManager();
  RequestPlanesReady(planes_without_fences_);

  EXPECT_FALSE(callback_called);

  task_env_.RunUntilIdle();

  EXPECT_TRUE(callback_called);
}

TEST_F(HardwareDisplayPlaneManagerPlanesReadyTest,
       AtomicWithFencesIsAsynchronousWithoutFenceWait) {
  UseAtomicManager();
  RequestPlanesReady(planes_with_fences_);

  EXPECT_FALSE(callback_called);

  task_env_.RunUntilIdle();

  EXPECT_TRUE(callback_called);
}

class HardwareDisplayPlaneAtomicMock : public ui::HardwareDisplayPlaneAtomic {
 public:
  HardwareDisplayPlaneAtomicMock() : ui::HardwareDisplayPlaneAtomic(1) {}
  ~HardwareDisplayPlaneAtomicMock() override {}

  bool SetPlaneData(drmModeAtomicReq* property_set,
                    uint32_t crtc_id,
                    uint32_t framebuffer,
                    const gfx::Rect& crtc_rect,
                    const gfx::Rect& src_rect,
                    const gfx::OverlayTransform transform,
                    int in_fence_fd) override {
    framebuffer_ = framebuffer;
    return true;
  }
  uint32_t framebuffer() const { return framebuffer_; }

 private:
  uint32_t framebuffer_ = 0;
};

TEST(HardwareDisplayPlaneManagerAtomic, EnableBlend) {
  auto plane_manager =
      std::make_unique<ui::HardwareDisplayPlaneManagerAtomic>();
  ui::HardwareDisplayPlaneList plane_list;
  HardwareDisplayPlaneAtomicMock hw_plane;
  scoped_refptr<ui::ScanoutBuffer> buffer =
      new ui::MockScanoutBuffer(kDefaultBufferSize);
  ui::OverlayPlane overlay(buffer, nullptr);
  overlay.enable_blend = true;
  plane_manager->SetPlaneData(&plane_list, &hw_plane, overlay, 1, gfx::Rect(),
                              nullptr);
  EXPECT_EQ(hw_plane.framebuffer(), buffer->GetFramebufferId());

  overlay.enable_blend = false;
  plane_manager->SetPlaneData(&plane_list, &hw_plane, overlay, 1, gfx::Rect(),
                              nullptr);
  EXPECT_EQ(hw_plane.framebuffer(), buffer->GetOpaqueFramebufferId());
}

}  // namespace
