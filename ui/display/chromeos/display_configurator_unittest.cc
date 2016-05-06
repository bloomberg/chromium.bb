// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/chromeos/display_configurator.h"

#include <stddef.h>
#include <stdint.h>

#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop/message_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/chromeos/test/action_logger_util.h"
#include "ui/display/chromeos/test/test_display_snapshot.h"
#include "ui/display/chromeos/test/test_native_display_delegate.h"
#include "ui/display/util/display_util.h"

namespace ui {
namespace test {

namespace {

class TestObserver : public DisplayConfigurator::Observer {
 public:
  explicit TestObserver(DisplayConfigurator* configurator)
      : configurator_(configurator) {
    Reset();
    configurator_->AddObserver(this);
  }
  ~TestObserver() override { configurator_->RemoveObserver(this); }

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
  void OnDisplayModeChanged(
      const DisplayConfigurator::DisplayStateList& outputs) override {
    num_changes_++;
    latest_outputs_ = outputs;
  }

  void OnDisplayModeChangeFailed(
      const DisplayConfigurator::DisplayStateList& outputs,
      MultipleDisplayState failed_new_state) override {
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
  ~TestStateController() override {}

  void set_state(MultipleDisplayState state) { state_ = state; }

  // DisplayConfigurator::StateController overrides:
  MultipleDisplayState GetStateForDisplayIds(
      const DisplayConfigurator::DisplayStateList& outputs) const override {
    return state_;
  }
  bool GetResolutionForDisplayId(int64_t display_id,
                                 gfx::Size* size) const override {
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
  ~TestMirroringController() override {}

  void SetSoftwareMirroring(bool enabled) override {
    software_mirroring_enabled_ = enabled;
  }

  bool SoftwareMirroringEnabled() const override {
    return software_mirroring_enabled_;
  }

 private:
  bool software_mirroring_enabled_;

  DISALLOW_COPY_AND_ASSIGN(TestMirroringController);
};

class DisplayConfiguratorTest : public testing::Test {
 public:
  enum CallbackResult {
    CALLBACK_FAILURE,
    CALLBACK_SUCCESS,
    CALLBACK_NOT_CALLED,
  };

  DisplayConfiguratorTest()
      : small_mode_(gfx::Size(1366, 768), false, 60.0f),
        big_mode_(gfx::Size(2560, 1600), false, 60.0f),
        observer_(&configurator_),
        test_api_(&configurator_),
        enable_content_protection_status_(0),
        enable_content_protection_call_count_(0),
        query_content_protection_call_count_(0),
        callback_result_(CALLBACK_NOT_CALLED),
        display_control_result_(CALLBACK_NOT_CALLED) {}
  ~DisplayConfiguratorTest() override {}

  void SetUp() override {
    log_.reset(new ActionLogger());

    native_display_delegate_ = new TestNativeDisplayDelegate(log_.get());
    configurator_.SetDelegateForTesting(
        std::unique_ptr<NativeDisplayDelegate>(native_display_delegate_));

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

    o = &outputs_[1];
    o->set_current_mode(&big_mode_);
    o->set_native_mode(&big_mode_);
    modes.push_back(&big_mode_);
    o->set_modes(modes);
    o->set_type(DISPLAY_CONNECTION_TYPE_HDMI);
    o->set_is_aspect_preserving_scaling(true);
    o->set_display_id(456);

    UpdateOutputs(2, false);
  }

  void OnConfiguredCallback(bool status) {
    callback_result_ = (status ? CALLBACK_SUCCESS : CALLBACK_FAILURE);
  }

  void OnDisplayControlUpdated(bool status) {
    display_control_result_ = (status ? CALLBACK_SUCCESS : CALLBACK_FAILURE);
  }

  void EnableContentProtectionCallback(bool status) {
    enable_content_protection_status_ = status;
    enable_content_protection_call_count_++;
  }

  void QueryContentProtectionCallback(
      const DisplayConfigurator::QueryProtectionResponse& response) {
    query_content_protection_response_ = response;
    query_content_protection_call_count_++;
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
      EXPECT_TRUE(test_api_.TriggerConfigureTimeout());
    }
  }

  void Init(bool panel_fitting_enabled) {
    configurator_.Init(std::unique_ptr<NativeDisplayDelegate>(),
                       panel_fitting_enabled);
  }

  // Initializes |configurator_| with a single internal display.
  void InitWithSingleOutput() {
    UpdateOutputs(1, false);
    EXPECT_EQ(kNoActions, log_->GetActionsAndClear());
    configurator_.Init(std::unique_ptr<NativeDisplayDelegate>(), false);

    EXPECT_EQ(kNoActions, log_->GetActionsAndClear());
    configurator_.ForceInitialConfigure(0);
    EXPECT_EQ(JoinActions(kInitXRandR, kGrab,
                          GetFramebufferAction(small_mode_.size(), &outputs_[0],
                                               NULL).c_str(),
                          GetCrtcAction(outputs_[0], &small_mode_,
                                        gfx::Point(0, 0)).c_str(),
                          kForceDPMS, kUngrab, NULL),
              log_->GetActionsAndClear());
  }

  CallbackResult PopCallbackResult() {
    CallbackResult result = callback_result_;
    callback_result_ = CALLBACK_NOT_CALLED;
    return result;
  }

  CallbackResult PopDisplayControlResult() {
    CallbackResult result = display_control_result_;
    display_control_result_ = CALLBACK_NOT_CALLED;
    return result;
  }

  base::MessageLoop message_loop_;
  TestStateController state_controller_;
  TestMirroringController mirroring_controller_;
  DisplayConfigurator configurator_;
  TestObserver observer_;
  std::unique_ptr<ActionLogger> log_;
  TestNativeDisplayDelegate* native_display_delegate_;  // not owned
  DisplayConfigurator::TestApi test_api_;

  bool enable_content_protection_status_;
  int enable_content_protection_call_count_;
  DisplayConfigurator::QueryProtectionResponse
      query_content_protection_response_;
  int query_content_protection_call_count_;

  TestDisplaySnapshot outputs_[2];

  CallbackResult callback_result_;
  CallbackResult display_control_result_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DisplayConfiguratorTest);
};

}  // namespace

TEST_F(DisplayConfiguratorTest, FindDisplayModeMatchingSize) {
  ScopedVector<const DisplayMode> modes;

  // Fields are width, height, interlaced, refresh rate.
  modes.push_back(new DisplayMode(gfx::Size(1920, 1200), false, 60.0));
  DisplayMode* native_mode =
      new DisplayMode(gfx::Size(1920, 1200), false, 50.0);
  modes.push_back(native_mode);
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
  output.set_native_mode(native_mode);

  // Should pick native over highest refresh rate.
  EXPECT_EQ(modes[1],
            DisplayConfigurator::FindDisplayModeMatchingSize(
                output, gfx::Size(1920, 1200)));

  // Should pick highest refresh rate.
  EXPECT_EQ(modes[3],
            DisplayConfigurator::FindDisplayModeMatchingSize(
                output, gfx::Size(1920, 1080)));

  // Should pick non-interlaced mode.
  EXPECT_EQ(modes[7],
            DisplayConfigurator::FindDisplayModeMatchingSize(
                output, gfx::Size(1280, 720)));

  // Interlaced only. Should pick one with the highest refresh rate in
  // interlaced mode.
  EXPECT_EQ(modes[10],
            DisplayConfigurator::FindDisplayModeMatchingSize(
                output, gfx::Size(1024, 768)));

  // Mixed: Should pick one with the highest refresh rate in
  // interlaced mode.
  EXPECT_EQ(modes[13],
            DisplayConfigurator::FindDisplayModeMatchingSize(
                output, gfx::Size(1024, 600)));

  // Just one interlaced mode.
  EXPECT_EQ(modes[14],
            DisplayConfigurator::FindDisplayModeMatchingSize(
                output, gfx::Size(640, 480)));

  // Refresh rate not available.
  EXPECT_EQ(modes[15],
            DisplayConfigurator::FindDisplayModeMatchingSize(
                output, gfx::Size(320, 200)));

  // No mode found.
  EXPECT_EQ(NULL,
            DisplayConfigurator::FindDisplayModeMatchingSize(
                output, gfx::Size(1440, 900)));
}

TEST_F(DisplayConfiguratorTest, EnableVirtualDisplay) {
  InitWithSingleOutput();

  observer_.Reset();
  const DisplayConfigurator::DisplayStateList& cached =
      configurator_.cached_displays();
  ASSERT_EQ(static_cast<size_t>(1u), cached.size());
  EXPECT_EQ(small_mode_.size(), cached[0]->current_mode()->size());

  // Add virtual display.
  int64_t virtual_display_id =
      configurator_.AddVirtualDisplay(big_mode_.size());
  EXPECT_EQ(GenerateDisplayID(0x8000, 0x0, 1), virtual_display_id);
  EXPECT_FALSE(mirroring_controller_.SoftwareMirroringEnabled());
  EXPECT_EQ(MULTIPLE_DISPLAY_STATE_DUAL_EXTENDED,
            configurator_.display_state());

  // Virtual should not trigger addition of added crtc but does change FB
  // height.
  const int kVirtualHeight = small_mode_.size().height() +
                             DisplayConfigurator::kVerticalGap +
                             big_mode_.size().height();
  EXPECT_EQ(
      JoinActions(
          kGrab, GetFramebufferAction(
                     gfx::Size(big_mode_.size().width(), kVirtualHeight),
                     &outputs_[0], nullptr)
                     .c_str(),
          GetCrtcAction(outputs_[0], &small_mode_, gfx::Point(0, 0)).c_str(),
          kUngrab, nullptr),
      log_->GetActionsAndClear());
  EXPECT_EQ(1, observer_.num_changes());
  ASSERT_EQ(static_cast<size_t>(2u), cached.size());
  EXPECT_EQ(small_mode_.size(), cached[0]->current_mode()->size());
  EXPECT_EQ(big_mode_.size(), cached[1]->current_mode()->size());
  EXPECT_EQ(virtual_display_id, cached[1]->display_id());

  // Remove virtual display.
  observer_.Reset();
  EXPECT_TRUE(configurator_.RemoveVirtualDisplay(virtual_display_id));
  EXPECT_EQ(
      JoinActions(
          kGrab, GetFramebufferAction(small_mode_.size(), &outputs_[0], nullptr)
                     .c_str(),
          GetCrtcAction(outputs_[0], &small_mode_, gfx::Point(0, 0)).c_str(),
          kUngrab, nullptr),
      log_->GetActionsAndClear());
  EXPECT_EQ(1, observer_.num_changes());
  ASSERT_EQ(static_cast<size_t>(1u), cached.size());
  EXPECT_EQ(small_mode_.size(), cached[0]->current_mode()->size());
  EXPECT_EQ(MULTIPLE_DISPLAY_STATE_SINGLE, configurator_.display_state());
}

TEST_F(DisplayConfiguratorTest, EnableTwoVirtualDisplays) {
  InitWithSingleOutput();

  observer_.Reset();
  const DisplayConfigurator::DisplayStateList& cached =
      configurator_.cached_displays();
  ASSERT_EQ(static_cast<size_t>(1u), cached.size());
  EXPECT_EQ(small_mode_.size(), cached[0]->current_mode()->size());

  // Add 1st virtual display.
  int64_t virtual_display_id_1 =
      configurator_.AddVirtualDisplay(big_mode_.size());
  EXPECT_EQ(GenerateDisplayID(0x8000, 0x0, 1), virtual_display_id_1);
  EXPECT_FALSE(mirroring_controller_.SoftwareMirroringEnabled());
  EXPECT_EQ(MULTIPLE_DISPLAY_STATE_DUAL_EXTENDED,
            configurator_.display_state());

  // Virtual should not trigger addition of added crtc but does change FB
  // height.
  const int kSingleVirtualHeight = small_mode_.size().height() +
                                   DisplayConfigurator::kVerticalGap +
                                   big_mode_.size().height();
  EXPECT_EQ(
      JoinActions(
          kGrab, GetFramebufferAction(
                     gfx::Size(big_mode_.size().width(), kSingleVirtualHeight),
                     &outputs_[0], nullptr)
                     .c_str(),
          GetCrtcAction(outputs_[0], &small_mode_, gfx::Point(0, 0)).c_str(),
          kUngrab, nullptr),
      log_->GetActionsAndClear());
  EXPECT_EQ(1, observer_.num_changes());
  ASSERT_EQ(static_cast<size_t>(2), cached.size());
  EXPECT_EQ(small_mode_.size(), cached[0]->current_mode()->size());
  EXPECT_EQ(big_mode_.size(), cached[1]->current_mode()->size());
  EXPECT_EQ(virtual_display_id_1, cached[1]->display_id());

  // Add 2nd virtual display
  int64_t virtual_display_id_2 =
      configurator_.AddVirtualDisplay(big_mode_.size());
  EXPECT_EQ(GenerateDisplayID(0x8000, 0x0, 2), virtual_display_id_2);
  EXPECT_FALSE(mirroring_controller_.SoftwareMirroringEnabled());
  EXPECT_EQ(MULTIPLE_DISPLAY_STATE_MULTI_EXTENDED,
            configurator_.display_state());

  const int kDualVirtualHeight =
      small_mode_.size().height() +
      (DisplayConfigurator::kVerticalGap + big_mode_.size().height()) * 2;
  EXPECT_EQ(
      JoinActions(
          kGrab, GetFramebufferAction(
                     gfx::Size(big_mode_.size().width(), kDualVirtualHeight),
                     &outputs_[0], nullptr)
                     .c_str(),
          GetCrtcAction(outputs_[0], &small_mode_, gfx::Point(0, 0)).c_str(),
          kUngrab, nullptr),
      log_->GetActionsAndClear());
  EXPECT_EQ(2, observer_.num_changes());
  ASSERT_EQ(static_cast<size_t>(3u), cached.size());
  EXPECT_EQ(small_mode_.size(), cached[0]->current_mode()->size());
  EXPECT_EQ(big_mode_.size(), cached[1]->current_mode()->size());
  EXPECT_EQ(big_mode_.size(), cached[2]->current_mode()->size());
  EXPECT_EQ(virtual_display_id_1, cached[1]->display_id());
  EXPECT_EQ(virtual_display_id_2, cached[2]->display_id());

  // Remove 1st virtual display.
  observer_.Reset();
  EXPECT_TRUE(configurator_.RemoveVirtualDisplay(virtual_display_id_1));
  EXPECT_EQ(
      JoinActions(
          kGrab, GetFramebufferAction(
                     gfx::Size(big_mode_.size().width(), kSingleVirtualHeight),
                     &outputs_[0], nullptr)
                     .c_str(),
          GetCrtcAction(outputs_[0], &small_mode_, gfx::Point(0, 0)).c_str(),
          kUngrab, nullptr),
      log_->GetActionsAndClear());
  EXPECT_EQ(1, observer_.num_changes());
  ASSERT_EQ(static_cast<size_t>(2u), cached.size());
  EXPECT_EQ(small_mode_.size(), cached[0]->current_mode()->size());
  EXPECT_EQ(big_mode_.size(), cached[1]->current_mode()->size());
  EXPECT_EQ(virtual_display_id_2, cached[1]->display_id());
  EXPECT_EQ(MULTIPLE_DISPLAY_STATE_DUAL_EXTENDED,
            configurator_.display_state());

  // Remove 2nd virtual display.
  observer_.Reset();
  EXPECT_TRUE(configurator_.RemoveVirtualDisplay(virtual_display_id_2));
  EXPECT_EQ(
      JoinActions(
          kGrab, GetFramebufferAction(small_mode_.size(), &outputs_[0], nullptr)
                     .c_str(),
          GetCrtcAction(outputs_[0], &small_mode_, gfx::Point(0, 0)).c_str(),
          kUngrab, nullptr),
      log_->GetActionsAndClear());
  EXPECT_EQ(1, observer_.num_changes());
  ASSERT_EQ(static_cast<size_t>(1), cached.size());
  EXPECT_EQ(small_mode_.size(), cached[0]->current_mode()->size());
  EXPECT_EQ(MULTIPLE_DISPLAY_STATE_SINGLE, configurator_.display_state());
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
  configurator_.SetDisplayMode(MULTIPLE_DISPLAY_STATE_DUAL_MIRROR);
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
  const gfx::Size framebuffer_size = configurator_.framebuffer_size();
  DCHECK(!framebuffer_size.IsEmpty());

  observer_.Reset();
  configurator_.SetDisplayMode(MULTIPLE_DISPLAY_STATE_DUAL_MIRROR);
  EXPECT_EQ(JoinActions(kGrab, kUngrab, NULL), log_->GetActionsAndClear());
  EXPECT_EQ(MULTIPLE_DISPLAY_STATE_DUAL_EXTENDED,
            configurator_.display_state());
  EXPECT_TRUE(mirroring_controller_.SoftwareMirroringEnabled());
  EXPECT_EQ(framebuffer_size.ToString(),
            configurator_.framebuffer_size().ToString());

  EXPECT_EQ(1, observer_.num_changes());

  // Setting MULTIPLE_DISPLAY_STATE_DUAL_MIRROR should try to reconfigure.
  observer_.Reset();
  configurator_.SetDisplayMode(MULTIPLE_DISPLAY_STATE_DUAL_EXTENDED);
  EXPECT_EQ(JoinActions(NULL), log_->GetActionsAndClear());
  EXPECT_FALSE(mirroring_controller_.SoftwareMirroringEnabled());
  EXPECT_EQ(1, observer_.num_changes());

  // Set back to software mirror mode.
  observer_.Reset();
  configurator_.SetDisplayMode(MULTIPLE_DISPLAY_STATE_DUAL_MIRROR);
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
      DisplayConfigurator::kSetDisplayPowerNoFlags,
      base::Bind(&DisplayConfiguratorTest::OnConfiguredCallback,
                 base::Unretained(this)));
  EXPECT_EQ(CALLBACK_SUCCESS, PopCallbackResult());
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
  configurator_.SetDisplayPower(
      chromeos::DISPLAY_POWER_ALL_OFF,
      DisplayConfigurator::kSetDisplayPowerNoFlags,
      base::Bind(&DisplayConfiguratorTest::OnConfiguredCallback,
                 base::Unretained(this)));
  EXPECT_EQ(CALLBACK_SUCCESS, PopCallbackResult());
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
  configurator_.SetDisplayPower(
      chromeos::DISPLAY_POWER_ALL_ON,
      DisplayConfigurator::kSetDisplayPowerNoFlags,
      base::Bind(&DisplayConfiguratorTest::OnConfiguredCallback,
                 base::Unretained(this)));
  EXPECT_EQ(CALLBACK_SUCCESS, PopCallbackResult());
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
      DisplayConfigurator::kSetDisplayPowerNoFlags,
      base::Bind(&DisplayConfiguratorTest::OnConfiguredCallback,
                 base::Unretained(this)));
  EXPECT_EQ(CALLBACK_SUCCESS, PopCallbackResult());
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
  configurator_.SetDisplayPower(
      chromeos::DISPLAY_POWER_ALL_OFF,
      DisplayConfigurator::kSetDisplayPowerNoFlags,
      base::Bind(&DisplayConfiguratorTest::OnConfiguredCallback,
                 base::Unretained(this)));
  EXPECT_EQ(CALLBACK_SUCCESS, PopCallbackResult());
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
  configurator_.SetDisplayPower(
      chromeos::DISPLAY_POWER_ALL_ON,
      DisplayConfigurator::kSetDisplayPowerNoFlags,
      base::Bind(&DisplayConfiguratorTest::OnConfiguredCallback,
                 base::Unretained(this)));
  EXPECT_EQ(CALLBACK_SUCCESS, PopCallbackResult());
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
  const gfx::Size framebuffer_size = configurator_.framebuffer_size();
  DCHECK(!framebuffer_size.IsEmpty());
  configurator_.SuspendDisplays(base::Bind(
      &DisplayConfiguratorTest::OnConfiguredCallback, base::Unretained(this)));
  EXPECT_EQ(CALLBACK_SUCCESS, PopCallbackResult());
  EXPECT_EQ(framebuffer_size.ToString(),
            configurator_.framebuffer_size().ToString());
  EXPECT_EQ(kNoActions, log_->GetActionsAndClear());
  configurator_.ResumeDisplays();
  EXPECT_TRUE(test_api_.TriggerConfigureTimeout());
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
  configurator_.SetDisplayPower(
      chromeos::DISPLAY_POWER_ALL_OFF,
      DisplayConfigurator::kSetDisplayPowerNoFlags,
      base::Bind(&DisplayConfiguratorTest::OnConfiguredCallback,
                 base::Unretained(this)));
  EXPECT_EQ(CALLBACK_SUCCESS, PopCallbackResult());
  EXPECT_EQ(
      JoinActions(
          kGrab,
          GetFramebufferAction(small_mode_.size(), &outputs_[0], NULL).c_str(),
          GetCrtcAction(outputs_[0], NULL, gfx::Point(0, 0)).c_str(),
          kUngrab,
          NULL),
      log_->GetActionsAndClear());

