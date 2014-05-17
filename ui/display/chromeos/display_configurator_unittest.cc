// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/chromeos/display_configurator.h"

#include <stdint.h>

#include <cmath>
#include <cstdarg>
#include <map>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/format_macros.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/chromeos/test/test_display_snapshot.h"
#include "ui/display/types/chromeos/display_mode.h"
#include "ui/display/types/chromeos/native_display_delegate.h"

namespace ui {

namespace {

// Strings returned by TestNativeDisplayDelegate::GetActionsAndClear() to
// describe various actions that were performed.
const char kInitXRandR[] = "init";
const char kGrab[] = "grab";
const char kUngrab[] = "ungrab";
const char kSync[] = "sync";
const char kForceDPMS[] = "dpms";

// String returned by TestNativeDisplayDelegate::GetActionsAndClear() if no
// actions were requested.
const char kNoActions[] = "";

std::string DisplaySnapshotToString(const DisplaySnapshot& output) {
  return base::StringPrintf("id=%" PRId64, output.display_id());
}

// Returns a string describing a TestNativeDisplayDelegate::SetBackgroundColor()
// call.
std::string GetBackgroundAction(uint32_t color_argb) {
  return base::StringPrintf("background(0x%x)", color_argb);
}

// Returns a string describing a TestNativeDisplayDelegate::AddOutputMode()
// call.
std::string GetAddOutputModeAction(const DisplaySnapshot& output,
                                   const DisplayMode* mode) {
  return base::StringPrintf("add_mode(output=%" PRId64 ",mode=%s)",
                            output.display_id(),
                            mode->ToString().c_str());
}

// Returns a string describing a TestNativeDisplayDelegate::Configure()
// call.
std::string GetCrtcAction(const DisplaySnapshot& output,
                          const DisplayMode* mode,
                          const gfx::Point& origin) {
  return base::StringPrintf("crtc(display=[%s],x=%d,y=%d,mode=[%s])",
                            DisplaySnapshotToString(output).c_str(),
                            origin.x(),
                            origin.y(),
                            mode ? mode->ToString().c_str() : "NULL");
}

// Returns a string describing a TestNativeDisplayDelegate::CreateFramebuffer()
// call.
std::string GetFramebufferAction(const gfx::Size& size,
                                 const DisplaySnapshot* out1,
                                 const DisplaySnapshot* out2) {
  return base::StringPrintf(
      "framebuffer(width=%d,height=%d,display1=%s,display2=%s)",
      size.width(),
      size.height(),
      out1 ? DisplaySnapshotToString(*out1).c_str() : "NULL",
      out2 ? DisplaySnapshotToString(*out2).c_str() : "NULL");
}

// Returns a string describing a TestNativeDisplayDelegate::SetHDCPState() call.
std::string GetSetHDCPStateAction(const DisplaySnapshot& output,
                                  HDCPState state) {
  return base::StringPrintf(
      "set_hdcp(id=%" PRId64 ",state=%d)", output.display_id(), state);
}

// Joins a sequence of strings describing actions (e.g. kScreenDim) such
// that they can be compared against a string returned by
// ActionLogger::GetActionsAndClear().  The list of actions must be
// terminated by a NULL pointer.
std::string JoinActions(const char* action, ...) {
  std::string actions;

  va_list arg_list;
  va_start(arg_list, action);
  while (action) {
    if (!actions.empty())
      actions += ",";
    actions += action;
    action = va_arg(arg_list, const char*);
  }
  va_end(arg_list);
  return actions;
}

class ActionLogger {
 public:
  ActionLogger() {}

  void AppendAction(const std::string& action) {
    if (!actions_.empty())
      actions_ += ",";
    actions_ += action;
  }

  // Returns a comma-separated string describing the actions that were
  // requested since the previous call to GetActionsAndClear() (i.e.
  // results are non-repeatable).
  std::string GetActionsAndClear() {
    std::string actions = actions_;
    actions_.clear();
    return actions;
  }

 private:
  std::string actions_;

  DISALLOW_COPY_AND_ASSIGN(ActionLogger);
};

class TestTouchscreenDelegate
    : public DisplayConfigurator::TouchscreenDelegate {
 public:
  // Ownership of |log| remains with the caller.
  explicit TestTouchscreenDelegate(ActionLogger* log)
      : log_(log),
        configure_touchscreens_(false) {}
  virtual ~TestTouchscreenDelegate() {}

  void set_configure_touchscreens(bool state) {
    configure_touchscreens_ = state;
  }

  // DisplayConfigurator::TouchscreenDelegate implementation:
  virtual void AssociateTouchscreens(
      DisplayConfigurator::DisplayStateList* outputs) OVERRIDE {
    if (configure_touchscreens_) {
      for (size_t i = 0; i < outputs->size(); ++i)
        (*outputs)[i].touch_device_id = i + 1;
    }
  }

 private:
  ActionLogger* log_;  // Not owned.

  bool configure_touchscreens_;

  DISALLOW_COPY_AND_ASSIGN(TestTouchscreenDelegate);
};

class TestNativeDisplayDelegate : public NativeDisplayDelegate {
 public:
  // Ownership of |log| remains with the caller.
  explicit TestNativeDisplayDelegate(ActionLogger* log)
      : max_configurable_pixels_(0),
        hdcp_state_(HDCP_STATE_UNDESIRED),
        log_(log) {}
  virtual ~TestNativeDisplayDelegate() {}

  const std::vector<DisplaySnapshot*>& outputs() const { return outputs_; }
  void set_outputs(const std::vector<DisplaySnapshot*>& outputs) {
    outputs_ = outputs;
  }

  void set_max_configurable_pixels(int pixels) {
    max_configurable_pixels_ = pixels;
  }

  void set_hdcp_state(HDCPState state) { hdcp_state_ = state; }

  // DisplayConfigurator::Delegate overrides:
  virtual void Initialize() OVERRIDE { log_->AppendAction(kInitXRandR); }
  virtual void GrabServer() OVERRIDE { log_->AppendAction(kGrab); }
  virtual void UngrabServer() OVERRIDE { log_->AppendAction(kUngrab); }
  virtual void SyncWithServer() OVERRIDE { log_->AppendAction(kSync); }
  virtual void SetBackgroundColor(uint32_t color_argb) OVERRIDE {
    log_->AppendAction(GetBackgroundAction(color_argb));
  }
  virtual void ForceDPMSOn() OVERRIDE { log_->AppendAction(kForceDPMS); }
  virtual std::vector<DisplaySnapshot*> GetDisplays() OVERRIDE {
    return outputs_;
  }
  virtual void AddMode(const DisplaySnapshot& output,
                       const DisplayMode* mode) OVERRIDE {
    log_->AppendAction(GetAddOutputModeAction(output, mode));
  }
  virtual bool Configure(const DisplaySnapshot& output,
                         const DisplayMode* mode,
                         const gfx::Point& origin) OVERRIDE {
    log_->AppendAction(GetCrtcAction(output, mode, origin));

    if (max_configurable_pixels_ == 0)
      return true;

    if (!mode)
      return false;

    return mode->size().GetArea() <= max_configurable_pixels_;
  }
  virtual void CreateFrameBuffer(const gfx::Size& size) OVERRIDE {
    log_->AppendAction(
        GetFramebufferAction(size,
                             outputs_.size() >= 1 ? outputs_[0] : NULL,
                             outputs_.size() >= 2 ? outputs_[1] : NULL));
  }
  virtual bool GetHDCPState(const DisplaySnapshot& output,
                            HDCPState* state) OVERRIDE {
    *state = hdcp_state_;
    return true;
  }

