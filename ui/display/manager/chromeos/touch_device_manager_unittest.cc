// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/chromeos/touch_device_manager.h"

#include <memory>
#include <string>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/display/screen_base.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/touchscreen_device.h"

namespace display {
namespace {
TouchDeviceIdentifier ToIdentifier(const ui::TouchscreenDevice& device) {
  return TouchDeviceIdentifier::FromDevice(device);
}

ui::TouchscreenDevice CreateTouchscreenDevice(int id,
                                              ui::InputDeviceType type,
                                              const gfx::Size& size) {
  ui::TouchscreenDevice device(id, type, base::IntToString(id), size, 0);
  device.vendor_id = id * id;
  device.product_id = device.vendor_id * id;
  return device;
}

}  // namespace

using DisplayInfoList = std::vector<ManagedDisplayInfo>;

class TouchAssociationTest : public testing::Test {
 public:
  TouchAssociationTest() {}
  ~TouchAssociationTest() override {}

  DisplayManager* display_manager() { return display_manager_.get(); }

  TouchDeviceManager* touch_device_manager() { return touch_device_manager_; }

  // testing::Test:
  void SetUp() override {
    // Recreate for each test, DisplayManager has a lot of state.
    display_manager_ =
        std::make_unique<DisplayManager>(std::make_unique<ScreenBase>());
    touch_device_manager_ = display_manager_->touch_device_manager();

    // Internal display will always match to internal touchscreen. If internal
    // touchscreen can't be detected, it is then associated to a touch screen
    // with matching size.
    {
      ManagedDisplayInfo display(1, "1", false);
      const ManagedDisplayMode mode(
          gfx::Size(1920, 1080), 60.0, false /* interlaced */,
          true /* native */, 1.0 /* ui_scale */, 1.0 /* device_scale_factor */);
      ManagedDisplayInfo::ManagedDisplayModeList modes(1, mode);
      display.SetManagedDisplayModes(modes);
      displays_.push_back(display);
    }

    {
      ManagedDisplayInfo display(2, "2", false);

      const ManagedDisplayMode mode(
          gfx::Size(800, 600), 60.0, false /* interlaced */, true /* native */,
          1.0 /* ui_scale */, 1.0 /* device_scale_factor */);
      ManagedDisplayInfo::ManagedDisplayModeList modes(1, mode);
      display.SetManagedDisplayModes(modes);
      displays_.push_back(display);
    }

    // Display without native mode. Must not be matched to any touch screen.
    {
      ManagedDisplayInfo display(3, "3", false);
      displays_.push_back(display);
    }

    {
      ManagedDisplayInfo display(4, "4", false);

      const ManagedDisplayMode mode(
          gfx::Size(1024, 768), 60.0, false /* interlaced */,
          /* native */ true, 1.0 /* ui_scale */, 1.0 /* device_scale_factor */);
      ManagedDisplayInfo::ManagedDisplayModeList modes(1, mode);
      display.SetManagedDisplayModes(modes);
      displays_.push_back(display);
    }
  }

  void TearDown() override { displays_.clear(); }

 protected:
  DisplayInfoList displays_;
  std::unique_ptr<DisplayManager> display_manager_;
  TouchDeviceManager* touch_device_manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TouchAssociationTest);
};

TEST_F(TouchAssociationTest, NoTouchscreens) {
  std::vector<ui::TouchscreenDevice> devices;

  test::ScopedSetInternalDisplayId set_internal(display_manager(),
                                                displays_[0].id());
  touch_device_manager()->AssociateTouchscreens(&displays_, devices);

  for (size_t i = 0; i < displays_.size(); ++i)
    EXPECT_EQ(0u, displays_[i].touch_device_identifiers().size());
}