  configurator_.SuspendDisplays(base::Bind(
      &DisplayConfiguratorTest::OnConfiguredCallback, base::Unretained(this)));
  EXPECT_EQ(CALLBACK_SUCCESS, PopCallbackResult());
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
  EXPECT_TRUE(test_api_.TriggerConfigureTimeout());
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

  configurator_.SetDisplayPower(
      chromeos::DISPLAY_POWER_ALL_OFF,
      DisplayConfigurator::kSetDisplayPowerNoFlags,
      base::Bind(&DisplayConfiguratorTest::OnConfiguredCallback,
                 base::Unretained(this)));
  EXPECT_EQ(CALLBACK_SUCCESS, PopCallbackResult());
  EXPECT_EQ(
      JoinActions(kGrab,
                  GetFramebufferAction(
                      small_mode_.size(), &outputs_[0], &outputs_[1]).c_str(),
                  GetCrtcAction(outputs_[0], NULL, gfx::Point(0, 0)).c_str(),
                  GetCrtcAction(outputs_[1], NULL, gfx::Point(0, 0)).c_str(),
                  kUngrab,
                  NULL),
      log_->GetActionsAndClear());

  configurator_.SuspendDisplays(base::Bind(
      &DisplayConfiguratorTest::OnConfiguredCallback, base::Unretained(this)));
  EXPECT_EQ(CALLBACK_SUCCESS, PopCallbackResult());
  EXPECT_EQ(JoinActions(kGrab, kUngrab, kSync, NULL),
            log_->GetActionsAndClear());

