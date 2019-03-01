// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/drm_gpu_display_manager.h"

#include <string>
#include <unordered_map>
#include <utility>

#include "base/rand_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/ozone/platform/drm/gpu/drm_device_generator.h"
#include "ui/ozone/platform/drm/gpu/drm_device_manager.h"
#include "ui/ozone/platform/drm/gpu/drm_display.h"
#include "ui/ozone/platform/drm/gpu/mock_drm_device.h"
#include "ui/ozone/platform/drm/gpu/mock_gbm_device.h"
#include "ui/ozone/platform/drm/gpu/screen_manager.h"

namespace ui {

namespace {

constexpr int kMaxSupportDisplayNum = 2;

class MockDrmDeviceManager : public DrmDeviceManager {
 public:
  MockDrmDeviceManager(std::unique_ptr<DrmDeviceGenerator> drm_device_generator)
      : DrmDeviceManager(std::move(drm_device_generator)) {}
  ~MockDrmDeviceManager() = default;

  void AddDrmDeviceForTesting(scoped_refptr<DrmDevice> drm_device) {
    devices_.push_back(drm_device);
  }
};

class DrmGpuDisplayManagerTestHelper {
 public:
  DrmGpuDisplayManagerTestHelper(size_t max_support_display_num) {
    for (size_t i = 1; i <= max_support_display_num; i++)
      crtc_resources_.push_back(i);
  }

  HardwareDisplayControllerInfos GetAvailableDisplayControllerInfos(
      int fd,
      bool* support_all_displays);

  const std::unordered_map<uint32_t, uint32_t>& intermediate_mappings() const {
    return intermediate_mappings_;
  }

  void AddDisplay(uint32_t connector_id) {
    connector_resources_.push_back(connector_id);
  }

 private:
  // Record the mappings between connector and crtc before crtc reallocation.
  std::unordered_map<uint32_t, uint32_t> intermediate_mappings_;

  std::vector<uint32_t> connector_resources_;
  std::vector<uint32_t> crtc_resources_;
};

HardwareDisplayControllerInfos
DrmGpuDisplayManagerTestHelper::GetAvailableDisplayControllerInfos(
    int fd,
    bool* support_all_displays) {
  intermediate_mappings_.clear();
  *support_all_displays = true;
  HardwareDisplayControllerInfos ret;
  auto crtc_iter = crtc_resources_.rbegin();

  // Pair connector with crtc in a dummy way. If there is no available crtc, the
  // assigned crtc should be a null pointer. Use reverse iterator to ensure that
  // the most recent connected display has associated crtc before crtc
  // reallocation. It can help to test the code block of crtc reallocation.
  for (auto connector_iter = connector_resources_.rbegin();
       connector_iter != connector_resources_.rend(); connector_iter++) {
    drmModeConnectorPtr connector = new drmModeConnector;
    connector->connector_id = *connector_iter;
    connector->encoders = nullptr;
    connector->prop_values = nullptr;
    connector->props = nullptr;
    connector->modes = nullptr;

    drmModeCrtcPtr crtc = nullptr;
    if (crtc_iter != crtc_resources_.rend()) {
      crtc = new drmModeCrtc;
      crtc->crtc_id = *crtc_iter;
      intermediate_mappings_[connector->connector_id] = crtc->crtc_id;
      crtc_iter++;
    } else {
      *support_all_displays = false;
    }

    ret.push_back(std::make_unique<HardwareDisplayControllerInfo>(
        ScopedDrmConnectorPtr(connector), ScopedDrmCrtcPtr(crtc),
        connector_iter - connector_resources_.rbegin()));
  }

  return ret;
}

class MockDrmGpuDisplayManager : public DrmGpuDisplayManager {
 public:
  MockDrmGpuDisplayManager(
      std::unique_ptr<DrmGpuDisplayManagerTestHelper> test_helper,
      ScreenManager* screen_manager,
      MockDrmDeviceManager* device_manager)
      : DrmGpuDisplayManager(screen_manager, device_manager),
        test_helper_(std::move(test_helper)) {}

  // Get the mappings between connector and crtc before crtc reallocation.
  const std::unordered_map<uint32_t, uint32_t>& intermediate_mappings() const {
    return test_helper_->intermediate_mappings();
  }

  void AddDisplay(uint32_t connector_id) {
    test_helper_->AddDisplay(connector_id);
  }

 private:
  HardwareDisplayControllerInfos QueryAvailableDisplayControllerInfos(
      int fd,
      bool* support_all_displays) const override {
    return test_helper_->GetAvailableDisplayControllerInfos(
        fd, support_all_displays);
  }

  MovableDisplaySnapshots GenerateParamsList(
      HardwareDisplayControllerInfos& display_infos,
      size_t device_index) override;

  // In test do nothing.
  void NotifyScreenManager(
      const std::vector<std::unique_ptr<DrmDisplay>>& new_displays,
      const std::vector<std::unique_ptr<DrmDisplay>>& old_displays)
      const override {}

