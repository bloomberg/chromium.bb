// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/chromeos/display_configurator.h"
#include "ui/display/chromeos/test/test_display_snapshot.h"
#include "ui/display/chromeos/touchscreen_delegate_impl.h"
#include "ui/display/types/chromeos/touchscreen_device_manager.h"

namespace ui {

namespace {

class MockTouchscreenDeviceManager : public TouchscreenDeviceManager {
 public:
  MockTouchscreenDeviceManager() {}
  virtual ~MockTouchscreenDeviceManager() {}

  void AddDevice(const TouchscreenDevice& device) {
    devices_.push_back(device);
  }

  // TouchscreenDeviceManager overrides:
  virtual std::vector<TouchscreenDevice> GetDevices() OVERRIDE {
    return devices_;
  }

 private:
  std::vector<TouchscreenDevice> devices_;

  DISALLOW_COPY_AND_ASSIGN(MockTouchscreenDeviceManager);
};

}  // namespace

class TouchscreenDelegateImplTest : public testing::Test {
 public:
  TouchscreenDelegateImplTest() {}
  virtual ~TouchscreenDelegateImplTest() {}

  virtual void SetUp() OVERRIDE {
    device_manager_ = new MockTouchscreenDeviceManager();
    delegate_.reset(new TouchscreenDelegateImpl(
        scoped_ptr<TouchscreenDeviceManager>(device_manager_)));

    // Internal display will always match to internal touchscreen. If internal
    // touchscreen can't be detected, it is then associated to a touch screen
    // with matching size.
    TestDisplaySnapshot* snapshot = new TestDisplaySnapshot();
    DisplayMode* mode = new DisplayMode(gfx::Size(1920, 1080), false, 60.0);
    snapshot->set_type(DISPLAY_CONNECTION_TYPE_INTERNAL);
    snapshot->set_modes(std::vector<const DisplayMode*>(1, mode));
    snapshot->set_native_mode(mode);
    displays_.push_back(snapshot);

    snapshot = new TestDisplaySnapshot();
    mode = new DisplayMode(gfx::Size(800, 600), false, 60.0);
    snapshot->set_modes(std::vector<const DisplayMode*>(1, mode));
    snapshot->set_native_mode(mode);
    displays_.push_back(snapshot);

    // Display without native mode. Must not be matched to any touch screen.
    displays_.push_back(new TestDisplaySnapshot());

    snapshot = new TestDisplaySnapshot();
    mode = new DisplayMode(gfx::Size(1024, 768), false, 60.0);
    snapshot->set_modes(std::vector<const DisplayMode*>(1, mode));
    snapshot->set_native_mode(mode);
    displays_.push_back(snapshot);
  }

  virtual void TearDown() OVERRIDE {
    // The snapshots do not own the modes, so we need to delete them.
    for (size_t i = 0; i < displays_.size(); ++i) {
      if (displays_[i]->native_mode())
        delete displays_[i]->native_mode();
    }

    displays_.clear();
  }

  std::vector<DisplayConfigurator::DisplayState> GetDisplayStates() {
    std::vector<DisplayConfigurator::DisplayState> states(displays_.size());
    for (size_t i = 0; i < displays_.size(); ++i)
      states[i].display = displays_[i];

    return states;
  }

 protected:
  MockTouchscreenDeviceManager* device_manager_;  // Not owned.
  scoped_ptr<TouchscreenDelegateImpl> delegate_;
  ScopedVector<DisplaySnapshot> displays_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TouchscreenDelegateImplTest);
};

TEST_F(TouchscreenDelegateImplTest, NoTouchscreens) {
  std::vector<DisplayConfigurator::DisplayState> display_states =
      GetDisplayStates();
  delegate_->AssociateTouchscreens(&display_states);

  for (size_t i = 0; i < display_states.size(); ++i)
    EXPECT_EQ(TouchscreenDevice::kInvalidId, display_states[i].touch_device_id);
}