  // If a display is disconnected while suspended, the configurator should
  // pick up the change.
  UpdateOutputs(1, false);
  configurator_.ResumeDisplays();
  EXPECT_TRUE(test_api_.TriggerConfigureTimeout());
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
  Init(false);
  EXPECT_EQ(kNoActions, log_->GetActionsAndClear());
  configurator_.ForceInitialConfigure(0);
  EXPECT_EQ(JoinActions(kInitXRandR, kGrab, kForceDPMS, kUngrab, NULL),
            log_->GetActionsAndClear());

  // Not much should happen when the display power state is changed while
  // no displays are connected.
  configurator_.SetDisplayPower(
      chromeos::DISPLAY_POWER_ALL_OFF,
      DisplayConfigurator::kSetDisplayPowerNoFlags,
      base::Bind(&DisplayConfiguratorTest::OnConfiguredCallback,
                 base::Unretained(this)));
  EXPECT_EQ(CALLBACK_SUCCESS, PopCallbackResult());
  EXPECT_EQ(JoinActions(kGrab, kUngrab, NULL), log_->GetActionsAndClear());
  configurator_.SetDisplayPower(
      chromeos::DISPLAY_POWER_ALL_ON,
      DisplayConfigurator::kSetDisplayPowerNoFlags,
      base::Bind(&DisplayConfiguratorTest::OnConfiguredCallback,
                 base::Unretained(this)));
  EXPECT_EQ(CALLBACK_SUCCESS, PopCallbackResult());
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
  const gfx::Size framebuffer_size = configurator_.framebuffer_size();
  DCHECK(!framebuffer_size.IsEmpty());