  std::unique_ptr<DrmGpuDisplayManagerTestHelper> test_helper_;
};

MovableDisplaySnapshots MockDrmGpuDisplayManager::GenerateParamsList(
    HardwareDisplayControllerInfos& display_infos,
    size_t device_index) {
  MovableDisplaySnapshots params_list;

  size_t display_index = 0;
  for (auto& display_info : display_infos) {
    if (display_info->has_associated_crtc()) {
      displays_[display_index++]->UpdateForTesting(
          display_info->connector()->connector_id,
          display_info->crtc()->crtc_id);
      hardware_infos_.push_back(std::move(display_info));
    }

    // Generate dummy snapshots.
    params_list.push_back(std::make_unique<display::DisplaySnapshot>(
        /*display_id=*/display::kInvalidDisplayId, /*origin=*/gfx::Point(),
        /*physical_size=*/gfx::Size(),
        /*type=*/display::DisplayConnectionType(),
        /*is_aspect_preserving_scaling=*/false, /*has_overscan=*/false,
        /*has_color_correction_matrix=*/false,
        /*color_correction_in_linear_space=*/false,
        /*color_space=*/gfx::ColorSpace(), /*display_name=*/std::string(),
        /*sys_path=*/base::FilePath(),
        /*modes=*/display::DisplaySnapshot::DisplayModeList(),
        /*edid=*/std::vector<uint8_t>(), /*current_mode=*/nullptr,
        /*native_mode=*/nullptr, /*product_code=*/0,
        /*year_of_manufacture=*/display::kInvalidYearOfManufacture,
        /*maximum_cursor_size=*/gfx::Size(), /*has_associated_crtc=*/true));
  }

  return params_list;
}

}  // namespace

class DrmGpuDisplayManagerTest : public testing::Test {
 public:
  DrmGpuDisplayManagerTest() {}
  ~DrmGpuDisplayManagerTest() override {}

  // From testing::Test.
  void SetUp() override;
  void TearDown() override {
    screen_manager_.reset();
    device_manager_.reset();
    drm_gpu_display_manager_.reset();
  }

 protected:
  std::unique_ptr<ScreenManager> screen_manager_;
  std::unique_ptr<MockDrmDeviceManager> device_manager_;
  std::unique_ptr<MockDrmGpuDisplayManager> drm_gpu_display_manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DrmGpuDisplayManagerTest);
};

void DrmGpuDisplayManagerTest::SetUp() {
  auto gbm = std::make_unique<ui::MockGbmDevice>();
  MockDrmDevice* device = new MockDrmDevice(std::move(gbm));
  scoped_refptr<DrmDevice> drm_device(device);

  device_manager_.reset(new MockDrmDeviceManager(nullptr));
  device_manager_->AddDrmDeviceForTesting(drm_device);
  screen_manager_.reset(new ScreenManager());

  size_t max_support_display_num = kMaxSupportDisplayNum;
  drm_gpu_display_manager_.reset(new MockDrmGpuDisplayManager(
      std::make_unique<DrmGpuDisplayManagerTestHelper>(max_support_display_num),
      screen_manager_.get(), device_manager_.get()));
}

TEST_F(DrmGpuDisplayManagerTest, GetDisplays) {
  const uint32_t display_id1 = 1, display_id2 = 2, display_id3 = 3;

  // Connect two displays with device then update |hardware_infos_|.
  drm_gpu_display_manager_->AddDisplay(display_id1);
  drm_gpu_display_manager_->AddDisplay(display_id2);
  MovableDisplaySnapshots display_snapshots1 =
      drm_gpu_display_manager_->GetDisplays();
  EXPECT_EQ(display_snapshots1.size(), 2u);

  std::unordered_map<uint32_t, uint32_t> connector_crtc_map;

  // Record the mappings between connector and crtc.
  const auto& hardware_infos =
      drm_gpu_display_manager_->hardware_infos_for_test();
  size_t connected_display_size = hardware_infos.size();
  for (const auto& hardware_info : hardware_infos) {
    uint32_t connector_id = hardware_info->connector()->connector_id,
             crtc_id = hardware_info->crtc()->crtc_id;
    connector_crtc_map[connector_id] = crtc_id;
  }

  drm_gpu_display_manager_->AddDisplay(display_id3);
  MovableDisplaySnapshots display_snapshots2 =
      drm_gpu_display_manager_->GetDisplays();
  EXPECT_EQ(display_snapshots2.size(), 3u);

  // The third display has associated crtc before crtc reallocation.
  const auto& iter =
      drm_gpu_display_manager_->intermediate_mappings().find(display_id3);
  EXPECT_EQ(iter->second, 2u);

  // Note that device only supports at most two displays. So the third one
  // should not have associated crtc after reallocation (see comments in
  // DrmGpuDisplayManager::GetDisplays for more details). |hardware_infos|,
  // recording information of connected displays, should not change.
  const auto& updated_hardware_infos =
      drm_gpu_display_manager_->hardware_infos_for_test();
  EXPECT_EQ(connected_display_size, updated_hardware_infos.size());
  for (const auto& hardware_info : updated_hardware_infos) {
    uint32_t connector_id = hardware_info->connector()->connector_id,
             crtc_id = hardware_info->crtc()->crtc_id;

    uint32_t expected_crtc_id = connector_crtc_map[connector_id];
    EXPECT_EQ(crtc_id, expected_crtc_id);
  }
}
}  // namespace ui