// Verify that if there are a lot of touchscreens, they will all get associated
// with a display.
TEST_F(TouchAssociationTest, ManyTouchscreens) {
  std::vector<ui::TouchscreenDevice> devices;
  for (int i = 0; i < 5; ++i) {
    devices.push_back(CreateTouchscreenDevice(
        i, ui::InputDeviceType::INPUT_DEVICE_EXTERNAL, gfx::Size(256, 256)));
  }

  DisplayInfoList displays;
  displays.push_back(displays_[3]);

  test::ScopedSetInternalDisplayId set_internal(display_manager(),
                                                displays_[0].id());
  touch_device_manager()->AssociateTouchscreens(&displays, devices);

  for (int i = 0; i < 5; ++i)
    EXPECT_TRUE(displays[0].HasTouchDevice(ToIdentifier(devices[i])));
}

TEST_F(TouchAssociationTest, OneToOneMapping) {
  std::vector<ui::TouchscreenDevice> devices;
  devices.push_back(CreateTouchscreenDevice(
      1, ui::InputDeviceType::INPUT_DEVICE_EXTERNAL, gfx::Size(800, 600)));
  devices.push_back(CreateTouchscreenDevice(
      2, ui::InputDeviceType::INPUT_DEVICE_EXTERNAL, gfx::Size(1024, 768)));

  test::ScopedSetInternalDisplayId set_internal(display_manager(),
                                                displays_[0].id());
  touch_device_manager()->AssociateTouchscreens(&displays_, devices);

  EXPECT_EQ(0u, displays_[0].touch_device_identifiers().size());
  EXPECT_EQ(1u, displays_[1].touch_device_identifiers().size());
  EXPECT_TRUE(displays_[1].HasTouchDevice(ToIdentifier(devices[0])));
  EXPECT_EQ(0u, displays_[2].touch_device_identifiers().size());
  EXPECT_EQ(1u, displays_[3].touch_device_identifiers().size());
  EXPECT_TRUE(displays_[3].HasTouchDevice(ToIdentifier(devices[1])));
}

TEST_F(TouchAssociationTest, MapToCorrectDisplaySize) {
  std::vector<ui::TouchscreenDevice> devices;
  devices.push_back(CreateTouchscreenDevice(
      2, ui::InputDeviceType::INPUT_DEVICE_EXTERNAL, gfx::Size(1024, 768)));

  test::ScopedSetInternalDisplayId set_internal(display_manager(),
                                                displays_[0].id());
  touch_device_manager()->AssociateTouchscreens(&displays_, devices);

  EXPECT_EQ(0u, displays_[0].touch_device_identifiers().size());
  EXPECT_EQ(0u, displays_[1].touch_device_identifiers().size());
  EXPECT_EQ(0u, displays_[2].touch_device_identifiers().size());
  EXPECT_EQ(1u, displays_[3].touch_device_identifiers().size());
  EXPECT_TRUE(displays_[3].HasTouchDevice(ToIdentifier(devices[0])));
}

TEST_F(TouchAssociationTest, MapWhenSizeDiffersByOne) {
  std::vector<ui::TouchscreenDevice> devices;
  devices.push_back(CreateTouchscreenDevice(
      1, ui::InputDeviceType::INPUT_DEVICE_EXTERNAL, gfx::Size(801, 600)));
  devices.push_back(CreateTouchscreenDevice(
      2, ui::InputDeviceType::INPUT_DEVICE_EXTERNAL, gfx::Size(1023, 768)));

  test::ScopedSetInternalDisplayId set_internal(display_manager(),
                                                displays_[0].id());
  touch_device_manager()->AssociateTouchscreens(&displays_, devices);

  EXPECT_EQ(0u, displays_[0].touch_device_identifiers().size());
  EXPECT_EQ(1u, displays_[1].touch_device_identifiers().size());
  EXPECT_TRUE(displays_[1].HasTouchDevice(ToIdentifier(devices[0])));
  EXPECT_EQ(0u, displays_[2].touch_device_identifiers().size());
  EXPECT_EQ(1u, displays_[3].touch_device_identifiers().size());
  EXPECT_TRUE(displays_[3].HasTouchDevice(ToIdentifier(devices[1])));
}