  UpdateOutputs(0, true);
  EXPECT_EQ(JoinActions(kGrab, kUngrab, NULL), log_->GetActionsAndClear());
  EXPECT_EQ(framebuffer_size.ToString(),
            configurator_.framebuffer_size().ToString());
}

TEST_F(DisplayConfiguratorTest, StartWithTwoOutputs) {
  UpdateOutputs(2, false);
  EXPECT_EQ(kNoActions, log_->GetActionsAndClear());
  Init(false);
  EXPECT_EQ(kNoActions, log_->GetActionsAndClear());

  state_controller_.set_state(MULTIPLE_DISPLAY_STATE_DUAL_MIRROR);
  configurator_.ForceInitialConfigure(0);
  EXPECT_EQ(
      JoinActions(
          kInitXRandR, kGrab,
          GetFramebufferAction(small_mode_.size(), &outputs_[0], &outputs_[1])
              .c_str(),
          GetCrtcAction(outputs_[0], &small_mode_, gfx::Point(0, 0)).c_str(),
          GetCrtcAction(outputs_[1], &small_mode_, gfx::Point(0, 0)).c_str(),
          kForceDPMS, kUngrab, NULL),
      log_->GetActionsAndClear());
}

TEST_F(DisplayConfiguratorTest, InvalidMultipleDisplayStates) {
  UpdateOutputs(0, false);
  EXPECT_EQ(kNoActions, log_->GetActionsAndClear());
  Init(false);
  configurator_.ForceInitialConfigure(0);
  observer_.Reset();
  configurator_.SetDisplayMode(MULTIPLE_DISPLAY_STATE_HEADLESS);
  EXPECT_EQ(1, observer_.num_changes());
  EXPECT_EQ(0, observer_.num_failures());
  configurator_.SetDisplayMode(MULTIPLE_DISPLAY_STATE_SINGLE);
  configurator_.SetDisplayMode(MULTIPLE_DISPLAY_STATE_DUAL_MIRROR);
  configurator_.SetDisplayMode(MULTIPLE_DISPLAY_STATE_DUAL_EXTENDED);
  EXPECT_EQ(1, observer_.num_changes());
  EXPECT_EQ(3, observer_.num_failures());

  UpdateOutputs(1, true);
  observer_.Reset();
  configurator_.SetDisplayMode(MULTIPLE_DISPLAY_STATE_HEADLESS);
  EXPECT_EQ(0, observer_.num_changes());
  EXPECT_EQ(1, observer_.num_failures());
  configurator_.SetDisplayMode(MULTIPLE_DISPLAY_STATE_SINGLE);
  EXPECT_EQ(1, observer_.num_changes());
  EXPECT_EQ(1, observer_.num_failures());
  configurator_.SetDisplayMode(MULTIPLE_DISPLAY_STATE_DUAL_MIRROR);
  configurator_.SetDisplayMode(MULTIPLE_DISPLAY_STATE_DUAL_EXTENDED);
  EXPECT_EQ(1, observer_.num_changes());
  EXPECT_EQ(3, observer_.num_failures());

  state_controller_.set_state(MULTIPLE_DISPLAY_STATE_DUAL_EXTENDED);
  UpdateOutputs(2, true);
  observer_.Reset();
  configurator_.SetDisplayMode(MULTIPLE_DISPLAY_STATE_HEADLESS);
  configurator_.SetDisplayMode(MULTIPLE_DISPLAY_STATE_SINGLE);
  EXPECT_EQ(0, observer_.num_changes());
  EXPECT_EQ(2, observer_.num_failures());
  configurator_.SetDisplayMode(MULTIPLE_DISPLAY_STATE_DUAL_MIRROR);
  configurator_.SetDisplayMode(MULTIPLE_DISPLAY_STATE_DUAL_EXTENDED);
  EXPECT_EQ(2, observer_.num_changes());
  EXPECT_EQ(2, observer_.num_failures());
}