  virtual bool SetHDCPState(const DisplaySnapshot& output,
                            HDCPState state) OVERRIDE {
    log_->AppendAction(GetSetHDCPStateAction(output, state));
    return true;
  }

  virtual std::vector<ui::ColorCalibrationProfile>
  GetAvailableColorCalibrationProfiles(const DisplaySnapshot& output) OVERRIDE {
    return std::vector<ui::ColorCalibrationProfile>();
  }

  virtual bool SetColorCalibrationProfile(
      const DisplaySnapshot& output,
      ui::ColorCalibrationProfile new_profile) OVERRIDE {
    return false;
  }

  virtual void AddObserver(NativeDisplayObserver* observer) OVERRIDE {}

  virtual void RemoveObserver(NativeDisplayObserver* observer) OVERRIDE {}

 private:
  // Outputs to be returned by GetDisplays().
  std::vector<DisplaySnapshot*> outputs_;

  // |max_configurable_pixels_| represents the maximum number of pixels that
  // Configure will support.  Tests can use this to force Configure
  // to fail if attempting to set a resolution that is higher than what
  // a device might support under a given circumstance.
  // A value of 0 means that no limit is enforced and Configure will
  // return success regardless of the resolution.
  int max_configurable_pixels_;

  // Result value of GetHDCPState().
  HDCPState hdcp_state_;

  ActionLogger* log_;  // Not owned.

  DISALLOW_COPY_AND_ASSIGN(TestNativeDisplayDelegate);
};

class TestObserver : public DisplayConfigurator::Observer {
 public:
  explicit TestObserver(DisplayConfigurator* configurator)
      : configurator_(configurator) {
    Reset();
    configurator_->AddObserver(this);
  }
  virtual ~TestObserver() { configurator_->RemoveObserver(this); }

  int num_changes() const { return num_changes_; }
  int num_failures() const { return num_failures_; }
  const DisplayConfigurator::DisplayStateList& latest_outputs() const {
    return latest_outputs_;
  }
  MultipleDisplayState latest_failed_state() const {
    return latest_failed_state_;
  }

  void Reset() {
    num_changes_ = 0;
    num_failures_ = 0;
    latest_outputs_.clear();
    latest_failed_state_ = MULTIPLE_DISPLAY_STATE_INVALID;
  }

  // DisplayConfigurator::Observer overrides:
  virtual void OnDisplayModeChanged(
      const DisplayConfigurator::DisplayStateList& outputs) OVERRIDE {
    num_changes_++;
    latest_outputs_ = outputs;
  }

  virtual void OnDisplayModeChangeFailed(MultipleDisplayState failed_new_state)
      OVERRIDE {
    num_failures_++;
    latest_failed_state_ = failed_new_state;
  }

 private:
  DisplayConfigurator* configurator_;  // Not owned.

  // Number of times that OnDisplayMode*() has been called.
  int num_changes_;
  int num_failures_;

  // Parameters most recently passed to OnDisplayMode*().
  DisplayConfigurator::DisplayStateList latest_outputs_;
  MultipleDisplayState latest_failed_state_;

  DISALLOW_COPY_AND_ASSIGN(TestObserver);
};

class TestStateController : public DisplayConfigurator::StateController {
 public:
  TestStateController() : state_(MULTIPLE_DISPLAY_STATE_DUAL_EXTENDED) {}
  virtual ~TestStateController() {}

  void set_state(MultipleDisplayState state) { state_ = state; }

  // DisplayConfigurator::StateController overrides:
  virtual MultipleDisplayState GetStateForDisplayIds(
      const std::vector<int64_t>& outputs) const OVERRIDE {
    return state_;
  }
  virtual bool GetResolutionForDisplayId(int64_t display_id,
                                         gfx::Size* size) const OVERRIDE {
    return false;
  }

 private:
  MultipleDisplayState state_;

  DISALLOW_COPY_AND_ASSIGN(TestStateController);
};

class TestMirroringController
    : public DisplayConfigurator::SoftwareMirroringController {
 public:
  TestMirroringController() : software_mirroring_enabled_(false) {}
  virtual ~TestMirroringController() {}

  virtual void SetSoftwareMirroring(bool enabled) OVERRIDE {
    software_mirroring_enabled_ = enabled;
  }

  virtual bool SoftwareMirroringEnabled() const OVERRIDE {
    return software_mirroring_enabled_;
  }

 private:
  bool software_mirroring_enabled_;

  DISALLOW_COPY_AND_ASSIGN(TestMirroringController);
};

class DisplayConfiguratorTest : public testing::Test {
 public:
  DisplayConfiguratorTest()
      : small_mode_(gfx::Size(1366, 768), false, 60.0f),
        big_mode_(gfx::Size(2560, 1600), false, 60.0f),
        observer_(&configurator_),
        test_api_(&configurator_) {}
  virtual ~DisplayConfiguratorTest() {}

  virtual void SetUp() OVERRIDE {
    log_.reset(new ActionLogger());

    native_display_delegate_ = new TestNativeDisplayDelegate(log_.get());
    touchscreen_delegate_ = new TestTouchscreenDelegate(log_.get());
    configurator_.SetDelegatesForTesting(
        scoped_ptr<NativeDisplayDelegate>(native_display_delegate_),
        scoped_ptr<DisplayConfigurator::TouchscreenDelegate>(
            touchscreen_delegate_));

    configurator_.set_state_controller(&state_controller_);
    configurator_.set_mirroring_controller(&mirroring_controller_);

    std::vector<const DisplayMode*> modes;
    modes.push_back(&small_mode_);

    TestDisplaySnapshot* o = &outputs_[0];
    o->set_current_mode(&small_mode_);
    o->set_native_mode(&small_mode_);
    o->set_modes(modes);
    o->set_type(DISPLAY_CONNECTION_TYPE_INTERNAL);
    o->set_is_aspect_preserving_scaling(true);
    o->set_display_id(123);
    o->set_has_proper_display_id(true);

    o = &outputs_[1];
    o->set_current_mode(&big_mode_);
    o->set_native_mode(&big_mode_);
    modes.push_back(&big_mode_);
    o->set_modes(modes);
    o->set_type(DISPLAY_CONNECTION_TYPE_HDMI);
    o->set_is_aspect_preserving_scaling(true);
    o->set_display_id(456);
    o->set_has_proper_display_id(true);

    UpdateOutputs(2, false);
  }

  // Predefined modes that can be used by outputs.
  const DisplayMode small_mode_;
  const DisplayMode big_mode_;

 protected:
  // Configures |native_display_delegate_| to return the first |num_outputs|
  // entries from
  // |outputs_|. If |send_events| is true, also sends screen-change and
  // output-change events to |configurator_| and triggers the configure
  // timeout if one was scheduled.
  void UpdateOutputs(size_t num_outputs, bool send_events) {
    ASSERT_LE(num_outputs, arraysize(outputs_));
    std::vector<DisplaySnapshot*> outputs;
    for (size_t i = 0; i < num_outputs; ++i)
      outputs.push_back(&outputs_[i]);
    native_display_delegate_->set_outputs(outputs);

    if (send_events) {
      configurator_.OnConfigurationChanged();
      test_api_.TriggerConfigureTimeout();
    }
  }