TEST_F(TouchAssociationTest, MapWhenSizesDoNotMatch) {
  std::vector<ui::TouchscreenDevice> devices;
  devices.push_back(CreateTouchscreenDevice(
      1, ui::InputDeviceType::INPUT_DEVICE_EXTERNAL, gfx::Size(1022, 768)));
  devices.push_back(CreateTouchscreenDevice(
      2, ui::InputDeviceType::INPUT_DEVICE_EXTERNAL, gfx::Size(802, 600)));

  DisplayInfoList displays;
  displays.push_back(displays_[0]);
  displays.push_back(displays_[1]);

  test::ScopedSetInternalDisplayId set_internal(display_manager(),
                                                displays[0].id());
  touch_device_manager()->AssociateTouchscreens(&displays, devices);

  // The touch devices should match to the internal display if they were not
  // matched in any of the steps.
  EXPECT_EQ(0u, displays[0].touch_device_identifiers().size());
  EXPECT_EQ(2u, displays[1].touch_device_identifiers().size());
  EXPECT_TRUE(displays[1].HasTouchDevice(ToIdentifier(devices[0])));
  EXPECT_TRUE(displays[1].HasTouchDevice(ToIdentifier(devices[1])));
}

TEST_F(TouchAssociationTest, MapInternalTouchscreen) {
  std::vector<ui::TouchscreenDevice> devices;
  devices.push_back(CreateTouchscreenDevice(
      1, ui::InputDeviceType::INPUT_DEVICE_EXTERNAL, gfx::Size(1920, 1080)));
  devices.push_back(CreateTouchscreenDevice(
      2, ui::InputDeviceType::INPUT_DEVICE_INTERNAL, gfx::Size(9999, 888)));

  DisplayInfoList displays;
  displays.push_back(displays_[0]);
  displays.push_back(displays_[1]);

  test::ScopedSetInternalDisplayId set_internal(display_manager(),
                                                displays[0].id());
  touch_device_manager()->AssociateTouchscreens(&displays, devices);

  // Internal touchscreen is always mapped to internal display.
  EXPECT_EQ(1u, displays[0].touch_device_identifiers().size());
  EXPECT_TRUE(displays[0].HasTouchDevice(ToIdentifier(devices[1])));
  EXPECT_EQ(1u, displays[1].touch_device_identifiers().size());
  EXPECT_TRUE(displays[1].HasTouchDevice(ToIdentifier(devices[0])));
}

TEST_F(TouchAssociationTest, MultipleInternal) {
  std::vector<ui::TouchscreenDevice> devices;
  devices.push_back(CreateTouchscreenDevice(
      1, ui::InputDeviceType::INPUT_DEVICE_INTERNAL, gfx::Size(1920, 1080)));
  devices.push_back(CreateTouchscreenDevice(
      2, ui::InputDeviceType::INPUT_DEVICE_INTERNAL, gfx::Size(1920, 1080)));

  test::ScopedSetInternalDisplayId set_internal(display_manager(),
                                                displays_[0].id());
  touch_device_manager()->AssociateTouchscreens(&displays_, devices);

  EXPECT_EQ(2u, displays_[0].touch_device_identifiers().size());
  EXPECT_EQ(0u, displays_[1].touch_device_identifiers().size());
  EXPECT_EQ(0u, displays_[2].touch_device_identifiers().size());
  EXPECT_EQ(0u, displays_[3].touch_device_identifiers().size());
}

TEST_F(TouchAssociationTest, MultipleInternalAndExternal) {
  std::vector<ui::TouchscreenDevice> devices;
  devices.push_back(CreateTouchscreenDevice(
      1, ui::InputDeviceType::INPUT_DEVICE_INTERNAL, gfx::Size(1920, 1080)));
  devices.push_back(CreateTouchscreenDevice(
      2, ui::InputDeviceType::INPUT_DEVICE_INTERNAL, gfx::Size(1920, 1080)));
  devices.push_back(CreateTouchscreenDevice(
      3, ui::InputDeviceType::INPUT_DEVICE_EXTERNAL, gfx::Size(1024, 768)));

  test::ScopedSetInternalDisplayId set_internal(display_manager(),
                                                displays_[0].id());
  touch_device_manager()->AssociateTouchscreens(&displays_, devices);

  EXPECT_EQ(2u, displays_[0].touch_device_identifiers().size());
  EXPECT_TRUE(displays_[0].HasTouchDevice(ToIdentifier(devices[0])));
  EXPECT_TRUE(displays_[0].HasTouchDevice(ToIdentifier(devices[1])));
  EXPECT_EQ(0u, displays_[1].touch_device_identifiers().size());
  EXPECT_EQ(0u, displays_[2].touch_device_identifiers().size());
  EXPECT_EQ(1u, displays_[3].touch_device_identifiers().size());
  EXPECT_TRUE(displays_[3].HasTouchDevice(ToIdentifier(devices[2])));
}