TEST_F(TouchscreenDelegateImplTest, OneToOneMapping) {
  device_manager_->AddDevice(TouchscreenDevice(1, gfx::Size(800, 600), false));
  device_manager_->AddDevice(TouchscreenDevice(2, gfx::Size(1024, 768), false));

  std::vector<DisplayConfigurator::DisplayState> display_states =
      GetDisplayStates();
  delegate_->AssociateTouchscreens(&display_states);

  EXPECT_EQ(TouchscreenDevice::kInvalidId, display_states[0].touch_device_id);
  EXPECT_EQ(1, display_states[1].touch_device_id);
  EXPECT_EQ(TouchscreenDevice::kInvalidId, display_states[2].touch_device_id);
  EXPECT_EQ(2, display_states[3].touch_device_id);
}

TEST_F(TouchscreenDelegateImplTest, MapToCorrectDisplaySize) {
  device_manager_->AddDevice(TouchscreenDevice(2, gfx::Size(1024, 768), false));

  std::vector<DisplayConfigurator::DisplayState> display_states =
      GetDisplayStates();
  delegate_->AssociateTouchscreens(&display_states);

  EXPECT_EQ(TouchscreenDevice::kInvalidId, display_states[0].touch_device_id);
  EXPECT_EQ(TouchscreenDevice::kInvalidId, display_states[1].touch_device_id);
  EXPECT_EQ(TouchscreenDevice::kInvalidId, display_states[2].touch_device_id);
  EXPECT_EQ(2, display_states[3].touch_device_id);
}

TEST_F(TouchscreenDelegateImplTest, MapWhenSizeDiffersByOne) {
  device_manager_->AddDevice(TouchscreenDevice(1, gfx::Size(801, 600), false));
  device_manager_->AddDevice(TouchscreenDevice(2, gfx::Size(1023, 768), false));

  std::vector<DisplayConfigurator::DisplayState> display_states =
      GetDisplayStates();
  delegate_->AssociateTouchscreens(&display_states);

  EXPECT_EQ(TouchscreenDevice::kInvalidId, display_states[0].touch_device_id);
  EXPECT_EQ(1, display_states[1].touch_device_id);
  EXPECT_EQ(TouchscreenDevice::kInvalidId, display_states[2].touch_device_id);
  EXPECT_EQ(2, display_states[3].touch_device_id);
}

TEST_F(TouchscreenDelegateImplTest, MapWhenSizesDoNotMatch) {
  device_manager_->AddDevice(TouchscreenDevice(1, gfx::Size(1022, 768), false));
  device_manager_->AddDevice(TouchscreenDevice(2, gfx::Size(802, 600), false));

  std::vector<DisplayConfigurator::DisplayState> display_states =
      GetDisplayStates();
  delegate_->AssociateTouchscreens(&display_states);

  EXPECT_EQ(TouchscreenDevice::kInvalidId, display_states[0].touch_device_id);
  EXPECT_EQ(1, display_states[1].touch_device_id);
  EXPECT_EQ(TouchscreenDevice::kInvalidId, display_states[2].touch_device_id);
  EXPECT_EQ(2, display_states[3].touch_device_id);
}

TEST_F(TouchscreenDelegateImplTest, MapInternalTouchscreen) {
  device_manager_->AddDevice(
      TouchscreenDevice(1, gfx::Size(1920, 1080), false));
  device_manager_->AddDevice(TouchscreenDevice(2, gfx::Size(9999, 888), true));

  std::vector<DisplayConfigurator::DisplayState> display_states =
      GetDisplayStates();
  delegate_->AssociateTouchscreens(&display_states);

  // Internal touchscreen is always mapped to internal display.
  EXPECT_EQ(2, display_states[0].touch_device_id);
  EXPECT_EQ(1, display_states[1].touch_device_id);
  EXPECT_EQ(TouchscreenDevice::kInvalidId, display_states[2].touch_device_id);
  EXPECT_EQ(TouchscreenDevice::kInvalidId, display_states[3].touch_device_id);
}

}  // namespace ui