  // Initializes |configurator_| with a single internal display.
  void InitWithSingleOutput() {
    UpdateOutputs(1, false);
    EXPECT_EQ(kNoActions, log_->GetActionsAndClear());
    configurator_.Init(false);
    EXPECT_EQ(kNoActions, log_->GetActionsAndClear());
    configurator_.ForceInitialConfigure(0);
    EXPECT_EQ(
        JoinActions(
            kGrab,
            kInitXRandR,
            GetFramebufferAction(small_mode_.size(), &outputs_[0], NULL)
                .c_str(),
            GetCrtcAction(outputs_[0], &small_mode_, gfx::Point(0, 0)).c_str(),
            kForceDPMS,
            kUngrab,
            NULL),
        log_->GetActionsAndClear());
  }

  base::MessageLoop message_loop_;
  TestStateController state_controller_;
  TestMirroringController mirroring_controller_;
  DisplayConfigurator configurator_;
  TestObserver observer_;
  scoped_ptr<ActionLogger> log_;
  TestNativeDisplayDelegate* native_display_delegate_;  // not owned
  TestTouchscreenDelegate* touchscreen_delegate_;       // not owned
  DisplayConfigurator::TestApi test_api_;

  TestDisplaySnapshot outputs_[2];

 private:
  DISALLOW_COPY_AND_ASSIGN(DisplayConfiguratorTest);
};

}  // namespace

TEST_F(DisplayConfiguratorTest, FindDisplayModeMatchingSize) {
  ScopedVector<const DisplayMode> modes;

  // Fields are width, height, interlaced, refresh rate.
  modes.push_back(new DisplayMode(gfx::Size(1920, 1200), false, 60.0));
  // Different rates.
  modes.push_back(new DisplayMode(gfx::Size(1920, 1080), false, 30.0));
  modes.push_back(new DisplayMode(gfx::Size(1920, 1080), false, 50.0));
  modes.push_back(new DisplayMode(gfx::Size(1920, 1080), false, 40.0));
  modes.push_back(new DisplayMode(gfx::Size(1920, 1080), false, 0.0));
  // Interlaced vs non-interlaced.
  modes.push_back(new DisplayMode(gfx::Size(1280, 720), true, 60.0));
  modes.push_back(new DisplayMode(gfx::Size(1280, 720), false, 40.0));
  // Interlaced only.
  modes.push_back(new DisplayMode(gfx::Size(1024, 768), true, 0.0));
  modes.push_back(new DisplayMode(gfx::Size(1024, 768), true, 40.0));
  modes.push_back(new DisplayMode(gfx::Size(1024, 768), true, 60.0));
  // Mixed.
  modes.push_back(new DisplayMode(gfx::Size(1024, 600), true, 60.0));
  modes.push_back(new DisplayMode(gfx::Size(1024, 600), false, 40.0));
  modes.push_back(new DisplayMode(gfx::Size(1024, 600), false, 50.0));
  // Just one interlaced mode.
  modes.push_back(new DisplayMode(gfx::Size(640, 480), true, 60.0));
  // Refresh rate not available.
  modes.push_back(new DisplayMode(gfx::Size(320, 200), false, 0.0));

  TestDisplaySnapshot output;
  output.set_modes(modes.get());

  EXPECT_EQ(modes[0],
            DisplayConfigurator::FindDisplayModeMatchingSize(
                output, gfx::Size(1920, 1200)));

  // Should pick highest refresh rate.
  EXPECT_EQ(modes[2],
            DisplayConfigurator::FindDisplayModeMatchingSize(
                output, gfx::Size(1920, 1080)));

  // Should pick non-interlaced mode.
  EXPECT_EQ(modes[6],
            DisplayConfigurator::FindDisplayModeMatchingSize(
                output, gfx::Size(1280, 720)));

  // Interlaced only. Should pick one with the highest refresh rate in
  // interlaced mode.
  EXPECT_EQ(modes[9],
            DisplayConfigurator::FindDisplayModeMatchingSize(
                output, gfx::Size(1024, 768)));

  // Mixed: Should pick one with the highest refresh rate in
  // interlaced mode.
  EXPECT_EQ(modes[12],
            DisplayConfigurator::FindDisplayModeMatchingSize(
                output, gfx::Size(1024, 600)));

  // Just one interlaced mode.
  EXPECT_EQ(modes[13],
            DisplayConfigurator::FindDisplayModeMatchingSize(
                output, gfx::Size(640, 480)));

  // Refresh rate not available.
  EXPECT_EQ(modes[14],
            DisplayConfigurator::FindDisplayModeMatchingSize(
                output, gfx::Size(320, 200)));

  // No mode found.
  EXPECT_EQ(NULL,
            DisplayConfigurator::FindDisplayModeMatchingSize(
                output, gfx::Size(1440, 900)));
}

TEST_F(DisplayConfiguratorTest, ConnectSecondOutput) {
  InitWithSingleOutput();

  // Connect a second output and check that the configurator enters
  // extended mode.
  observer_.Reset();
  state_controller_.set_state(MULTIPLE_DISPLAY_STATE_DUAL_EXTENDED);
  UpdateOutputs(2, true);
  const int kDualHeight = small_mode_.size().height() +
                          DisplayConfigurator::kVerticalGap +
                          big_mode_.size().height();
  EXPECT_EQ(
      JoinActions(
          kGrab,
          GetFramebufferAction(gfx::Size(big_mode_.size().width(), kDualHeight),
                               &outputs_[0],
                               &outputs_[1]).c_str(),
          GetCrtcAction(outputs_[0], &small_mode_, gfx::Point(0, 0)).c_str(),
          GetCrtcAction(outputs_[1],
                        &big_mode_,
                        gfx::Point(0,
                                   small_mode_.size().height() +
                                       DisplayConfigurator::kVerticalGap))
              .c_str(),
          kUngrab,
          NULL),
      log_->GetActionsAndClear());
  EXPECT_FALSE(mirroring_controller_.SoftwareMirroringEnabled());
  EXPECT_EQ(1, observer_.num_changes());

  observer_.Reset();
  EXPECT_TRUE(configurator_.SetDisplayMode(MULTIPLE_DISPLAY_STATE_DUAL_MIRROR));
  EXPECT_EQ(
      JoinActions(
          kGrab,
          GetFramebufferAction(small_mode_.size(), &outputs_[0], &outputs_[1])
              .c_str(),
          GetCrtcAction(outputs_[0], &small_mode_, gfx::Point(0, 0)).c_str(),
          GetCrtcAction(outputs_[1], &small_mode_, gfx::Point(0, 0)).c_str(),
          kUngrab,
          NULL),
      log_->GetActionsAndClear());
  EXPECT_FALSE(mirroring_controller_.SoftwareMirroringEnabled());
  EXPECT_EQ(1, observer_.num_changes());

  // Disconnect the second output.
  observer_.Reset();
  UpdateOutputs(1, true);
  EXPECT_EQ(
      JoinActions(
          kGrab,
          GetFramebufferAction(small_mode_.size(), &outputs_[0], NULL).c_str(),
          GetCrtcAction(outputs_[0], &small_mode_, gfx::Point(0, 0)).c_str(),
          kUngrab,
          NULL),
      log_->GetActionsAndClear());
  EXPECT_FALSE(mirroring_controller_.SoftwareMirroringEnabled());
  EXPECT_EQ(1, observer_.num_changes());

  // Get rid of shared modes to force software mirroring.
  outputs_[1].set_modes(std::vector<const DisplayMode*>(1, &big_mode_));
  state_controller_.set_state(MULTIPLE_DISPLAY_STATE_DUAL_EXTENDED);
  UpdateOutputs(2, true);
  EXPECT_EQ(
      JoinActions(
          kGrab,
          GetFramebufferAction(gfx::Size(big_mode_.size().width(), kDualHeight),
                               &outputs_[0],
                               &outputs_[1]).c_str(),
          GetCrtcAction(outputs_[0], &small_mode_, gfx::Point(0, 0)).c_str(),
          GetCrtcAction(outputs_[1],
                        &big_mode_,
                        gfx::Point(0,
                                   small_mode_.size().height() +
                                       DisplayConfigurator::kVerticalGap))
              .c_str(),
          kUngrab,
          NULL),
      log_->GetActionsAndClear());
  EXPECT_FALSE(mirroring_controller_.SoftwareMirroringEnabled());

  observer_.Reset();
  EXPECT_TRUE(configurator_.SetDisplayMode(MULTIPLE_DISPLAY_STATE_DUAL_MIRROR));
  EXPECT_EQ(JoinActions(kGrab, kUngrab, NULL), log_->GetActionsAndClear());
  EXPECT_EQ(MULTIPLE_DISPLAY_STATE_DUAL_EXTENDED,
            configurator_.display_state());
  EXPECT_TRUE(mirroring_controller_.SoftwareMirroringEnabled());
  EXPECT_EQ(1, observer_.num_changes());

  // Setting MULTIPLE_DISPLAY_STATE_DUAL_MIRROR should try to reconfigure.
  observer_.Reset();
  EXPECT_TRUE(
      configurator_.SetDisplayMode(MULTIPLE_DISPLAY_STATE_DUAL_EXTENDED));
  EXPECT_EQ(JoinActions(NULL), log_->GetActionsAndClear());
  EXPECT_FALSE(mirroring_controller_.SoftwareMirroringEnabled());
  EXPECT_EQ(1, observer_.num_changes());

  // Set back to software mirror mode.
  observer_.Reset();
  EXPECT_TRUE(configurator_.SetDisplayMode(MULTIPLE_DISPLAY_STATE_DUAL_MIRROR));
  EXPECT_EQ(JoinActions(kGrab, kUngrab, NULL), log_->GetActionsAndClear());
  EXPECT_EQ(MULTIPLE_DISPLAY_STATE_DUAL_EXTENDED,
            configurator_.display_state());
  EXPECT_TRUE(mirroring_controller_.SoftwareMirroringEnabled());
  EXPECT_EQ(1, observer_.num_changes());

  // Disconnect the second output.
  observer_.Reset();
  UpdateOutputs(1, true);
  EXPECT_EQ(
      JoinActions(
          kGrab,
          GetFramebufferAction(small_mode_.size(), &outputs_[0], NULL).c_str(),
          GetCrtcAction(outputs_[0], &small_mode_, gfx::Point(0, 0)).c_str(),
          kUngrab,
          NULL),
      log_->GetActionsAndClear());
  EXPECT_FALSE(mirroring_controller_.SoftwareMirroringEnabled());
  EXPECT_EQ(1, observer_.num_changes());
}

TEST_F(DisplayConfiguratorTest, SetDisplayPower) {
  InitWithSingleOutput();

  state_controller_.set_state(MULTIPLE_DISPLAY_STATE_DUAL_MIRROR);
  observer_.Reset();
  UpdateOutputs(2, true);
  EXPECT_EQ(
      JoinActions(
          kGrab,
          GetFramebufferAction(small_mode_.size(), &outputs_[0], &outputs_[1])
              .c_str(),
          GetCrtcAction(outputs_[0], &small_mode_, gfx::Point(0, 0)).c_str(),
          GetCrtcAction(outputs_[1], &small_mode_, gfx::Point(0, 0)).c_str(),
          kUngrab,
          NULL),
      log_->GetActionsAndClear());
  EXPECT_FALSE(mirroring_controller_.SoftwareMirroringEnabled());
  EXPECT_EQ(1, observer_.num_changes());

  // Turning off the internal display should switch the external display to
  // its native mode.
  observer_.Reset();
  configurator_.SetDisplayPower(
      chromeos::DISPLAY_POWER_INTERNAL_OFF_EXTERNAL_ON,
      DisplayConfigurator::kSetDisplayPowerNoFlags);
  EXPECT_EQ(
      JoinActions(
          kGrab,
          GetFramebufferAction(big_mode_.size(), &outputs_[0], &outputs_[1])
              .c_str(),
          GetCrtcAction(outputs_[0], NULL, gfx::Point(0, 0)).c_str(),
          GetCrtcAction(outputs_[1], &big_mode_, gfx::Point(0, 0)).c_str(),
          kForceDPMS,
          kUngrab,
          NULL),
      log_->GetActionsAndClear());
  EXPECT_EQ(MULTIPLE_DISPLAY_STATE_SINGLE, configurator_.display_state());
  EXPECT_EQ(1, observer_.num_changes());

  // When all displays are turned off, the framebuffer should switch back
  // to the mirrored size.
  observer_.Reset();
  configurator_.SetDisplayPower(chromeos::DISPLAY_POWER_ALL_OFF,
                                DisplayConfigurator::kSetDisplayPowerNoFlags);
  EXPECT_EQ(
      JoinActions(kGrab,
                  GetFramebufferAction(
                      small_mode_.size(), &outputs_[0], &outputs_[1]).c_str(),
                  GetCrtcAction(outputs_[0], NULL, gfx::Point(0, 0)).c_str(),
                  GetCrtcAction(outputs_[1], NULL, gfx::Point(0, 0)).c_str(),
                  kUngrab,
                  NULL),
      log_->GetActionsAndClear());
  EXPECT_EQ(MULTIPLE_DISPLAY_STATE_DUAL_MIRROR, configurator_.display_state());
  EXPECT_FALSE(mirroring_controller_.SoftwareMirroringEnabled());
  EXPECT_EQ(1, observer_.num_changes());

  // Turn all displays on and check that mirroring is still used.
  observer_.Reset();
  configurator_.SetDisplayPower(chromeos::DISPLAY_POWER_ALL_ON,
                                DisplayConfigurator::kSetDisplayPowerNoFlags);
  EXPECT_EQ(
      JoinActions(
          kGrab,
          GetFramebufferAction(small_mode_.size(), &outputs_[0], &outputs_[1])
              .c_str(),
          GetCrtcAction(outputs_[0], &small_mode_, gfx::Point(0, 0)).c_str(),
          GetCrtcAction(outputs_[1], &small_mode_, gfx::Point(0, 0)).c_str(),
          kForceDPMS,
          kUngrab,
          NULL),
      log_->GetActionsAndClear());
  EXPECT_EQ(MULTIPLE_DISPLAY_STATE_DUAL_MIRROR, configurator_.display_state());
  EXPECT_FALSE(mirroring_controller_.SoftwareMirroringEnabled());
  EXPECT_EQ(1, observer_.num_changes());

  // Get rid of shared modes to force software mirroring.
  outputs_[1].set_modes(std::vector<const DisplayMode*>(1, &big_mode_));
  state_controller_.set_state(MULTIPLE_DISPLAY_STATE_DUAL_MIRROR);
  observer_.Reset();
  UpdateOutputs(2, true);
  const int kDualHeight = small_mode_.size().height() +
                          DisplayConfigurator::kVerticalGap +
                          big_mode_.size().height();
  EXPECT_EQ(
      JoinActions(
          kGrab,
          GetFramebufferAction(gfx::Size(big_mode_.size().width(), kDualHeight),
                               &outputs_[0],
                               &outputs_[1]).c_str(),
          GetCrtcAction(outputs_[0], &small_mode_, gfx::Point(0, 0)).c_str(),
          GetCrtcAction(outputs_[1],
                        &big_mode_,
                        gfx::Point(0,
                                   small_mode_.size().height() +
                                       DisplayConfigurator::kVerticalGap))
              .c_str(),
          kUngrab,
          NULL),
      log_->GetActionsAndClear());
  EXPECT_EQ(MULTIPLE_DISPLAY_STATE_DUAL_EXTENDED,
            configurator_.display_state());
  EXPECT_TRUE(mirroring_controller_.SoftwareMirroringEnabled());
  EXPECT_EQ(1, observer_.num_changes());

  // Turning off the internal display should switch the external display to
  // its native mode.
  observer_.Reset();
  configurator_.SetDisplayPower(
      chromeos::DISPLAY_POWER_INTERNAL_OFF_EXTERNAL_ON,
      DisplayConfigurator::kSetDisplayPowerNoFlags);
  EXPECT_EQ(
      JoinActions(
          kGrab,
          GetFramebufferAction(big_mode_.size(), &outputs_[0], &outputs_[1])
              .c_str(),
          GetCrtcAction(outputs_[0], NULL, gfx::Point(0, 0)).c_str(),
          GetCrtcAction(outputs_[1], &big_mode_, gfx::Point(0, 0)).c_str(),
          kForceDPMS,
          kUngrab,
          NULL),
      log_->GetActionsAndClear());
  EXPECT_EQ(MULTIPLE_DISPLAY_STATE_SINGLE, configurator_.display_state());
  EXPECT_FALSE(mirroring_controller_.SoftwareMirroringEnabled());
  EXPECT_EQ(1, observer_.num_changes());

  // When all displays are turned off, the framebuffer should switch back
  // to the extended + software mirroring.
  observer_.Reset();
  configurator_.SetDisplayPower(chromeos::DISPLAY_POWER_ALL_OFF,
                                DisplayConfigurator::kSetDisplayPowerNoFlags);
  EXPECT_EQ(
      JoinActions(
          kGrab,
          GetFramebufferAction(gfx::Size(big_mode_.size().width(), kDualHeight),
                               &outputs_[0],
                               &outputs_[1]).c_str(),
          GetCrtcAction(outputs_[0], NULL, gfx::Point(0, 0)).c_str(),
          GetCrtcAction(outputs_[1],
                        NULL,
                        gfx::Point(0,
                                   small_mode_.size().height() +
                                       DisplayConfigurator::kVerticalGap))
              .c_str(),
          kUngrab,
          NULL),
      log_->GetActionsAndClear());
  EXPECT_EQ(MULTIPLE_DISPLAY_STATE_DUAL_EXTENDED,
            configurator_.display_state());
  EXPECT_TRUE(mirroring_controller_.SoftwareMirroringEnabled());
  EXPECT_EQ(1, observer_.num_changes());

  // Turn all displays on and check that mirroring is still used.
  observer_.Reset();
  configurator_.SetDisplayPower(chromeos::DISPLAY_POWER_ALL_ON,
                                DisplayConfigurator::kSetDisplayPowerNoFlags);
  EXPECT_EQ(
      JoinActions(
          kGrab,
          GetFramebufferAction(gfx::Size(big_mode_.size().width(), kDualHeight),
                               &outputs_[0],
                               &outputs_[1]).c_str(),
          GetCrtcAction(outputs_[0], &small_mode_, gfx::Point(0, 0)).c_str(),
          GetCrtcAction(outputs_[1],
                        &big_mode_,
                        gfx::Point(0,
                                   small_mode_.size().height() +
                                       DisplayConfigurator::kVerticalGap))
              .c_str(),
          kForceDPMS,
          kUngrab,
          NULL),
      log_->GetActionsAndClear());
  EXPECT_EQ(MULTIPLE_DISPLAY_STATE_DUAL_EXTENDED,
            configurator_.display_state());
  EXPECT_TRUE(mirroring_controller_.SoftwareMirroringEnabled());
  EXPECT_EQ(1, observer_.num_changes());
}

TEST_F(DisplayConfiguratorTest, SuspendAndResume) {
  InitWithSingleOutput();

  // No preparation is needed before suspending when the display is already
  // on.  The configurator should still reprobe on resume in case a display
  // was connected while suspended.
  configurator_.SuspendDisplays();
  EXPECT_EQ(kNoActions, log_->GetActionsAndClear());
  configurator_.ResumeDisplays();
  EXPECT_EQ(
      JoinActions(
          kGrab,
          GetFramebufferAction(small_mode_.size(), &outputs_[0], NULL).c_str(),
          GetCrtcAction(outputs_[0], &small_mode_, gfx::Point(0, 0)).c_str(),
          kForceDPMS,
          kUngrab,
          NULL),
      log_->GetActionsAndClear());

  // Now turn the display off before suspending and check that the
  // configurator turns it back on and syncs with the server.
  configurator_.SetDisplayPower(chromeos::DISPLAY_POWER_ALL_OFF,
                                DisplayConfigurator::kSetDisplayPowerNoFlags);
  EXPECT_EQ(
      JoinActions(
          kGrab,
          GetFramebufferAction(small_mode_.size(), &outputs_[0], NULL).c_str(),
          GetCrtcAction(outputs_[0], NULL, gfx::Point(0, 0)).c_str(),
          kUngrab,
          NULL),
      log_->GetActionsAndClear());

  configurator_.SuspendDisplays();
  EXPECT_EQ(
      JoinActions(
          kGrab,
          GetFramebufferAction(small_mode_.size(), &outputs_[0], NULL).c_str(),
          GetCrtcAction(outputs_[0], &small_mode_, gfx::Point(0, 0)).c_str(),
          kForceDPMS,
          kUngrab,
          kSync,
          NULL),
      log_->GetActionsAndClear());

  configurator_.ResumeDisplays();
  EXPECT_EQ(
      JoinActions(
          kGrab,
          GetFramebufferAction(small_mode_.size(), &outputs_[0], NULL).c_str(),
          GetCrtcAction(outputs_[0], &small_mode_, gfx::Point(0, 0)).c_str(),
          kForceDPMS,
          kUngrab,
          NULL),
      log_->GetActionsAndClear());

  // If a second, external display is connected, the displays shouldn't be
  // powered back on before suspending.
  state_controller_.set_state(MULTIPLE_DISPLAY_STATE_DUAL_MIRROR);
  UpdateOutputs(2, true);
  EXPECT_EQ(
      JoinActions(
          kGrab,
          GetFramebufferAction(small_mode_.size(), &outputs_[0], &outputs_[1])
              .c_str(),
          GetCrtcAction(outputs_[0], &small_mode_, gfx::Point(0, 0)).c_str(),
          GetCrtcAction(outputs_[1], &small_mode_, gfx::Point(0, 0)).c_str(),
          kUngrab,
          NULL),
      log_->GetActionsAndClear());

  configurator_.SetDisplayPower(chromeos::DISPLAY_POWER_ALL_OFF,
                                DisplayConfigurator::kSetDisplayPowerNoFlags);
  EXPECT_EQ(
      JoinActions(kGrab,
                  GetFramebufferAction(
                      small_mode_.size(), &outputs_[0], &outputs_[1]).c_str(),
                  GetCrtcAction(outputs_[0], NULL, gfx::Point(0, 0)).c_str(),
                  GetCrtcAction(outputs_[1], NULL, gfx::Point(0, 0)).c_str(),
                  kUngrab,
                  NULL),
      log_->GetActionsAndClear());

  configurator_.SuspendDisplays();
  EXPECT_EQ(JoinActions(kGrab, kUngrab, kSync, NULL),
            log_->GetActionsAndClear());

  // If a display is disconnected while suspended, the configurator should
  // pick up the change.
  UpdateOutputs(1, false);
  configurator_.ResumeDisplays();
  EXPECT_EQ(
      JoinActions(
          kGrab,
          GetFramebufferAction(small_mode_.size(), &outputs_[0], NULL).c_str(),
          GetCrtcAction(outputs_[0], NULL, gfx::Point(0, 0)).c_str(),
          kUngrab,
          NULL),
      log_->GetActionsAndClear());
}

TEST_F(DisplayConfiguratorTest, Headless) {
  UpdateOutputs(0, false);
  EXPECT_EQ(kNoActions, log_->GetActionsAndClear());
  configurator_.Init(false);
  EXPECT_EQ(kNoActions, log_->GetActionsAndClear());
  configurator_.ForceInitialConfigure(0);
  EXPECT_EQ(JoinActions(kGrab, kInitXRandR, kForceDPMS, kUngrab, NULL),
            log_->GetActionsAndClear());

  // Not much should happen when the display power state is changed while
  // no displays are connected.
  configurator_.SetDisplayPower(chromeos::DISPLAY_POWER_ALL_OFF,
                                DisplayConfigurator::kSetDisplayPowerNoFlags);
  EXPECT_EQ(JoinActions(kGrab, kUngrab, NULL), log_->GetActionsAndClear());
  configurator_.SetDisplayPower(chromeos::DISPLAY_POWER_ALL_ON,
                                DisplayConfigurator::kSetDisplayPowerNoFlags);
  EXPECT_EQ(JoinActions(kGrab, kForceDPMS, kUngrab, NULL),
            log_->GetActionsAndClear());

  // Connect an external display and check that it's configured correctly.
  outputs_[0].set_current_mode(outputs_[1].current_mode());
  outputs_[0].set_native_mode(outputs_[1].native_mode());
  outputs_[0].set_modes(outputs_[1].modes());
  outputs_[0].set_type(outputs_[1].type());

  UpdateOutputs(1, true);
  EXPECT_EQ(
      JoinActions(
          kGrab,
          GetFramebufferAction(big_mode_.size(), &outputs_[0], NULL).c_str(),
          GetCrtcAction(outputs_[0], &big_mode_, gfx::Point(0, 0)).c_str(),
          kUngrab,
          NULL),
      log_->GetActionsAndClear());
}

TEST_F(DisplayConfiguratorTest, StartWithTwoOutputs) {
  UpdateOutputs(2, false);
  EXPECT_EQ(kNoActions, log_->GetActionsAndClear());
  configurator_.Init(false);
  EXPECT_EQ(kNoActions, log_->GetActionsAndClear());

  state_controller_.set_state(MULTIPLE_DISPLAY_STATE_DUAL_MIRROR);
  configurator_.ForceInitialConfigure(0);
  EXPECT_EQ(
      JoinActions(
          kGrab,
          kInitXRandR,
          GetFramebufferAction(small_mode_.size(), &outputs_[0], &outputs_[1])
              .c_str(),
          GetCrtcAction(outputs_[0], &small_mode_, gfx::Point(0, 0)).c_str(),
          GetCrtcAction(outputs_[1], &small_mode_, gfx::Point(0, 0)).c_str(),
          kForceDPMS,
          kUngrab,
          NULL),
      log_->GetActionsAndClear());
}

TEST_F(DisplayConfiguratorTest, InvalidMultipleDisplayStates) {
  UpdateOutputs(0, false);
  EXPECT_EQ(kNoActions, log_->GetActionsAndClear());
  configurator_.Init(false);
  configurator_.ForceInitialConfigure(0);
  observer_.Reset();
  EXPECT_TRUE(configurator_.SetDisplayMode(MULTIPLE_DISPLAY_STATE_HEADLESS));
  EXPECT_FALSE(configurator_.SetDisplayMode(MULTIPLE_DISPLAY_STATE_SINGLE));
  EXPECT_FALSE(
      configurator_.SetDisplayMode(MULTIPLE_DISPLAY_STATE_DUAL_MIRROR));
  EXPECT_FALSE(
      configurator_.SetDisplayMode(MULTIPLE_DISPLAY_STATE_DUAL_EXTENDED));
  EXPECT_EQ(1, observer_.num_changes());
  EXPECT_EQ(3, observer_.num_failures());

  UpdateOutputs(1, true);
  observer_.Reset();
  EXPECT_FALSE(configurator_.SetDisplayMode(MULTIPLE_DISPLAY_STATE_HEADLESS));
  EXPECT_TRUE(configurator_.SetDisplayMode(MULTIPLE_DISPLAY_STATE_SINGLE));
  EXPECT_FALSE(
      configurator_.SetDisplayMode(MULTIPLE_DISPLAY_STATE_DUAL_MIRROR));
  EXPECT_FALSE(
      configurator_.SetDisplayMode(MULTIPLE_DISPLAY_STATE_DUAL_EXTENDED));
  EXPECT_EQ(1, observer_.num_changes());
  EXPECT_EQ(3, observer_.num_failures());

  state_controller_.set_state(MULTIPLE_DISPLAY_STATE_DUAL_EXTENDED);
  UpdateOutputs(2, true);
  observer_.Reset();
  EXPECT_FALSE(configurator_.SetDisplayMode(MULTIPLE_DISPLAY_STATE_HEADLESS));
  EXPECT_FALSE(configurator_.SetDisplayMode(MULTIPLE_DISPLAY_STATE_SINGLE));
  EXPECT_TRUE(configurator_.SetDisplayMode(MULTIPLE_DISPLAY_STATE_DUAL_MIRROR));
  EXPECT_TRUE(
      configurator_.SetDisplayMode(MULTIPLE_DISPLAY_STATE_DUAL_EXTENDED));
  EXPECT_EQ(2, observer_.num_changes());
  EXPECT_EQ(2, observer_.num_failures());
}

TEST_F(DisplayConfiguratorTest, GetMultipleDisplayStateForDisplaysWithoutId) {
  outputs_[0].set_has_proper_display_id(false);
  UpdateOutputs(2, false);
  configurator_.Init(false);
  state_controller_.set_state(MULTIPLE_DISPLAY_STATE_DUAL_MIRROR);
  configurator_.ForceInitialConfigure(0);
  EXPECT_EQ(MULTIPLE_DISPLAY_STATE_DUAL_EXTENDED,
            configurator_.display_state());
}

TEST_F(DisplayConfiguratorTest, GetMultipleDisplayStateForDisplaysWithId) {
  outputs_[0].set_has_proper_display_id(true);
  UpdateOutputs(2, false);
  configurator_.Init(false);
  state_controller_.set_state(MULTIPLE_DISPLAY_STATE_DUAL_MIRROR);
  configurator_.ForceInitialConfigure(0);
  EXPECT_EQ(MULTIPLE_DISPLAY_STATE_DUAL_MIRROR, configurator_.display_state());
}

TEST_F(DisplayConfiguratorTest, UpdateCachedOutputsEvenAfterFailure) {
  InitWithSingleOutput();
  const DisplayConfigurator::DisplayStateList* cached =
      &configurator_.cached_displays();
  ASSERT_EQ(static_cast<size_t>(1), cached->size());
  EXPECT_EQ(outputs_[0].current_mode(), (*cached)[0].display->current_mode());

  // After connecting a second output, check that it shows up in
  // |cached_displays_| even if an invalid state is requested.
  state_controller_.set_state(MULTIPLE_DISPLAY_STATE_SINGLE);
  UpdateOutputs(2, true);
  cached = &configurator_.cached_displays();
  ASSERT_EQ(static_cast<size_t>(2), cached->size());
  EXPECT_EQ(outputs_[0].current_mode(), (*cached)[0].display->current_mode());
  EXPECT_EQ(outputs_[1].current_mode(), (*cached)[1].display->current_mode());
}

TEST_F(DisplayConfiguratorTest, PanelFitting) {
  // Configure the internal display to support only the big mode and the
  // external display to support only the small mode.
  outputs_[0].set_current_mode(&big_mode_);
  outputs_[0].set_native_mode(&big_mode_);
  outputs_[0].set_modes(std::vector<const DisplayMode*>(1, &big_mode_));

  outputs_[1].set_current_mode(&small_mode_);
  outputs_[1].set_native_mode(&small_mode_);
  outputs_[1].set_modes(std::vector<const DisplayMode*>(1, &small_mode_));

  // The small mode should be added to the internal output when requesting
  // mirrored mode.
  UpdateOutputs(2, false);
  state_controller_.set_state(MULTIPLE_DISPLAY_STATE_DUAL_MIRROR);
  configurator_.Init(true /* is_panel_fitting_enabled */);
  configurator_.ForceInitialConfigure(0);
  EXPECT_EQ(MULTIPLE_DISPLAY_STATE_DUAL_MIRROR, configurator_.display_state());
  EXPECT_EQ(
      JoinActions(
          kGrab,
          kInitXRandR,
          GetAddOutputModeAction(outputs_[0], &small_mode_).c_str(),
          GetFramebufferAction(small_mode_.size(), &outputs_[0], &outputs_[1])
              .c_str(),
          GetCrtcAction(outputs_[0], &small_mode_, gfx::Point(0, 0)).c_str(),
          GetCrtcAction(outputs_[1], &small_mode_, gfx::Point(0, 0)).c_str(),
          kForceDPMS,
          kUngrab,
          NULL),
      log_->GetActionsAndClear());

  // Both outputs should be using the small mode.
  ASSERT_EQ(1, observer_.num_changes());
  ASSERT_EQ(static_cast<size_t>(2), observer_.latest_outputs().size());
  EXPECT_EQ(&small_mode_, observer_.latest_outputs()[0].mirror_mode);
  EXPECT_EQ(&small_mode_,
            observer_.latest_outputs()[0].display->current_mode());
  EXPECT_EQ(&small_mode_, observer_.latest_outputs()[1].mirror_mode);
  EXPECT_EQ(&small_mode_,
            observer_.latest_outputs()[1].display->current_mode());

  // Also check that the newly-added small mode is present in the internal
  // snapshot that was passed to the observer (http://crbug.com/289159).
  const DisplayConfigurator::DisplayState& state =
      observer_.latest_outputs()[0];
  ASSERT_NE(state.display->modes().end(),
            std::find(state.display->modes().begin(),
                      state.display->modes().end(),
                      &small_mode_));
}

TEST_F(DisplayConfiguratorTest, ContentProtection) {
  configurator_.Init(false);
  configurator_.ForceInitialConfigure(0);
  EXPECT_NE(kNoActions, log_->GetActionsAndClear());

  DisplayConfigurator::ContentProtectionClientId id =
      configurator_.RegisterContentProtectionClient();
  EXPECT_NE(0u, id);

  // One output.
  UpdateOutputs(1, true);
  EXPECT_NE(kNoActions, log_->GetActionsAndClear());
  uint32_t link_mask = 0;
  uint32_t protection_mask = 0;
  EXPECT_TRUE(configurator_.QueryContentProtectionStatus(
      id, outputs_[0].display_id(), &link_mask, &protection_mask));
  EXPECT_EQ(static_cast<uint32_t>(DISPLAY_CONNECTION_TYPE_INTERNAL), link_mask);
  EXPECT_EQ(static_cast<uint32_t>(CONTENT_PROTECTION_METHOD_NONE),
            protection_mask);
  EXPECT_EQ(kNoActions, log_->GetActionsAndClear());

  // Two outputs.
  UpdateOutputs(2, true);
  EXPECT_NE(kNoActions, log_->GetActionsAndClear());
  EXPECT_TRUE(configurator_.QueryContentProtectionStatus(
      id, outputs_[1].display_id(), &link_mask, &protection_mask));
  EXPECT_EQ(static_cast<uint32_t>(DISPLAY_CONNECTION_TYPE_HDMI), link_mask);
  EXPECT_EQ(static_cast<uint32_t>(CONTENT_PROTECTION_METHOD_NONE),
            protection_mask);
  EXPECT_EQ(kNoActions, log_->GetActionsAndClear());

  EXPECT_TRUE(configurator_.EnableContentProtection(
      id, outputs_[1].display_id(), CONTENT_PROTECTION_METHOD_HDCP));
  EXPECT_EQ(GetSetHDCPStateAction(outputs_[1], HDCP_STATE_DESIRED),
            log_->GetActionsAndClear());

  // Enable protection.
  native_display_delegate_->set_hdcp_state(HDCP_STATE_ENABLED);
  EXPECT_TRUE(configurator_.QueryContentProtectionStatus(
      id, outputs_[1].display_id(), &link_mask, &protection_mask));
  EXPECT_EQ(static_cast<uint32_t>(DISPLAY_CONNECTION_TYPE_HDMI), link_mask);
  EXPECT_EQ(static_cast<uint32_t>(CONTENT_PROTECTION_METHOD_HDCP),
            protection_mask);
  EXPECT_EQ(kNoActions, log_->GetActionsAndClear());

  // Protections should be disabled after unregister.
  configurator_.UnregisterContentProtectionClient(id);
  EXPECT_EQ(GetSetHDCPStateAction(outputs_[1], HDCP_STATE_UNDESIRED),
            log_->GetActionsAndClear());
}

TEST_F(DisplayConfiguratorTest, ContentProtectionTwoClients) {
  DisplayConfigurator::ContentProtectionClientId client1 =
      configurator_.RegisterContentProtectionClient();
  DisplayConfigurator::ContentProtectionClientId client2 =
      configurator_.RegisterContentProtectionClient();
  EXPECT_NE(client1, client2);

  configurator_.Init(false);
  configurator_.ForceInitialConfigure(0);
  UpdateOutputs(2, true);
  EXPECT_NE(kNoActions, log_->GetActionsAndClear());

  // Clients never know state enableness for methods that they didn't request.
  EXPECT_TRUE(configurator_.EnableContentProtection(
      client1, outputs_[1].display_id(), CONTENT_PROTECTION_METHOD_HDCP));
  EXPECT_EQ(GetSetHDCPStateAction(outputs_[1], HDCP_STATE_DESIRED).c_str(),
            log_->GetActionsAndClear());
  native_display_delegate_->set_hdcp_state(HDCP_STATE_ENABLED);

  uint32_t link_mask = 0;
  uint32_t protection_mask = 0;
  EXPECT_TRUE(configurator_.QueryContentProtectionStatus(
      client1, outputs_[1].display_id(), &link_mask, &protection_mask));
  EXPECT_EQ(static_cast<uint32_t>(DISPLAY_CONNECTION_TYPE_HDMI), link_mask);
  EXPECT_EQ(CONTENT_PROTECTION_METHOD_HDCP, protection_mask);

  EXPECT_TRUE(configurator_.QueryContentProtectionStatus(
      client2, outputs_[1].display_id(), &link_mask, &protection_mask));
  EXPECT_EQ(static_cast<uint32_t>(DISPLAY_CONNECTION_TYPE_HDMI), link_mask);
  EXPECT_EQ(CONTENT_PROTECTION_METHOD_NONE, protection_mask);

  // Protections will be disabled only if no more clients request them.
  EXPECT_TRUE(configurator_.EnableContentProtection(
      client2, outputs_[1].display_id(), CONTENT_PROTECTION_METHOD_NONE));
  EXPECT_EQ(GetSetHDCPStateAction(outputs_[1], HDCP_STATE_DESIRED).c_str(),
            log_->GetActionsAndClear());
  EXPECT_TRUE(configurator_.EnableContentProtection(
      client1, outputs_[1].display_id(), CONTENT_PROTECTION_METHOD_NONE));
  EXPECT_EQ(GetSetHDCPStateAction(outputs_[1], HDCP_STATE_UNDESIRED).c_str(),
            log_->GetActionsAndClear());
}

TEST_F(DisplayConfiguratorTest, HandleConfigureCrtcFailure) {
  InitWithSingleOutput();

  ScopedVector<const DisplayMode> modes;
  // The first mode is the mode we are requesting DisplayConfigurator to choose.
  // The test will be setup so that this mode will fail and it will have to
  // choose the next best option.
  modes.push_back(new DisplayMode(gfx::Size(2560, 1600), false, 60.0));
  modes.push_back(new DisplayMode(gfx::Size(1024, 768), false, 60.0));
  modes.push_back(new DisplayMode(gfx::Size(1280, 720), false, 60.0));
  modes.push_back(new DisplayMode(gfx::Size(1920, 1080), false, 60.0));
  modes.push_back(new DisplayMode(gfx::Size(1920, 1080), false, 40.0));

  for (unsigned int i = 0; i < arraysize(outputs_); i++) {
    outputs_[i].set_modes(modes.get());
    outputs_[i].set_current_mode(modes[0]);
    outputs_[i].set_native_mode(modes[0]);
  }

  configurator_.Init(false);

  // First test simply fails in MULTIPLE_DISPLAY_STATE_SINGLE mode. This is
  // probably unrealistic but we want to make sure any assumptions don't creep
  // in.
  native_display_delegate_->set_max_configurable_pixels(
      modes[2]->size().GetArea());
  state_controller_.set_state(MULTIPLE_DISPLAY_STATE_SINGLE);
  UpdateOutputs(1, true);

  EXPECT_EQ(
      JoinActions(
          kGrab,
          GetFramebufferAction(big_mode_.size(), &outputs_[0], NULL).c_str(),
          GetCrtcAction(outputs_[0], modes[0], gfx::Point(0, 0)).c_str(),
          GetCrtcAction(outputs_[0], modes[3], gfx::Point(0, 0)).c_str(),
          GetCrtcAction(outputs_[0], modes[2], gfx::Point(0, 0)).c_str(),
          kUngrab,
          NULL),
      log_->GetActionsAndClear());

  // This test should attempt to configure a mirror mode that will not succeed
  // and should end up in extended mode.
  native_display_delegate_->set_max_configurable_pixels(
      modes[3]->size().GetArea());
  state_controller_.set_state(MULTIPLE_DISPLAY_STATE_DUAL_MIRROR);
  UpdateOutputs(2, true);

  EXPECT_EQ(
      JoinActions(
          kGrab,
          GetFramebufferAction(modes[0]->size(), &outputs_[0], &outputs_[1])
              .c_str(),
          GetCrtcAction(outputs_[0], modes[0], gfx::Point(0, 0)).c_str(),
          // First mode tried is expected to fail and it will
          // retry wil the 4th mode in the list.
          GetCrtcAction(outputs_[0], modes[3], gfx::Point(0, 0)).c_str(),
          // Then attempt to configure crtc1 with the first mode.
          GetCrtcAction(outputs_[1], modes[0], gfx::Point(0, 0)).c_str(),
          GetCrtcAction(outputs_[1], modes[3], gfx::Point(0, 0)).c_str(),
          // Since it was requested to go into mirror mode
          // and the configured modes were different, it
          // should now try and setup a valid configurable
          // extended mode.
          GetFramebufferAction(
              gfx::Size(modes[0]->size().width(),
                        modes[0]->size().height() + modes[0]->size().height() +
                            DisplayConfigurator::kVerticalGap),
              &outputs_[0],
              &outputs_[1]).c_str(),
          GetCrtcAction(outputs_[0], modes[0], gfx::Point(0, 0)).c_str(),
          GetCrtcAction(outputs_[0], modes[3], gfx::Point(0, 0)).c_str(),
          GetCrtcAction(outputs_[1],
                        modes[0],
                        gfx::Point(0,
                                   modes[0]->size().height() +
                                       DisplayConfigurator::kVerticalGap))
              .c_str(),
          GetCrtcAction(outputs_[1],
                        modes[3],
                        gfx::Point(0,
                                   modes[0]->size().height() +
                                       DisplayConfigurator::kVerticalGap))
              .c_str(),
          kUngrab,
          NULL),
      log_->GetActionsAndClear());
}

}  // namespace ui
