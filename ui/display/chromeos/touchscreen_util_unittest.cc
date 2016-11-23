// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/chromeos/touchscreen_util.h"

#include <memory>
#include <string>
#include <vector>

#include "base/memory/ptr_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/display/screen_base.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/devices/input_device.h"

namespace display {

using DisplayInfoList = std::vector<ManagedDisplayInfo>;

class TouchscreenUtilTest : public testing::Test {
 public:
  TouchscreenUtilTest() {}
  ~TouchscreenUtilTest() override {}

  DisplayManager* display_manager() { return display_manager_.get(); }

  // testing::Test:
  void SetUp() override {
    // Recreate for each test, DisplayManager has a lot of state.
    display_manager_ =
        base::MakeUnique<DisplayManager>(base::MakeUnique<ScreenBase>());

    // Internal display will always match to internal touchscreen. If internal
    // touchscreen can't be detected, it is then associated to a touch screen
    // with matching size.
    {
      ManagedDisplayInfo display(1, "1", false);
      scoped_refptr<ManagedDisplayMode> mode(new ManagedDisplayMode(
          gfx::Size(1920, 1080), 60.0, false /* interlaced */,
          true /* native */, 1.0 /* ui_scale */,
          1.0 /* device_scale_factor */));
      ManagedDisplayInfo::ManagedDisplayModeList modes(1, mode);
      display.SetManagedDisplayModes(modes);
      displays_.push_back(display);
    }

    {
      ManagedDisplayInfo display(2, "2", false);

      scoped_refptr<ManagedDisplayMode> mode(new ManagedDisplayMode(
          gfx::Size(800, 600), 60.0, false /* interlaced */, true /* native */,
          1.0 /* ui_scale */, 1.0 /* device_scale_factor */));
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

      scoped_refptr<ManagedDisplayMode> mode(new ManagedDisplayMode(
          gfx::Size(1024, 768), 60.0, false /* interlaced */,
          /* native */ true, 1.0 /* ui_scale */,
          1.0 /* device_scale_factor */));
      ManagedDisplayInfo::ManagedDisplayModeList modes(1, mode);
      display.SetManagedDisplayModes(modes);
      displays_.push_back(display);
    }
  }

  void TearDown() override { displays_.clear(); }

 protected:
  DisplayInfoList displays_;
  std::unique_ptr<DisplayManager> display_manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TouchscreenUtilTest);
};

TEST_F(TouchscreenUtilTest, NoTouchscreens) {
  std::vector<ui::TouchscreenDevice> devices;

  test::ScopedSetInternalDisplayId set_internal(display_manager(),
                                                displays_[0].id());
  AssociateTouchscreens(&displays_, devices);

  for (size_t i = 0; i < displays_.size(); ++i)
    EXPECT_EQ(0u, displays_[i].input_devices().size());
}

// Verify that if there are a lot of touchscreens, they will all get associated
// with a display.
TEST_F(TouchscreenUtilTest, ManyTouchscreens) {
  std::vector<ui::TouchscreenDevice> devices;
  for (int i = 0; i < 5; ++i) {
    devices.push_back(
        ui::TouchscreenDevice(i, ui::InputDeviceType::INPUT_DEVICE_EXTERNAL, "",
                              gfx::Size(256, 256), 0));
  }

  DisplayInfoList displays;
  displays.push_back(displays_[3]);

  test::ScopedSetInternalDisplayId set_internal(display_manager(),
                                                displays_[0].id());
  AssociateTouchscreens(&displays, devices);

  for (int i = 0; i < 5; ++i)
    EXPECT_EQ(i, displays[0].input_devices()[i]);
}

