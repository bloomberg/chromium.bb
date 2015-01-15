// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/chromeos/test/action_logger_util.h"
#include "ui/display/chromeos/test/test_display_snapshot.h"
#include "ui/display/chromeos/test/test_native_display_delegate.h"
#include "ui/display/chromeos/update_display_configuration_task.h"

namespace ui {
namespace test {

namespace {

class TestDisplayLayoutManager
    : public DisplayConfigurator::DisplayLayoutManager {
 public:
  TestDisplayLayoutManager()
      : should_mirror_(true),
        display_state_(MULTIPLE_DISPLAY_STATE_INVALID),
        power_state_(chromeos::DISPLAY_POWER_ALL_ON) {}
  ~TestDisplayLayoutManager() override {}

  void set_should_mirror(bool should_mirror) { should_mirror_ = should_mirror; }

  void set_display_state(MultipleDisplayState state) { display_state_ = state; }

  void set_power_state(chromeos::DisplayPowerState state) {
    power_state_ = state;
  }

  // DisplayConfigurator::DisplayLayoutManager:
  DisplayConfigurator::SoftwareMirroringController*
  GetSoftwareMirroringController() const override {
    return nullptr;
  }

  DisplayConfigurator::StateController* GetStateController() const override {
    return nullptr;
  }

  MultipleDisplayState GetDisplayState() const override {
    return display_state_;
  }

  chromeos::DisplayPowerState GetPowerState() const override {
    return power_state_;
  }

  std::vector<DisplayConfigurator::DisplayState> ParseDisplays(
      const std::vector<DisplaySnapshot*>& displays) const override {
    std::vector<DisplayConfigurator::DisplayState> parsed_displays;
    for (DisplaySnapshot* display : displays) {
      DisplayConfigurator::DisplayState state;
      state.display = display;
      state.selected_mode = display->native_mode();
      if (should_mirror_)
        state.mirror_mode = FindMirrorMode(displays);

      parsed_displays.push_back(state);
    }

    return parsed_displays;
  }

  bool GetDisplayLayout(
      const std::vector<DisplayConfigurator::DisplayState>& displays,
      MultipleDisplayState new_display_state,
      chromeos::DisplayPowerState new_power_state,
      std::vector<DisplayConfigureRequest>* requests,
      gfx::Size* framebuffer_size) const override {
    gfx::Point origin;
    for (const DisplayConfigurator::DisplayState& state : displays) {
      const DisplayMode* mode = state.selected_mode;
      if (new_display_state == MULTIPLE_DISPLAY_STATE_DUAL_MIRROR)
        mode = state.mirror_mode;

      if (!mode)
        return false;

      if (new_power_state == chromeos::DISPLAY_POWER_ALL_ON) {
        requests->push_back(
            DisplayConfigureRequest(state.display, mode, origin));
      } else {
        requests->push_back(
            DisplayConfigureRequest(state.display, nullptr, origin));
      }

      if (new_display_state != MULTIPLE_DISPLAY_STATE_DUAL_MIRROR) {
        origin.Offset(0, state.selected_mode->size().height());
        framebuffer_size->SetToMax(gfx::Size(mode->size().width(), origin.y()));
      } else {
        *framebuffer_size = mode->size();
      }
    }

    return true;
  }

 private:
  const DisplayMode* FindMirrorMode(
      const std::vector<DisplaySnapshot*>& displays) const {
    const DisplayMode* mode = displays[0]->native_mode();
    for (DisplaySnapshot* display : displays) {
      if (mode->size().GetArea() > display->native_mode()->size().GetArea())
        mode = display->native_mode();
    }

    return mode;
  }

  bool should_mirror_;

  MultipleDisplayState display_state_;

  chromeos::DisplayPowerState power_state_;

  DISALLOW_COPY_AND_ASSIGN(TestDisplayLayoutManager);
};

class UpdateDisplayConfigurationTaskTest : public testing::Test {
 public:
  UpdateDisplayConfigurationTaskTest()
      : delegate_(&log_),
        small_mode_(gfx::Size(1366, 768), false, 60.0f),
        big_mode_(gfx::Size(2560, 1600), false, 60.0f),
        configured_(false),
        configuration_status_(false),
        display_state_(MULTIPLE_DISPLAY_STATE_INVALID),
        power_state_(chromeos::DISPLAY_POWER_ALL_ON) {
    std::vector<const DisplayMode*> modes;
    modes.push_back(&small_mode_);
    displays_[0].set_current_mode(&small_mode_);
    displays_[0].set_native_mode(&small_mode_);
    displays_[0].set_modes(modes);
    displays_[0].set_display_id(123);

    modes.push_back(&big_mode_);
    displays_[1].set_current_mode(&big_mode_);
    displays_[1].set_native_mode(&big_mode_);
    displays_[1].set_modes(modes);
    displays_[1].set_display_id(456);
  }
  ~UpdateDisplayConfigurationTaskTest() override {}

  void UpdateDisplays(size_t count) {
    std::vector<DisplaySnapshot*> displays;
    for (size_t i = 0; i < count; ++i)
      displays.push_back(&displays_[i]);

    delegate_.set_outputs(displays);
  }