// crbug.com/515201
TEST_F(TouchAssociationTest, TestWithNoInternalDisplay) {
  std::vector<ui::TouchscreenDevice> devices;
  devices.push_back(CreateTouchscreenDevice(
      1, ui::InputDeviceType::INPUT_DEVICE_EXTERNAL, gfx::Size(1920, 1080)));
  devices.push_back(CreateTouchscreenDevice(
      2, ui::InputDeviceType::INPUT_DEVICE_INTERNAL, gfx::Size(9999, 888)));

  // Internal touchscreen should not be associated with any display
  touch_device_manager()->AssociateTouchscreens(&displays_, devices);

  EXPECT_EQ(1u, displays_[0].touch_device_identifiers().size());
  EXPECT_TRUE(displays_[0].HasTouchDevice(ToIdentifier(devices[0])));
  EXPECT_EQ(0u, displays_[1].touch_device_identifiers().size());
  EXPECT_EQ(0u, displays_[2].touch_device_identifiers().size());
  EXPECT_EQ(0u, displays_[3].touch_device_identifiers().size());
}

TEST_F(TouchAssociationTest, MatchRemainingDevicesToInternalDisplay) {
  std::vector<ui::TouchscreenDevice> devices;
  devices.push_back(CreateTouchscreenDevice(
      1, ui::InputDeviceType::INPUT_DEVICE_EXTERNAL, gfx::Size(123, 456)));
  devices.push_back(CreateTouchscreenDevice(
      2, ui::InputDeviceType::INPUT_DEVICE_EXTERNAL, gfx::Size(234, 567)));
  devices.push_back(CreateTouchscreenDevice(
      3, ui::InputDeviceType::INPUT_DEVICE_EXTERNAL, gfx::Size(345, 678)));

  test::ScopedSetInternalDisplayId set_internal(display_manager(),
                                                displays_[0].id());
  touch_device_manager()->AssociateTouchscreens(&displays_, devices);

  EXPECT_EQ(3u, displays_[0].touch_device_identifiers().size());
  EXPECT_TRUE(displays_[0].HasTouchDevice(ToIdentifier(devices[0])));
  EXPECT_TRUE(displays_[0].HasTouchDevice(ToIdentifier(devices[1])));
  EXPECT_TRUE(displays_[0].HasTouchDevice(ToIdentifier(devices[2])));
  EXPECT_EQ(0u, displays_[1].touch_device_identifiers().size());
  EXPECT_EQ(0u, displays_[2].touch_device_identifiers().size());
  EXPECT_EQ(0u, displays_[3].touch_device_identifiers().size());
}

TEST_F(TouchAssociationTest, MatchRemainingDevicesWithInternalDisplayPresent) {
  std::vector<ui::TouchscreenDevice> devices;
  devices.push_back(CreateTouchscreenDevice(
      1, ui::InputDeviceType::INPUT_DEVICE_EXTERNAL, gfx::Size(123, 456)));
  devices.push_back(CreateTouchscreenDevice(
      2, ui::InputDeviceType::INPUT_DEVICE_EXTERNAL, gfx::Size(234, 567)));
  devices.push_back(CreateTouchscreenDevice(
      3, ui::InputDeviceType::INPUT_DEVICE_EXTERNAL, gfx::Size(345, 678)));

  touch_device_manager()->AssociateTouchscreens(&displays_, devices);

  std::size_t total = 0;
  for (const auto& display : displays_)
    total += display.touch_device_identifiers().size();

  // Make sure all devices were matched.
  EXPECT_EQ(total, devices.size());
}

}  // namespace display