TEST_F(TouchscreenUtilTest, OneToOneMapping) {
  std::vector<ui::TouchscreenDevice> devices;
  devices.push_back(
      ui::TouchscreenDevice(1, ui::InputDeviceType::INPUT_DEVICE_EXTERNAL, "1",
                            gfx::Size(800, 600), 0));
  devices.push_back(
      ui::TouchscreenDevice(2, ui::InputDeviceType::INPUT_DEVICE_EXTERNAL, "2",
                            gfx::Size(1024, 768), 0));

  test::ScopedSetInternalDisplayId set_internal(display_manager(),
                                                displays_[0].id());
  AssociateTouchscreens(&displays_, devices);

  EXPECT_EQ(0u, displays_[0].input_devices().size());
  EXPECT_EQ(1u, displays_[1].input_devices().size());
  EXPECT_EQ(1, displays_[1].input_devices()[0]);
  EXPECT_EQ(0u, displays_[2].input_devices().size());
  EXPECT_EQ(1u, displays_[3].input_devices().size());
  EXPECT_EQ(2, displays_[3].input_devices()[0]);
}

TEST_F(TouchscreenUtilTest, MapToCorrectDisplaySize) {
  std::vector<ui::TouchscreenDevice> devices;
  devices.push_back(
      ui::TouchscreenDevice(2, ui::InputDeviceType::INPUT_DEVICE_EXTERNAL, "2",
                            gfx::Size(1024, 768), 0));

  test::ScopedSetInternalDisplayId set_internal(display_manager(),
                                                displays_[0].id());
  AssociateTouchscreens(&displays_, devices);

  EXPECT_EQ(0u, displays_[0].input_devices().size());
  EXPECT_EQ(0u, displays_[1].input_devices().size());
  EXPECT_EQ(0u, displays_[2].input_devices().size());
  EXPECT_EQ(1u, displays_[3].input_devices().size());
  EXPECT_EQ(2, displays_[3].input_devices()[0]);
}

TEST_F(TouchscreenUtilTest, MapWhenSizeDiffersByOne) {
  std::vector<ui::TouchscreenDevice> devices;
  devices.push_back(
      ui::TouchscreenDevice(1, ui::InputDeviceType::INPUT_DEVICE_EXTERNAL, "1",
                            gfx::Size(801, 600), 0));
  devices.push_back(
      ui::TouchscreenDevice(2, ui::InputDeviceType::INPUT_DEVICE_EXTERNAL, "2",
                            gfx::Size(1023, 768), 0));

  test::ScopedSetInternalDisplayId set_internal(display_manager(),
                                                displays_[0].id());
  AssociateTouchscreens(&displays_, devices);

  EXPECT_EQ(0u, displays_[0].input_devices().size());
  EXPECT_EQ(1u, displays_[1].input_devices().size());
  EXPECT_EQ(1, displays_[1].input_devices()[0]);
  EXPECT_EQ(0u, displays_[2].input_devices().size());
  EXPECT_EQ(1u, displays_[3].input_devices().size());
  EXPECT_EQ(2, displays_[3].input_devices()[0]);
}

TEST_F(TouchscreenUtilTest, MapWhenSizesDoNotMatch) {
  std::vector<ui::TouchscreenDevice> devices;
  devices.push_back(
      ui::TouchscreenDevice(1, ui::InputDeviceType::INPUT_DEVICE_EXTERNAL, "1",
                            gfx::Size(1022, 768), 0));
  devices.push_back(
      ui::TouchscreenDevice(2, ui::InputDeviceType::INPUT_DEVICE_EXTERNAL, "2",
                            gfx::Size(802, 600), 0));

  DisplayInfoList displays;
  displays.push_back(displays_[0]);
  displays.push_back(displays_[1]);

  test::ScopedSetInternalDisplayId set_internal(display_manager(),
                                                displays[0].id());
  AssociateTouchscreens(&displays, devices);

  EXPECT_EQ(0u, displays[0].input_devices().size());
  EXPECT_EQ(2u, displays[1].input_devices().size());
  EXPECT_EQ(1, displays[1].input_devices()[0]);
  EXPECT_EQ(2, displays[1].input_devices()[1]);
}

