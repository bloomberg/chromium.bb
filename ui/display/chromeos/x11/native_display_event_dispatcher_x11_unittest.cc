// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <X11/extensions/Xrandr.h>

#undef Bool
#undef None

#include "base/test/simple_test_tick_clock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/chromeos/x11/display_mode_x11.h"
#include "ui/display/chromeos/x11/display_snapshot_x11.h"
#include "ui/display/chromeos/x11/native_display_delegate_x11.h"
#include "ui/display/chromeos/x11/native_display_event_dispatcher_x11.h"

namespace ui {

namespace {

DisplaySnapshotX11* CreateOutput(RROutput output, RRCrtc crtc) {
  static const DisplayModeX11* kDefaultDisplayMode =
      new DisplayModeX11(gfx::Size(1, 1), false, 60.0f, 20);

  DisplaySnapshotX11* snapshot = new DisplaySnapshotX11(
      0,
      false,
      gfx::Point(0, 0),
      gfx::Size(0, 0),
      DISPLAY_CONNECTION_TYPE_UNKNOWN,
      false,
      false,
      std::string(),
      std::vector<const DisplayMode*>(1, kDefaultDisplayMode),
      kDefaultDisplayMode,
      NULL,
      output,
      crtc,
      0);

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

  void set_cached_outputs(const std::vector<DisplaySnapshot*>& outputs) {
    cached_outputs_ = outputs;
  }

  // NativeDisplayDelegateX11::HelperDelegate overrides:
  virtual void UpdateXRandRConfiguration(const base::NativeEvent& event)
      OVERRIDE;
  virtual const std::vector<DisplaySnapshot*>& GetCachedDisplays() const
      OVERRIDE;
  virtual void NotifyDisplayObservers() OVERRIDE;

 private:
  int num_calls_update_xrandr_config_;
  int num_calls_notify_observers_;

  std::vector<DisplaySnapshot*> cached_outputs_;

  DISALLOW_COPY_AND_ASSIGN(TestHelperDelegate);
};

TestHelperDelegate::TestHelperDelegate()
    : num_calls_update_xrandr_config_(0), num_calls_notify_observers_(0) {}

TestHelperDelegate::~TestHelperDelegate() {}

void TestHelperDelegate::UpdateXRandRConfiguration(
    const base::NativeEvent& event) {
  ++num_calls_update_xrandr_config_;
}

const std::vector<DisplaySnapshot*>& TestHelperDelegate::GetCachedDisplays()
    const {
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
  base::SimpleTestTickClock* test_tick_clock_;  // Owned by |dispatcher_|.

 private:
  DISALLOW_COPY_AND_ASSIGN(NativeDisplayEventDispatcherX11Test);
};

NativeDisplayEventDispatcherX11Test::NativeDisplayEventDispatcherX11Test()
    : xrandr_event_base_(10),
      helper_delegate_(new TestHelperDelegate()),
      dispatcher_(new NativeDisplayEventDispatcherX11(helper_delegate_.get(),
                                                      xrandr_event_base_)),
      test_tick_clock_(new base::SimpleTestTickClock) {
  test_tick_clock_->Advance(base::TimeDelta::FromMilliseconds(1));
  dispatcher_->SetTickClockForTest(
      scoped_ptr<base::TickClock>(test_tick_clock_));
}

NativeDisplayEventDispatcherX11Test::~NativeDisplayEventDispatcherX11Test() {}

void NativeDisplayEventDispatcherX11Test::DispatchScreenChangeEvent() {
  XRRScreenChangeNotifyEvent event = {0};
  event.type = xrandr_event_base_ + RRScreenChangeNotify;

  dispatcher_->DispatchEvent(reinterpret_cast<const PlatformEvent>(&event));
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

  dispatcher_->DispatchEvent(reinterpret_cast<const PlatformEvent>(&event));
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
  ScopedVector<DisplaySnapshot> outputs;
  outputs.push_back(CreateOutput(1, 10));
  helper_delegate_->set_cached_outputs(outputs.get());

  DispatchOutputChangeEvent(2, 11, 20, true);
  EXPECT_EQ(2, helper_delegate_->num_calls_notify_observers());
}

TEST_F(NativeDisplayEventDispatcherX11Test, CheckNotificationOnDisconnect) {
  ScopedVector<DisplaySnapshot> outputs;
  outputs.push_back(CreateOutput(1, 10));
  helper_delegate_->set_cached_outputs(outputs.get());

  DispatchOutputChangeEvent(1, 10, 20, false);
  EXPECT_EQ(1, helper_delegate_->num_calls_notify_observers());
}

TEST_F(NativeDisplayEventDispatcherX11Test, CheckNotificationOnModeChange) {
  ScopedVector<DisplaySnapshot> outputs;
  outputs.push_back(CreateOutput(1, 10));
  helper_delegate_->set_cached_outputs(outputs.get());

  DispatchOutputChangeEvent(1, 10, 21, true);
  EXPECT_EQ(1, helper_delegate_->num_calls_notify_observers());
}

TEST_F(NativeDisplayEventDispatcherX11Test, CheckNotificationOnSecondOutput) {
  ScopedVector<DisplaySnapshot> outputs;
  outputs.push_back(CreateOutput(1, 10));
  helper_delegate_->set_cached_outputs(outputs.get());

  DispatchOutputChangeEvent(2, 11, 20, true);
  EXPECT_EQ(1, helper_delegate_->num_calls_notify_observers());
}

TEST_F(NativeDisplayEventDispatcherX11Test, CheckNotificationOnDifferentCrtc) {
  ScopedVector<DisplaySnapshot> outputs;
  outputs.push_back(CreateOutput(1, 10));
  helper_delegate_->set_cached_outputs(outputs.get());

  DispatchOutputChangeEvent(1, 11, 20, true);
  EXPECT_EQ(1, helper_delegate_->num_calls_notify_observers());
}

TEST_F(NativeDisplayEventDispatcherX11Test,
       CheckNotificationOnSecondOutputDisconnect) {
  ScopedVector<DisplaySnapshot> outputs;
  outputs.push_back(CreateOutput(1, 10));
  outputs.push_back(CreateOutput(2, 11));
  helper_delegate_->set_cached_outputs(outputs.get());

  DispatchOutputChangeEvent(2, 11, 20, false);
  EXPECT_EQ(1, helper_delegate_->num_calls_notify_observers());
}

TEST_F(NativeDisplayEventDispatcherX11Test,
       AvoidDuplicateNotificationOnSecondOutputDisconnect) {
  ScopedVector<DisplaySnapshot> outputs;
  outputs.push_back(CreateOutput(1, 10));
  outputs.push_back(CreateOutput(2, 11));
  helper_delegate_->set_cached_outputs(outputs.get());

  DispatchOutputChangeEvent(2, 11, 20, false);
  EXPECT_EQ(1, helper_delegate_->num_calls_notify_observers());

  // Simulate removal of second output from cached output list.
  outputs.erase(outputs.begin() + 1);
  helper_delegate_->set_cached_outputs(outputs.get());

  DispatchOutputChangeEvent(2, 11, 20, false);
  EXPECT_EQ(1, helper_delegate_->num_calls_notify_observers());
}

TEST_F(NativeDisplayEventDispatcherX11Test,
       ForceUpdateAfterCacheExpiration) {
  // +1 to compenstate a possible rounding error.
  const int kHalfOfExpirationMs =
      NativeDisplayEventDispatcherX11::kUseCacheAfterStartupMs / 2 + 1;

  ScopedVector<DisplaySnapshot> outputs;
  outputs.push_back(CreateOutput(1, 10));
  outputs.push_back(CreateOutput(2, 11));
  helper_delegate_->set_cached_outputs(outputs.get());

  EXPECT_EQ(0, helper_delegate_->num_calls_notify_observers());

  // Duplicated event will be ignored during the startup.
  DispatchOutputChangeEvent(2, 11, 20, true);
  EXPECT_EQ(0, helper_delegate_->num_calls_notify_observers());

  test_tick_clock_->Advance(base::TimeDelta::FromMilliseconds(
      kHalfOfExpirationMs));

  // Duplicated event will still be ignored.
  DispatchOutputChangeEvent(2, 11, 20, true);
  EXPECT_EQ(0, helper_delegate_->num_calls_notify_observers());

  // The startup timeout has been elapsed. Duplicated event
  // should not be ignored.
  test_tick_clock_->Advance(
      base::TimeDelta::FromMilliseconds(kHalfOfExpirationMs));
  DispatchOutputChangeEvent(2, 11, 20, true);
  EXPECT_EQ(1, helper_delegate_->num_calls_notify_observers());

  // Sending the same event immediately shoudldn't be ignored.
  DispatchOutputChangeEvent(2, 11, 20, true);
  EXPECT_EQ(2, helper_delegate_->num_calls_notify_observers());

  // Advancing time further should not change the behavior.
  test_tick_clock_->Advance(base::TimeDelta::FromMilliseconds(
      kHalfOfExpirationMs));
  DispatchOutputChangeEvent(2, 11, 20, true);
  EXPECT_EQ(3, helper_delegate_->num_calls_notify_observers());

  test_tick_clock_->Advance(
      base::TimeDelta::FromMilliseconds(kHalfOfExpirationMs));
  DispatchOutputChangeEvent(2, 11, 20, true);
  EXPECT_EQ(4, helper_delegate_->num_calls_notify_observers());
}

}  // namespace ui