TEST_F(DisplayConfiguratorTest, GetMultipleDisplayStateForMirroredDisplays) {
  UpdateOutputs(2, false);
  Init(false);
  state_controller_.set_state(MULTIPLE_DISPLAY_STATE_DUAL_MIRROR);
  configurator_.ForceInitialConfigure(0);
  EXPECT_EQ(MULTIPLE_DISPLAY_STATE_DUAL_MIRROR, configurator_.display_state());
}

TEST_F(DisplayConfiguratorTest, UpdateCachedOutputsEvenAfterFailure) {
  InitWithSingleOutput();
  const DisplayConfigurator::DisplayStateList& cached =
      configurator_.cached_displays();
  ASSERT_EQ(static_cast<size_t>(1), cached.size());
  EXPECT_EQ(outputs_[0].current_mode(), cached[0]->current_mode());

  // After connecting a second output, check that it shows up in
  // |cached_displays_| even if an invalid state is requested.
  state_controller_.set_state(MULTIPLE_DISPLAY_STATE_SINGLE);
  UpdateOutputs(2, true);
  ASSERT_EQ(static_cast<size_t>(2), cached.size());
  EXPECT_EQ(outputs_[0].current_mode(), cached[0]->current_mode());
  EXPECT_EQ(outputs_[1].current_mode(), cached[1]->current_mode());
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
  Init(true /* is_panel_fitting_enabled */);
  configurator_.ForceInitialConfigure(0);
  EXPECT_EQ(MULTIPLE_DISPLAY_STATE_DUAL_MIRROR, configurator_.display_state());
  EXPECT_EQ(
      JoinActions(
          kInitXRandR, kGrab,
          GetAddOutputModeAction(outputs_[0], &small_mode_).c_str(),
          GetFramebufferAction(small_mode_.size(), &outputs_[0], &outputs_[1])
              .c_str(),
          GetCrtcAction(outputs_[0], &small_mode_, gfx::Point(0, 0)).c_str(),
          GetCrtcAction(outputs_[1], &small_mode_, gfx::Point(0, 0)).c_str(),
          kForceDPMS, kUngrab, NULL),
      log_->GetActionsAndClear());

  // Both outputs should be using the small mode.
  ASSERT_EQ(1, observer_.num_changes());
  ASSERT_EQ(static_cast<size_t>(2), observer_.latest_outputs().size());
  EXPECT_EQ(&small_mode_, observer_.latest_outputs()[0]->current_mode());
  EXPECT_EQ(&small_mode_, observer_.latest_outputs()[1]->current_mode());

  // Also check that the newly-added small mode is present in the internal
  // snapshot that was passed to the observer (http://crbug.com/289159).
  DisplaySnapshot* state = observer_.latest_outputs()[0];
  ASSERT_NE(
      state->modes().end(),
      std::find(state->modes().begin(), state->modes().end(), &small_mode_));
}

TEST_F(DisplayConfiguratorTest, ContentProtection) {
  Init(false);
  configurator_.ForceInitialConfigure(0);
  EXPECT_NE(kNoActions, log_->GetActionsAndClear());

  DisplayConfigurator::ContentProtectionClientId id =
      configurator_.RegisterContentProtectionClient();
  EXPECT_NE(0u, id);

  // One output.
  UpdateOutputs(1, true);
  EXPECT_NE(kNoActions, log_->GetActionsAndClear());
  configurator_.QueryContentProtectionStatus(
      id, outputs_[0].display_id(),
      base::Bind(&DisplayConfiguratorTest::QueryContentProtectionCallback,
                 base::Unretained(this)));
  EXPECT_EQ(1, query_content_protection_call_count_);
  EXPECT_TRUE(query_content_protection_response_.success);
  EXPECT_EQ(static_cast<uint32_t>(DISPLAY_CONNECTION_TYPE_INTERNAL),
            query_content_protection_response_.link_mask);
  EXPECT_EQ(static_cast<uint32_t>(CONTENT_PROTECTION_METHOD_NONE),
            query_content_protection_response_.protection_mask);
  EXPECT_EQ(kNoActions, log_->GetActionsAndClear());

  // Two outputs.
  UpdateOutputs(2, true);
  EXPECT_NE(kNoActions, log_->GetActionsAndClear());
  configurator_.QueryContentProtectionStatus(
      id, outputs_[1].display_id(),
      base::Bind(&DisplayConfiguratorTest::QueryContentProtectionCallback,
                 base::Unretained(this)));
  EXPECT_EQ(2, query_content_protection_call_count_);
  EXPECT_TRUE(query_content_protection_response_.success);
  EXPECT_EQ(static_cast<uint32_t>(DISPLAY_CONNECTION_TYPE_HDMI),
            query_content_protection_response_.link_mask);
  EXPECT_EQ(static_cast<uint32_t>(CONTENT_PROTECTION_METHOD_NONE),
            query_content_protection_response_.protection_mask);
  EXPECT_EQ(kNoActions, log_->GetActionsAndClear());

  configurator_.EnableContentProtection(
      id, outputs_[1].display_id(), CONTENT_PROTECTION_METHOD_HDCP,
      base::Bind(&DisplayConfiguratorTest::EnableContentProtectionCallback,
                 base::Unretained(this)));
  EXPECT_EQ(1, enable_content_protection_call_count_);
  EXPECT_TRUE(enable_content_protection_status_);
  EXPECT_EQ(GetSetHDCPStateAction(outputs_[1], HDCP_STATE_DESIRED),
            log_->GetActionsAndClear());

  // Enable protection.
  native_display_delegate_->set_hdcp_state(HDCP_STATE_ENABLED);
  configurator_.QueryContentProtectionStatus(
      id, outputs_[1].display_id(),
      base::Bind(&DisplayConfiguratorTest::QueryContentProtectionCallback,
                 base::Unretained(this)));
  EXPECT_EQ(3, query_content_protection_call_count_);
  EXPECT_TRUE(query_content_protection_response_.success);
  EXPECT_EQ(static_cast<uint32_t>(DISPLAY_CONNECTION_TYPE_HDMI),
            query_content_protection_response_.link_mask);
  EXPECT_EQ(static_cast<uint32_t>(CONTENT_PROTECTION_METHOD_HDCP),
            query_content_protection_response_.protection_mask);
  EXPECT_EQ(kNoActions, log_->GetActionsAndClear());

  // Protections should be disabled after unregister.
  configurator_.UnregisterContentProtectionClient(id);
  EXPECT_EQ(GetSetHDCPStateAction(outputs_[1], HDCP_STATE_UNDESIRED),
            log_->GetActionsAndClear());
}