TEST_F(TouchscreenUtilTest, MapInternalTouchscreen) {
  std::vector<ui::TouchscreenDevice> devices;
  devices.push_back(
      ui::TouchscreenDevice(1, ui::InputDeviceType::INPUT_DEVICE_EXTERNAL, "1",
                            gfx::Size(1920, 1080), 0));
  devices.push_back(
      ui::TouchscreenDevice(2, ui::InputDeviceType::INPUT_DEVICE_INTERNAL, "2",
                            gfx::Size(9999, 888), 0));

  DisplayInfoList displays;
  displays.push_back(displays_[0]);
  displays.push_back(displays_[1]);

  test::ScopedSetInternalDisplayId set_internal(display_manager(),
                                                displays[0].id());
  AssociateTouchscreens(&displays, devices);

  // Internal touchscreen is always mapped to internal display.
  EXPECT_EQ(1u, displays[0].input_devices().size());
  EXPECT_EQ(2, displays[0].input_devices()[0]);
  EXPECT_EQ(1u, displays[1].input_devices().size());
  EXPECT_EQ(1, displays[1].input_devices()[0]);
}

TEST_F(TouchscreenUtilTest, MultipleInternal) {
  std::vector<ui::TouchscreenDevice> devices;
  devices.push_back(
      ui::TouchscreenDevice(1, ui::InputDeviceType::INPUT_DEVICE_INTERNAL, "1",
                            gfx::Size(1920, 1080), 0));
  devices.push_back(
      ui::TouchscreenDevice(2, ui::InputDeviceType::INPUT_DEVICE_INTERNAL, "2",
                            gfx::Size(1920, 1080), 0));

  test::ScopedSetInternalDisplayId set_internal(display_manager(),
                                                displays_[0].id());
  AssociateTouchscreens(&displays_, devices);

  EXPECT_EQ(2u, displays_[0].input_devices().size());
  EXPECT_EQ(0u, displays_[1].input_devices().size());
  EXPECT_EQ(0u, displays_[2].input_devices().size());
  EXPECT_EQ(0u, displays_[3].input_devices().size());
}

TEST_F(TouchscreenUtilTest, MultipleInternalAndExternal) {
  std::vector<ui::TouchscreenDevice> devices;
  devices.push_back(
      ui::TouchscreenDevice(1, ui::InputDeviceType::INPUT_DEVICE_INTERNAL, "1",
                            gfx::Size(1920, 1080), 0));
  devices.push_back(
      ui::TouchscreenDevice(2, ui::InputDeviceType::INPUT_DEVICE_INTERNAL, "2",
                            gfx::Size(1920, 1080), 0));
  devices.push_back(
      ui::TouchscreenDevice(3, ui::InputDeviceType::INPUT_DEVICE_EXTERNAL, "3",
                            gfx::Size(1024, 768), 0));

  test::ScopedSetInternalDisplayId set_internal(display_manager(),
                                                displays_[0].id());
  AssociateTouchscreens(&displays_, devices);

  EXPECT_EQ(2u, displays_[0].input_devices().size());
  EXPECT_EQ(1, displays_[0].input_devices()[0]);
  EXPECT_EQ(2, displays_[0].input_devices()[1]);
  EXPECT_EQ(0u, displays_[1].input_devices().size());
  EXPECT_EQ(0u, displays_[2].input_devices().size());
  EXPECT_EQ(1u, displays_[3].input_devices().size());
  EXPECT_EQ(3, displays_[3].input_devices()[0]);
}

// crbug.com/515201
TEST_F(TouchscreenUtilTest, TestWithNoInternalDisplay) {
  std::vector<ui::TouchscreenDevice> devices;
  devices.push_back(
      ui::TouchscreenDevice(1, ui::InputDeviceType::INPUT_DEVICE_EXTERNAL, "1",
                            gfx::Size(1920, 1080), 0));
  devices.push_back(
      ui::TouchscreenDevice(2, ui::InputDeviceType::INPUT_DEVICE_INTERNAL, "2",
                            gfx::Size(9999, 888), 0));

  // Internal touchscreen should not be associated with any display
  AssociateTouchscreens(&displays_, devices);

  EXPECT_EQ(1u, displays_[0].input_devices().size());
  EXPECT_EQ(1, displays_[0].input_devices()[0]);
  EXPECT_EQ(0u, displays_[1].input_devices().size());
  EXPECT_EQ(0u, displays_[2].input_devices().size());
  EXPECT_EQ(0u, displays_[3].input_devices().size());
}

}  // namespace display
