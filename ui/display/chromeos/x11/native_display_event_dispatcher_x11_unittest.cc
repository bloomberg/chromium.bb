// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <X11/extensions/Xrandr.h>

#undef Bool
#undef None

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/chromeos/x11/native_display_delegate_x11.h"
#include "ui/display/chromeos/x11/native_display_event_dispatcher_x11.h"

namespace ui {

namespace {

OutputConfigurator::OutputSnapshot CreateOutput(RROutput output,
                                                RRCrtc crtc,
                                                RRMode mode) {
  OutputConfigurator::OutputSnapshot snapshot;
  snapshot.output = output;
  snapshot.crtc = crtc;
  snapshot.current_mode = mode;

  return snapshot;
}

class TestHelperDelegate : public NativeDisplayDelegateX11::HelperDelegate {
 public:
  TestHelperDelegate();
  virtual ~TestHelperDelegate();

  int num_calls_update_xrandr_config() const {
    return num_calls_update_xrandr_config_;
  }

  int num_calls_notify_observers() const { return num_calls_notify_observers_; }

  void set_cached_outputs(
      const std::vector<OutputConfigurator::OutputSnapshot>& outputs) {
    cached_outputs_ = outputs;
  }

  // NativeDisplayDelegateX11::HelperDelegate overrides:
  virtual void UpdateXRandRConfiguration(const base::NativeEvent& event)
      OVERRIDE;
  virtual const std::vector<OutputConfigurator::OutputSnapshot>&
      GetCachedOutputs() const OVERRIDE;
  virtual void NotifyDisplayObservers() OVERRIDE;

 private:
  int num_calls_update_xrandr_config_;
  int num_calls_notify_observers_;

  std::vector<OutputConfigurator::OutputSnapshot> cached_outputs_;

  DISALLOW_COPY_AND_ASSIGN(TestHelperDelegate);
};

TestHelperDelegate::TestHelperDelegate()
    : num_calls_update_xrandr_config_(0), num_calls_notify_observers_(0) {}

TestHelperDelegate::~TestHelperDelegate() {}

void TestHelperDelegate::UpdateXRandRConfiguration(
    const base::NativeEvent& event) {
  ++num_calls_update_xrandr_config_;
}

const std::vector<OutputConfigurator::OutputSnapshot>&
TestHelperDelegate::GetCachedOutputs() const {
  return cached_outputs_;
}

void TestHelperDelegate::NotifyDisplayObservers() {
  ++num_calls_notify_observers_;
}

////////////////////////////////////////////////////////////////////////////////
// NativeDisplayEventDispatcherX11Test

class NativeDisplayEventDispatcherX11Test : public testing::Test {
 public:
  NativeDisplayEventDispatcherX11Test();
  virtual ~NativeDisplayEventDispatcherX11Test();

 protected:
  void DispatchScreenChangeEvent();
  void DispatchOutputChangeEvent(RROutput output,
                                 RRCrtc crtc,
                                 RRMode mode,
                                 bool connected);

  int xrandr_event_base_;
  scoped_ptr<TestHelperDelegate> helper_delegate_;
  scoped_ptr<NativeDisplayEventDispatcherX11> dispatcher_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NativeDisplayEventDispatcherX11Test);
};

NativeDisplayEventDispatcherX11Test::NativeDisplayEventDispatcherX11Test()
    : xrandr_event_base_(10),
      helper_delegate_(new TestHelperDelegate()),
      dispatcher_(new NativeDisplayEventDispatcherX11(helper_delegate_.get(),
                                                      xrandr_event_base_)) {}

NativeDisplayEventDispatcherX11Test::~NativeDisplayEventDispatcherX11Test() {}

void NativeDisplayEventDispatcherX11Test::DispatchScreenChangeEvent() {
  XRRScreenChangeNotifyEvent event = {0};
  event.type = xrandr_event_base_ + RRScreenChangeNotify;

  dispatcher_->Dispatch(reinterpret_cast<const base::NativeEvent>(&event));
}

void NativeDisplayEventDispatcherX11Test::DispatchOutputChangeEvent(
    RROutput output,
    RRCrtc crtc,
    RRMode mode,
    bool connected) {
  XRROutputChangeNotifyEvent event = {0};
  event.type = xrandr_event_base_ + RRNotify;
  event.subtype = RRNotify_OutputChange;
  event.output = output;
  event.crtc = crtc;
  event.mode = mode;
  event.connection = connected ? RR_Connected : RR_Disconnected;

  dispatcher_->Dispatch(reinterpret_cast<const base::NativeEvent>(&event));
}

}  // namespace

TEST_F(NativeDisplayEventDispatcherX11Test, OnScreenChangedEvent) {
  DispatchScreenChangeEvent();
  EXPECT_EQ(1, helper_delegate_->num_calls_update_xrandr_config());
  EXPECT_EQ(0, helper_delegate_->num_calls_notify_observers());
}