TEST_F(DisplayConfiguratorTest, DoNotConfigureWithSuspendedDisplays) {
  InitWithSingleOutput();

  // The DisplayConfigurator may occasionally receive OnConfigurationChanged()
  // after the displays have been suspended.  This event should be ignored since
  // the DisplayConfigurator will force a probe and reconfiguration of displays
  // at resume time.
  configurator_.SuspendDisplays(base::Bind(
      &DisplayConfiguratorTest::OnConfiguredCallback, base::Unretained(this)));
  EXPECT_EQ(CALLBACK_SUCCESS, PopCallbackResult());
  EXPECT_EQ(kNoActions, log_->GetActionsAndClear());

  // The configuration timer should not be started when the displays
  // are suspended.
  configurator_.OnConfigurationChanged();
  EXPECT_FALSE(test_api_.TriggerConfigureTimeout());
  EXPECT_EQ(kNoActions, log_->GetActionsAndClear());

  // Calls to SetDisplayPower and SetDisplayMode should be successful.
  configurator_.SetDisplayPower(
      chromeos::DISPLAY_POWER_ALL_OFF,
      DisplayConfigurator::kSetDisplayPowerNoFlags,
      base::Bind(&DisplayConfiguratorTest::OnConfiguredCallback,
                 base::Unretained(this)));
  EXPECT_EQ(CALLBACK_SUCCESS, PopCallbackResult());
  EXPECT_EQ(
      JoinActions(
          kGrab,
          GetFramebufferAction(small_mode_.size(), &outputs_[0], NULL).c_str(),
          GetCrtcAction(outputs_[0], NULL, gfx::Point(0, 0)).c_str(),
          kUngrab,
          NULL),
      log_->GetActionsAndClear());
  configurator_.SetDisplayPower(
      chromeos::DISPLAY_POWER_ALL_ON,
      DisplayConfigurator::kSetDisplayPowerNoFlags,
      base::Bind(&DisplayConfiguratorTest::OnConfiguredCallback,
                 base::Unretained(this)));
  EXPECT_EQ(CALLBACK_SUCCESS, PopCallbackResult());
  EXPECT_EQ(
      JoinActions(
          kGrab,
          GetFramebufferAction(small_mode_.size(), &outputs_[0], NULL).c_str(),
          GetCrtcAction(outputs_[0], &small_mode_, gfx::Point(0, 0)).c_str(),
          kForceDPMS,
          kUngrab,
          NULL),
      log_->GetActionsAndClear());

  UpdateOutputs(2, false);
  configurator_.SetDisplayMode(MULTIPLE_DISPLAY_STATE_DUAL_MIRROR);
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

  // The DisplayConfigurator should force a probe and reconfiguration at resume
  // time.
  UpdateOutputs(1, false);
  configurator_.ResumeDisplays();
  EXPECT_TRUE(test_api_.TriggerConfigureTimeout());
  EXPECT_EQ(
      JoinActions(
          kGrab,
          GetFramebufferAction(small_mode_.size(), &outputs_[0], NULL).c_str(),
          GetCrtcAction(outputs_[0], &small_mode_, gfx::Point(0, 0)).c_str(),
          kForceDPMS,
          kUngrab,
          NULL),
      log_->GetActionsAndClear());

  // If a configuration task is pending when the displays are suspended, that
  // task should not run either and the timer should be stopped.
  configurator_.OnConfigurationChanged();
  configurator_.SuspendDisplays(base::Bind(
      &DisplayConfiguratorTest::OnConfiguredCallback, base::Unretained(this)));
  EXPECT_EQ(CALLBACK_SUCCESS, PopCallbackResult());
  EXPECT_EQ(kNoActions, log_->GetActionsAndClear());

  EXPECT_FALSE(test_api_.TriggerConfigureTimeout());
  EXPECT_EQ(kNoActions, log_->GetActionsAndClear());

  configurator_.ResumeDisplays();
  EXPECT_TRUE(test_api_.TriggerConfigureTimeout());
  EXPECT_EQ(
      JoinActions(
          kGrab,
          GetFramebufferAction(small_mode_.size(), &outputs_[0], NULL).c_str(),
          GetCrtcAction(outputs_[0], &small_mode_, gfx::Point(0, 0)).c_str(),
          kForceDPMS,
          kUngrab,
          NULL),
      log_->GetActionsAndClear());
}