  void ResponseCallback(
      bool success,
      const std::vector<DisplayConfigurator::DisplayState>& displays,
      const gfx::Size& framebuffer_size,
      MultipleDisplayState new_display_state,
      chromeos::DisplayPowerState new_power_state) {
    configured_ = true;
    configuration_status_ = success;
    display_states_ = displays;
    display_state_ = new_display_state;
    power_state_ = new_power_state;

    if (success) {
      layout_manager_.set_display_state(display_state_);
      layout_manager_.set_power_state(power_state_);
    }
  }

 protected:
  ActionLogger log_;
  TestNativeDisplayDelegate delegate_;
  TestDisplayLayoutManager layout_manager_;

  const DisplayMode small_mode_;
  const DisplayMode big_mode_;

  TestDisplaySnapshot displays_[2];

  bool configured_;
  bool configuration_status_;
  std::vector<DisplayConfigurator::DisplayState> display_states_;
  MultipleDisplayState display_state_;
  chromeos::DisplayPowerState power_state_;

 private:
  DISALLOW_COPY_AND_ASSIGN(UpdateDisplayConfigurationTaskTest);
};

}  // namespace

TEST_F(UpdateDisplayConfigurationTaskTest, HeadlessConfiguration) {
  {
    UpdateDisplayConfigurationTask task(
        &delegate_, &layout_manager_, MULTIPLE_DISPLAY_STATE_HEADLESS,
        chromeos::DISPLAY_POWER_ALL_ON, 0, 0, false,
        base::Bind(&UpdateDisplayConfigurationTaskTest::ResponseCallback,
                   base::Unretained(this)));
    task.Run();
  }

  EXPECT_TRUE(configured_);
  EXPECT_TRUE(configuration_status_);
  EXPECT_EQ(MULTIPLE_DISPLAY_STATE_HEADLESS, display_state_);
  EXPECT_EQ(chromeos::DISPLAY_POWER_ALL_ON, power_state_);
  EXPECT_EQ(JoinActions(kGrab, kUngrab, NULL), log_.GetActionsAndClear());
}

TEST_F(UpdateDisplayConfigurationTaskTest, SingleConfiguration) {
  UpdateDisplays(1);

  {
    UpdateDisplayConfigurationTask task(
        &delegate_, &layout_manager_, MULTIPLE_DISPLAY_STATE_SINGLE,
        chromeos::DISPLAY_POWER_ALL_ON, 0, 0, false,
        base::Bind(&UpdateDisplayConfigurationTaskTest::ResponseCallback,
                   base::Unretained(this)));
    task.Run();
  }

  EXPECT_TRUE(configured_);
  EXPECT_TRUE(configuration_status_);
  EXPECT_EQ(MULTIPLE_DISPLAY_STATE_SINGLE, display_state_);
  EXPECT_EQ(chromeos::DISPLAY_POWER_ALL_ON, power_state_);
  EXPECT_EQ(JoinActions(
                kGrab, GetFramebufferAction(small_mode_.size(), &displays_[0],
                                            nullptr).c_str(),
                GetCrtcAction(displays_[0], &small_mode_, gfx::Point()).c_str(),
                kUngrab, NULL),
            log_.GetActionsAndClear());
}

TEST_F(UpdateDisplayConfigurationTaskTest, ExtendedConfiguration) {
  UpdateDisplays(2);

  {
    UpdateDisplayConfigurationTask task(
        &delegate_, &layout_manager_, MULTIPLE_DISPLAY_STATE_DUAL_EXTENDED,
        chromeos::DISPLAY_POWER_ALL_ON, 0, 0, false,
        base::Bind(&UpdateDisplayConfigurationTaskTest::ResponseCallback,
                   base::Unretained(this)));
    task.Run();
  }

  EXPECT_TRUE(configured_);
  EXPECT_TRUE(configuration_status_);
  EXPECT_EQ(MULTIPLE_DISPLAY_STATE_DUAL_EXTENDED, display_state_);
  EXPECT_EQ(chromeos::DISPLAY_POWER_ALL_ON, power_state_);
  EXPECT_EQ(
      JoinActions(
          kGrab, GetFramebufferAction(gfx::Size(big_mode_.size().width(),
                                                small_mode_.size().height() +
                                                    big_mode_.size().height()),
                                      &displays_[0], &displays_[1]).c_str(),
          GetCrtcAction(displays_[0], &small_mode_, gfx::Point()).c_str(),
          GetCrtcAction(displays_[1], &big_mode_,
                        gfx::Point(0, small_mode_.size().height())).c_str(),
          kUngrab, NULL),
      log_.GetActionsAndClear());
}

TEST_F(UpdateDisplayConfigurationTaskTest, MirrorConfiguration) {
  UpdateDisplays(2);

  {
    UpdateDisplayConfigurationTask task(
        &delegate_, &layout_manager_, MULTIPLE_DISPLAY_STATE_DUAL_MIRROR,
        chromeos::DISPLAY_POWER_ALL_ON, 0, 0, false,
        base::Bind(&UpdateDisplayConfigurationTaskTest::ResponseCallback,
                   base::Unretained(this)));
    task.Run();
  }

  EXPECT_TRUE(configured_);
  EXPECT_TRUE(configuration_status_);
  EXPECT_EQ(MULTIPLE_DISPLAY_STATE_DUAL_MIRROR, display_state_);
  EXPECT_EQ(chromeos::DISPLAY_POWER_ALL_ON, power_state_);
  EXPECT_EQ(JoinActions(
                kGrab, GetFramebufferAction(small_mode_.size(), &displays_[0],
                                            &displays_[1]).c_str(),
                GetCrtcAction(displays_[0], &small_mode_, gfx::Point()).c_str(),
                GetCrtcAction(displays_[1], &small_mode_, gfx::Point()).c_str(),
                kUngrab, NULL),
            log_.GetActionsAndClear());
}

TEST_F(UpdateDisplayConfigurationTaskTest, FailMirrorConfiguration) {
  layout_manager_.set_should_mirror(false);
  UpdateDisplays(2);

  {
    UpdateDisplayConfigurationTask task(
        &delegate_, &layout_manager_, MULTIPLE_DISPLAY_STATE_DUAL_MIRROR,
        chromeos::DISPLAY_POWER_ALL_ON, 0, 0, false,
        base::Bind(&UpdateDisplayConfigurationTaskTest::ResponseCallback,
                   base::Unretained(this)));
    task.Run();
  }

  EXPECT_TRUE(configured_);
  EXPECT_FALSE(configuration_status_);
  EXPECT_EQ(JoinActions(kGrab, kUngrab, NULL), log_.GetActionsAndClear());
}

TEST_F(UpdateDisplayConfigurationTaskTest, FailExtendedConfiguration) {
  delegate_.set_max_configurable_pixels(1);
  UpdateDisplays(2);

  {
    UpdateDisplayConfigurationTask task(
        &delegate_, &layout_manager_, MULTIPLE_DISPLAY_STATE_DUAL_EXTENDED,
        chromeos::DISPLAY_POWER_ALL_ON, 0, 0, false,
        base::Bind(&UpdateDisplayConfigurationTaskTest::ResponseCallback,
                   base::Unretained(this)));
    task.Run();
  }

  EXPECT_TRUE(configured_);
  EXPECT_FALSE(configuration_status_);
  EXPECT_EQ(
      JoinActions(
          kGrab, GetFramebufferAction(gfx::Size(big_mode_.size().width(),
                                                small_mode_.size().height() +
                                                    big_mode_.size().height()),
                                      &displays_[0], &displays_[1]).c_str(),
          GetCrtcAction(displays_[0], &small_mode_, gfx::Point()).c_str(),
          GetCrtcAction(displays_[1], &big_mode_,
                        gfx::Point(0, small_mode_.size().height())).c_str(),
          GetCrtcAction(displays_[1], &small_mode_,
                        gfx::Point(0, small_mode_.size().height())).c_str(),
          kUngrab, NULL),
      log_.GetActionsAndClear());
}

TEST_F(UpdateDisplayConfigurationTaskTest, SingleChangePowerConfiguration) {
  UpdateDisplays(1);

  {
    UpdateDisplayConfigurationTask task(
        &delegate_, &layout_manager_, MULTIPLE_DISPLAY_STATE_SINGLE,
        chromeos::DISPLAY_POWER_ALL_ON, 0, 0, false,
        base::Bind(&UpdateDisplayConfigurationTaskTest::ResponseCallback,
                   base::Unretained(this)));
    task.Run();
  }

  EXPECT_TRUE(configured_);
  EXPECT_TRUE(configuration_status_);
  EXPECT_EQ(MULTIPLE_DISPLAY_STATE_SINGLE, display_state_);
  EXPECT_EQ(chromeos::DISPLAY_POWER_ALL_ON, power_state_);
  EXPECT_EQ(JoinActions(
                kGrab, GetFramebufferAction(small_mode_.size(), &displays_[0],
                                            nullptr).c_str(),
                GetCrtcAction(displays_[0], &small_mode_, gfx::Point()).c_str(),
                kUngrab, NULL),
            log_.GetActionsAndClear());

  // Turn power off
  {
    UpdateDisplayConfigurationTask task(
        &delegate_, &layout_manager_, MULTIPLE_DISPLAY_STATE_SINGLE,
        chromeos::DISPLAY_POWER_ALL_OFF, 0, 0, false,
        base::Bind(&UpdateDisplayConfigurationTaskTest::ResponseCallback,
                   base::Unretained(this)));
    task.Run();
  }

  EXPECT_TRUE(configuration_status_);
  EXPECT_EQ(MULTIPLE_DISPLAY_STATE_SINGLE, display_state_);
  EXPECT_EQ(chromeos::DISPLAY_POWER_ALL_OFF, power_state_);
  EXPECT_EQ(
      JoinActions(kGrab, GetFramebufferAction(small_mode_.size(), &displays_[0],
                                              nullptr).c_str(),
                  GetCrtcAction(displays_[0], nullptr, gfx::Point()).c_str(),
                  kUngrab, NULL),
      log_.GetActionsAndClear());
}

}  // namespace test
}  // namespace ui