TEST_F(NativeDisplayEventDispatcherX11Test, CheckNotificationOnFirstEvent) {
  DispatchOutputChangeEvent(1, 10, 20, true);
  EXPECT_EQ(0, helper_delegate_->num_calls_update_xrandr_config());
  EXPECT_EQ(1, helper_delegate_->num_calls_notify_observers());
}

TEST_F(NativeDisplayEventDispatcherX11Test, CheckNotificationAfterSecondEvent) {
  DispatchOutputChangeEvent(1, 10, 20, true);

  // Simulate addition of the first output to the cached output list.
  std::vector<OutputConfigurator::OutputSnapshot> outputs;
  outputs.push_back(CreateOutput(1, 10, 20));
  helper_delegate_->set_cached_outputs(outputs);

  DispatchOutputChangeEvent(2, 11, 20, true);
  EXPECT_EQ(2, helper_delegate_->num_calls_notify_observers());
}

TEST_F(NativeDisplayEventDispatcherX11Test, AvoidNotificationOnDuplicateEvent) {
  std::vector<OutputConfigurator::OutputSnapshot> outputs;
  outputs.push_back(CreateOutput(1, 10, 20));
  helper_delegate_->set_cached_outputs(outputs);

  DispatchOutputChangeEvent(1, 10, 20, true);
  EXPECT_EQ(0, helper_delegate_->num_calls_notify_observers());
}

TEST_F(NativeDisplayEventDispatcherX11Test, CheckNotificationOnDisconnect) {
  std::vector<OutputConfigurator::OutputSnapshot> outputs;
  outputs.push_back(CreateOutput(1, 10, 20));
  helper_delegate_->set_cached_outputs(outputs);

  DispatchOutputChangeEvent(1, 10, 20, false);
  EXPECT_EQ(1, helper_delegate_->num_calls_notify_observers());
}

TEST_F(NativeDisplayEventDispatcherX11Test, CheckNotificationOnModeChange) {
  std::vector<OutputConfigurator::OutputSnapshot> outputs;
  outputs.push_back(CreateOutput(1, 10, 20));
  helper_delegate_->set_cached_outputs(outputs);

  DispatchOutputChangeEvent(1, 10, 21, true);
  EXPECT_EQ(1, helper_delegate_->num_calls_notify_observers());
}

TEST_F(NativeDisplayEventDispatcherX11Test, CheckNotificationOnSecondOutput) {
  std::vector<OutputConfigurator::OutputSnapshot> outputs;
  outputs.push_back(CreateOutput(1, 10, 20));
  helper_delegate_->set_cached_outputs(outputs);

  DispatchOutputChangeEvent(2, 11, 20, true);
  EXPECT_EQ(1, helper_delegate_->num_calls_notify_observers());
}

TEST_F(NativeDisplayEventDispatcherX11Test, CheckNotificationOnDifferentCrtc) {
  std::vector<OutputConfigurator::OutputSnapshot> outputs;
  outputs.push_back(CreateOutput(1, 10, 20));
  helper_delegate_->set_cached_outputs(outputs);

  DispatchOutputChangeEvent(1, 11, 20, true);
  EXPECT_EQ(1, helper_delegate_->num_calls_notify_observers());
}

TEST_F(NativeDisplayEventDispatcherX11Test,
       CheckNotificationOnSecondOutputDisconnect) {
  std::vector<OutputConfigurator::OutputSnapshot> outputs;
  outputs.push_back(CreateOutput(1, 10, 20));
  outputs.push_back(CreateOutput(2, 11, 20));
  helper_delegate_->set_cached_outputs(outputs);

  DispatchOutputChangeEvent(2, 11, 20, false);
  EXPECT_EQ(1, helper_delegate_->num_calls_notify_observers());
}

TEST_F(NativeDisplayEventDispatcherX11Test,
       AvoidDuplicateNotificationOnSecondOutputDisconnect) {
  std::vector<OutputConfigurator::OutputSnapshot> outputs;
  outputs.push_back(CreateOutput(1, 10, 20));
  outputs.push_back(CreateOutput(2, 11, 20));
  helper_delegate_->set_cached_outputs(outputs);

  DispatchOutputChangeEvent(2, 11, 20, false);
  EXPECT_EQ(1, helper_delegate_->num_calls_notify_observers());

  // Simulate removal of second output from cached output list.
  outputs.erase(outputs.begin() + 1);
  helper_delegate_->set_cached_outputs(outputs);

  DispatchOutputChangeEvent(2, 11, 20, false);
  EXPECT_EQ(1, helper_delegate_->num_calls_notify_observers());
}

}  // namespace ui