TEST_F(DisplayConfiguratorTest, ContentProtectionTwoClients) {
  DisplayConfigurator::ContentProtectionClientId client1 =
      configurator_.RegisterContentProtectionClient();
  DisplayConfigurator::ContentProtectionClientId client2 =
      configurator_.RegisterContentProtectionClient();
  EXPECT_NE(client1, client2);

  Init(false);
  configurator_.ForceInitialConfigure(0);
  UpdateOutputs(2, true);
  EXPECT_NE(kNoActions, log_->GetActionsAndClear());

  // Clients never know state enableness for methods that they didn't request.
  configurator_.EnableContentProtection(
      client1, outputs_[1].display_id(), CONTENT_PROTECTION_METHOD_HDCP,
      base::Bind(&DisplayConfiguratorTest::EnableContentProtectionCallback,
                 base::Unretained(this)));
  EXPECT_EQ(1, enable_content_protection_call_count_);
  EXPECT_TRUE(enable_content_protection_status_);
  EXPECT_EQ(GetSetHDCPStateAction(outputs_[1], HDCP_STATE_DESIRED).c_str(),
            log_->GetActionsAndClear());
  native_display_delegate_->set_hdcp_state(HDCP_STATE_ENABLED);

  configurator_.QueryContentProtectionStatus(
      client1, outputs_[1].display_id(),
      base::Bind(&DisplayConfiguratorTest::QueryContentProtectionCallback,
                 base::Unretained(this)));
  EXPECT_EQ(1, query_content_protection_call_count_);
  EXPECT_TRUE(query_content_protection_response_.success);
  EXPECT_EQ(static_cast<uint32_t>(DISPLAY_CONNECTION_TYPE_HDMI),
            query_content_protection_response_.link_mask);
  EXPECT_EQ(CONTENT_PROTECTION_METHOD_HDCP,
            query_content_protection_response_.protection_mask);

  configurator_.QueryContentProtectionStatus(
      client2, outputs_[1].display_id(),
      base::Bind(&DisplayConfiguratorTest::QueryContentProtectionCallback,
                 base::Unretained(this)));
  EXPECT_EQ(2, query_content_protection_call_count_);
  EXPECT_TRUE(query_content_protection_response_.success);
  EXPECT_EQ(static_cast<uint32_t>(DISPLAY_CONNECTION_TYPE_HDMI),
            query_content_protection_response_.link_mask);
  EXPECT_EQ(CONTENT_PROTECTION_METHOD_NONE,
            query_content_protection_response_.protection_mask);

  // Protections will be disabled only if no more clients request them.
  configurator_.EnableContentProtection(
      client2, outputs_[1].display_id(), CONTENT_PROTECTION_METHOD_NONE,
      base::Bind(&DisplayConfiguratorTest::EnableContentProtectionCallback,
                 base::Unretained(this)));
  EXPECT_EQ(2, enable_content_protection_call_count_);
  EXPECT_TRUE(enable_content_protection_status_);
  EXPECT_EQ(kNoActions, log_->GetActionsAndClear());

  configurator_.EnableContentProtection(
      client1, outputs_[1].display_id(), CONTENT_PROTECTION_METHOD_NONE,
      base::Bind(&DisplayConfiguratorTest::EnableContentProtectionCallback,
                 base::Unretained(this)));
  EXPECT_EQ(3, enable_content_protection_call_count_);
  EXPECT_TRUE(enable_content_protection_status_);
  EXPECT_EQ(GetSetHDCPStateAction(outputs_[1], HDCP_STATE_UNDESIRED).c_str(),
            log_->GetActionsAndClear());
}

TEST_F(DisplayConfiguratorTest, ContentProtectionTwoClientsEnable) {
  DisplayConfigurator::ContentProtectionClientId client1 =
      configurator_.RegisterContentProtectionClient();
  DisplayConfigurator::ContentProtectionClientId client2 =
      configurator_.RegisterContentProtectionClient();
  EXPECT_NE(client1, client2);

  Init(false);
  configurator_.ForceInitialConfigure(0);
  UpdateOutputs(2, true);
  log_->GetActionsAndClear();

  // Only enable once if HDCP is enabling.
  configurator_.EnableContentProtection(
      client1, outputs_[1].display_id(), CONTENT_PROTECTION_METHOD_HDCP,
      base::Bind(&DisplayConfiguratorTest::EnableContentProtectionCallback,
                 base::Unretained(this)));
  EXPECT_EQ(1, enable_content_protection_call_count_);
  EXPECT_TRUE(enable_content_protection_status_);
  native_display_delegate_->set_hdcp_state(HDCP_STATE_DESIRED);
  configurator_.EnableContentProtection(
      client2, outputs_[1].display_id(), CONTENT_PROTECTION_METHOD_HDCP,
      base::Bind(&DisplayConfiguratorTest::EnableContentProtectionCallback,
                 base::Unretained(this)));
  EXPECT_EQ(2, enable_content_protection_call_count_);
  EXPECT_TRUE(enable_content_protection_status_);
  EXPECT_EQ(GetSetHDCPStateAction(outputs_[1], HDCP_STATE_DESIRED).c_str(),
            log_->GetActionsAndClear());
  native_display_delegate_->set_hdcp_state(HDCP_STATE_ENABLED);

  // Don't enable again if HDCP is already active.
  configurator_.EnableContentProtection(
      client1, outputs_[1].display_id(), CONTENT_PROTECTION_METHOD_HDCP,
      base::Bind(&DisplayConfiguratorTest::EnableContentProtectionCallback,
                 base::Unretained(this)));
  EXPECT_EQ(3, enable_content_protection_call_count_);
  EXPECT_TRUE(enable_content_protection_status_);
  configurator_.EnableContentProtection(
      client2, outputs_[1].display_id(), CONTENT_PROTECTION_METHOD_HDCP,
      base::Bind(&DisplayConfiguratorTest::EnableContentProtectionCallback,
                 base::Unretained(this)));
  EXPECT_EQ(4, enable_content_protection_call_count_);
  EXPECT_TRUE(enable_content_protection_status_);
  EXPECT_EQ(kNoActions, log_->GetActionsAndClear());
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
          kGrab, GetFramebufferAction(modes[0]->size(), &outputs_[0],
                                      &outputs_[1]).c_str(),
          GetCrtcAction(outputs_[0], modes[0], gfx::Point(0, 0)).c_str(),
          // Then attempt to configure crtc1 with the first mode.
          GetCrtcAction(outputs_[1], modes[0], gfx::Point(0, 0)).c_str(),
          // First mode tried is expected to fail and it will
          // retry wil the 4th mode in the list.
          GetCrtcAction(outputs_[0], modes[3], gfx::Point(0, 0)).c_str(),
          GetCrtcAction(outputs_[1], modes[3], gfx::Point(0, 0)).c_str(),
          // Since it was requested to go into mirror mode
          // and the configured modes were different, it
          // should now try and setup a valid configurable
          // extended mode.
          GetFramebufferAction(
              gfx::Size(modes[0]->size().width(),
                        modes[0]->size().height() + modes[0]->size().height() +
                            DisplayConfigurator::kVerticalGap),
              &outputs_[0], &outputs_[1]).c_str(),
          GetCrtcAction(outputs_[0], modes[0], gfx::Point(0, 0)).c_str(),
          GetCrtcAction(outputs_[1], modes[0],
                        gfx::Point(0, modes[0]->size().height() +
                                          DisplayConfigurator::kVerticalGap))
              .c_str(),
          GetCrtcAction(outputs_[0], modes[3], gfx::Point(0, 0)).c_str(),
          GetCrtcAction(outputs_[1], modes[3],
                        gfx::Point(0, modes[0]->size().height() +
                                          DisplayConfigurator::kVerticalGap))
              .c_str(),
          kUngrab, NULL),
      log_->GetActionsAndClear());
}

// Tests that power state requests are saved after failed configuration attempts
// so they can be reused later: http://crosbug.com/p/31571
TEST_F(DisplayConfiguratorTest, SaveDisplayPowerStateOnConfigFailure) {
  // Start out with two displays in extended mode.
  state_controller_.set_state(MULTIPLE_DISPLAY_STATE_DUAL_EXTENDED);
  Init(false);
  configurator_.ForceInitialConfigure(0);
  log_->GetActionsAndClear();
  observer_.Reset();

  // Turn off the internal display, simulating docked mode.
  configurator_.SetDisplayPower(
      chromeos::DISPLAY_POWER_INTERNAL_OFF_EXTERNAL_ON,
      DisplayConfigurator::kSetDisplayPowerNoFlags,
      base::Bind(&DisplayConfiguratorTest::OnConfiguredCallback,
                 base::Unretained(this)));
  EXPECT_EQ(CALLBACK_SUCCESS, PopCallbackResult());
  EXPECT_EQ(1, observer_.num_changes());
  EXPECT_EQ(0, observer_.num_failures());
  log_->GetActionsAndClear();

  // Make all subsequent configuration requests fail and try to turn the
  // internal display back on.
  native_display_delegate_->set_max_configurable_pixels(1);
  configurator_.SetDisplayPower(
      chromeos::DISPLAY_POWER_ALL_ON,
      DisplayConfigurator::kSetDisplayPowerNoFlags,
      base::Bind(&DisplayConfiguratorTest::OnConfiguredCallback,
                 base::Unretained(this)));
  EXPECT_EQ(CALLBACK_FAILURE, PopCallbackResult());
  EXPECT_EQ(1, observer_.num_changes());
  EXPECT_EQ(1, observer_.num_failures());
  log_->GetActionsAndClear();

  // Simulate the external display getting disconnected and check that the
  // internal display is turned on (i.e. DISPLAY_POWER_ALL_ON is used) rather
  // than the earlier DISPLAY_POWER_INTERNAL_OFF_EXTERNAL_ON state.
  native_display_delegate_->set_max_configurable_pixels(0);
  UpdateOutputs(1, true);
  EXPECT_EQ(JoinActions(kGrab, GetFramebufferAction(small_mode_.size(),
                                                    &outputs_[0], NULL).c_str(),
                        GetCrtcAction(outputs_[0], &small_mode_,
                                      gfx::Point(0, 0)).c_str(),
                        kForceDPMS, kUngrab, NULL),
            log_->GetActionsAndClear());
}

// Tests that the SetDisplayPowerState() task posted by HandleResume() doesn't
// use a stale state if a new state is requested before it runs:
// http://crosbug.com/p/32393
TEST_F(DisplayConfiguratorTest, DontRestoreStalePowerStateAfterResume) {
  // Start out with two displays in mirrored mode.
  state_controller_.set_state(MULTIPLE_DISPLAY_STATE_DUAL_MIRROR);
  Init(false);
  configurator_.ForceInitialConfigure(0);
  log_->GetActionsAndClear();
  observer_.Reset();

  // Turn off the internal display, simulating docked mode.
  configurator_.SetDisplayPower(
      chromeos::DISPLAY_POWER_INTERNAL_OFF_EXTERNAL_ON,
      DisplayConfigurator::kSetDisplayPowerNoFlags,
      base::Bind(&DisplayConfiguratorTest::OnConfiguredCallback,
                 base::Unretained(this)));
  EXPECT_EQ(CALLBACK_SUCCESS, PopCallbackResult());
  EXPECT_EQ(1, observer_.num_changes());
  EXPECT_EQ(0, observer_.num_failures());
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

  // Suspend and resume the system. Resuming should post a task to restore the
  // previous power state, additionally forcing a probe.
  configurator_.SuspendDisplays(base::Bind(
      &DisplayConfiguratorTest::OnConfiguredCallback, base::Unretained(this)));
  EXPECT_EQ(CALLBACK_SUCCESS, PopCallbackResult());
  configurator_.ResumeDisplays();

  // Before the task runs, exit docked mode.
  configurator_.SetDisplayPower(
      chromeos::DISPLAY_POWER_ALL_ON,
      DisplayConfigurator::kSetDisplayPowerNoFlags,
      base::Bind(&DisplayConfiguratorTest::OnConfiguredCallback,
                 base::Unretained(this)));
  EXPECT_EQ(CALLBACK_SUCCESS, PopCallbackResult());
  EXPECT_EQ(2, observer_.num_changes());
  EXPECT_EQ(0, observer_.num_failures());
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

  // Check that the task doesn't restore the old internal-off-external-on power
  // state.
  EXPECT_TRUE(test_api_.TriggerConfigureTimeout());
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
}

TEST_F(DisplayConfiguratorTest, ExternalControl) {
  InitWithSingleOutput();
  state_controller_.set_state(MULTIPLE_DISPLAY_STATE_SINGLE);
  configurator_.RelinquishControl(
      base::Bind(&DisplayConfiguratorTest::OnDisplayControlUpdated,
                 base::Unretained(this)));
  EXPECT_EQ(CALLBACK_SUCCESS, PopDisplayControlResult());
  EXPECT_EQ(
      JoinActions(
          kRelinquishDisplayControl,
          NULL),
      log_->GetActionsAndClear());
  configurator_.TakeControl(
      base::Bind(&DisplayConfiguratorTest::OnDisplayControlUpdated,
                 base::Unretained(this)));
  EXPECT_EQ(CALLBACK_SUCCESS, PopDisplayControlResult());
  EXPECT_EQ(JoinActions(kTakeDisplayControl, kGrab,
                        GetFramebufferAction(small_mode_.size(), &outputs_[0],
                                             nullptr).c_str(),
                        GetCrtcAction(outputs_[0], &small_mode_,
                                      gfx::Point(0, 0)).c_str(),
                        kUngrab, NULL),
            log_->GetActionsAndClear());
}

}  // namespace test
}  // namespace ui
