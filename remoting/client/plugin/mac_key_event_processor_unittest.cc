// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/plugin/mac_key_event_processor.h"
#include "remoting/proto/event.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

using remoting::protocol::InputStub;
using remoting::protocol::KeyEvent;
using remoting::protocol::MouseEvent;

namespace remoting {

namespace {

const int kShift = 16;
const int kOption = 18;
const int kLeftCmd = 91;
const int kRightCmd = 93;

class FakeInputStub : public InputStub {
 public:
  FakeInputStub() {}
  virtual ~FakeInputStub() {}

  virtual void InjectKeyEvent(const KeyEvent& event) {
    key_events_.push_back(std::make_pair(event.keycode(), event.pressed()));
  }

  virtual void InjectMouseEvent(const MouseEvent& event) {
  }

  void ExpectOneKeyEvent(int keycode, bool pressed) {
    EXPECT_EQ(1u, key_events_.size());
    EXPECT_EQ(keycode, key_events_[0].first);
    EXPECT_EQ(pressed, key_events_[0].second);
  }

  void ExpectTwoKeyEvents(int keycode0, bool pressed0,
                          int keycode1, bool pressed1) {
    EXPECT_EQ(2u, key_events_.size());
    EXPECT_EQ(keycode0, key_events_[0].first);
    EXPECT_EQ(pressed0, key_events_[0].second);
    EXPECT_EQ(keycode1, key_events_[1].first);
    EXPECT_EQ(pressed1, key_events_[1].second);
  }

  void ExpectThreeKeyEvents(int keycode0, bool pressed0,
                            int keycode1, bool pressed1,
                            int keycode2, bool pressed2) {
    EXPECT_EQ(3u, key_events_.size());
    EXPECT_EQ(keycode0, key_events_[0].first);
    EXPECT_EQ(pressed0, key_events_[0].second);
    EXPECT_EQ(keycode1, key_events_[1].first);
    EXPECT_EQ(pressed1, key_events_[1].second);
    EXPECT_EQ(keycode2, key_events_[2].first);
    EXPECT_EQ(pressed2, key_events_[2].second);
  }

  void ClearKeyEvents() {
    key_events_.clear();
  }

 private:
  std::vector<std::pair<int, bool> > key_events_;

  DISALLOW_COPY_AND_ASSIGN(FakeInputStub);
};

KeyEvent MakeKeyEvent(int keycode, bool pressed) {
  KeyEvent event;
  event.set_keycode(keycode);
  event.set_pressed(pressed);
  return event;
}

}  // namespace

// Test without pressing command key.
TEST(MacKeyEventProcessorTest, NoInjection) {
  FakeInputStub stub;
  MacKeyEventProcessor processor(&stub);

  // C Down.
  processor.InjectKeyEvent(MakeKeyEvent('C', true));
  stub.ExpectOneKeyEvent('C', true);
  stub.ClearKeyEvents();
  EXPECT_EQ(1, processor.NumberOfPressedKeys());

  // C Up.
  processor.InjectKeyEvent(MakeKeyEvent('C', false));
  stub.ExpectOneKeyEvent('C', false);
  stub.ClearKeyEvents();
  EXPECT_EQ(0, processor.NumberOfPressedKeys());
}

// Test pressing command key and other normal keys.
TEST(MacKeyEventProcessorTest, CmdKey) {
  FakeInputStub stub;
  MacKeyEventProcessor processor(&stub);

  // Left command key.
  processor.InjectKeyEvent(MakeKeyEvent(kLeftCmd, true));
  stub.ExpectOneKeyEvent(kLeftCmd, true);
  stub.ClearKeyEvents();
  EXPECT_EQ(1, processor.NumberOfPressedKeys());
  processor.InjectKeyEvent(MakeKeyEvent('C', true));
  stub.ExpectOneKeyEvent('C', true);
  stub.ClearKeyEvents();
  EXPECT_EQ(2, processor.NumberOfPressedKeys());
  processor.InjectKeyEvent(MakeKeyEvent(kLeftCmd, false));
  stub.ExpectTwoKeyEvents('C', false, kLeftCmd, false);
  stub.ClearKeyEvents();
  EXPECT_EQ(0, processor.NumberOfPressedKeys());

  // Right command key.
  processor.InjectKeyEvent(MakeKeyEvent(kRightCmd, true));
  stub.ExpectOneKeyEvent(kRightCmd, true);
  stub.ClearKeyEvents();
  EXPECT_EQ(1, processor.NumberOfPressedKeys());
  processor.InjectKeyEvent(MakeKeyEvent('C', true));
  stub.ExpectOneKeyEvent('C', true);
  stub.ClearKeyEvents();
  EXPECT_EQ(2, processor.NumberOfPressedKeys());
  processor.InjectKeyEvent(MakeKeyEvent(kRightCmd, false));
  stub.ExpectTwoKeyEvents('C', false, kRightCmd, false);
  stub.ClearKeyEvents();
  EXPECT_EQ(0, processor.NumberOfPressedKeys());

  // More than one keys after CMD.
  processor.InjectKeyEvent(MakeKeyEvent(kRightCmd, true));
  stub.ExpectOneKeyEvent(kRightCmd, true);
  stub.ClearKeyEvents();
  EXPECT_EQ(1, processor.NumberOfPressedKeys());
  processor.InjectKeyEvent(MakeKeyEvent('C', true));
  stub.ExpectOneKeyEvent('C', true);
  stub.ClearKeyEvents();
  EXPECT_EQ(2, processor.NumberOfPressedKeys());
  processor.InjectKeyEvent(MakeKeyEvent('V', true));
  stub.ExpectOneKeyEvent('V', true);
  stub.ClearKeyEvents();
  EXPECT_EQ(3, processor.NumberOfPressedKeys());
  processor.InjectKeyEvent(MakeKeyEvent(kRightCmd, false));
  stub.ExpectThreeKeyEvents('C', false, 'V', false,
                            kRightCmd, false);
  EXPECT_EQ(0, processor.NumberOfPressedKeys());
}

// Test pressing command and special keys.
TEST(MacKeyEventProcessorTest, SpecialKeys) {
  FakeInputStub stub;
  MacKeyEventProcessor processor(&stub);

  // Command + Shift.
  processor.InjectKeyEvent(MakeKeyEvent(kLeftCmd, true));
  stub.ExpectOneKeyEvent(kLeftCmd, true);
  stub.ClearKeyEvents();
  EXPECT_EQ(1, processor.NumberOfPressedKeys());
  processor.InjectKeyEvent(MakeKeyEvent(kShift, true));
  stub.ExpectOneKeyEvent(kShift, true);
  stub.ClearKeyEvents();
  EXPECT_EQ(2, processor.NumberOfPressedKeys());
  processor.InjectKeyEvent(MakeKeyEvent(kLeftCmd, false));
  stub.ExpectOneKeyEvent(kLeftCmd, false);
  stub.ClearKeyEvents();
  EXPECT_EQ(1, processor.NumberOfPressedKeys());
  processor.InjectKeyEvent(MakeKeyEvent(kShift, false));
  stub.ExpectOneKeyEvent(kShift, false);
  stub.ClearKeyEvents();
  EXPECT_EQ(0, processor.NumberOfPressedKeys());

  // Command + Option.
  processor.InjectKeyEvent(MakeKeyEvent(kLeftCmd, true));
  stub.ExpectOneKeyEvent(kLeftCmd, true);
  stub.ClearKeyEvents();
  EXPECT_EQ(1, processor.NumberOfPressedKeys());
  processor.InjectKeyEvent(MakeKeyEvent(kOption, true));
  stub.ExpectOneKeyEvent(kOption, true);
  stub.ClearKeyEvents();
  EXPECT_EQ(2, processor.NumberOfPressedKeys());
  processor.InjectKeyEvent(MakeKeyEvent(kLeftCmd, false));
  stub.ExpectOneKeyEvent(kLeftCmd, false);
  stub.ClearKeyEvents();
  EXPECT_EQ(1, processor.NumberOfPressedKeys());
  processor.InjectKeyEvent(MakeKeyEvent(kOption, false));
  stub.ExpectOneKeyEvent(kOption, false);
  stub.ClearKeyEvents();
  EXPECT_EQ(0, processor.NumberOfPressedKeys());
}

// Test pressing multiple command keys.
TEST(MacKeyEventProcessorTest, MultipleCmdKeys) {
  FakeInputStub stub;
  MacKeyEventProcessor processor(&stub);

  // Test multiple CMD keys at the same time.
  // L CMD Down.
  processor.InjectKeyEvent(MakeKeyEvent(kLeftCmd, true));
  stub.ExpectOneKeyEvent(kLeftCmd, true);
  stub.ClearKeyEvents();
  EXPECT_EQ(1, processor.NumberOfPressedKeys());
  // C Down.
  processor.InjectKeyEvent(MakeKeyEvent('C', true));
  stub.ExpectOneKeyEvent('C', true);
  stub.ClearKeyEvents();
  EXPECT_EQ(2, processor.NumberOfPressedKeys());
  // R CMD Down.
  processor.InjectKeyEvent(MakeKeyEvent(kRightCmd, true));
  stub.ExpectOneKeyEvent(kRightCmd, true);
  stub.ClearKeyEvents();
  EXPECT_EQ(3, processor.NumberOfPressedKeys());
  // L CMD Up.
  processor.InjectKeyEvent(MakeKeyEvent(kLeftCmd, false));
  stub.ExpectTwoKeyEvents('C', false, kLeftCmd, false);
  stub.ClearKeyEvents();
  EXPECT_EQ(1, processor.NumberOfPressedKeys());
}

// Test press C key before command key.
TEST(MacKeyEventProcessorTest, BeforeCmdKey) {
  FakeInputStub stub;
  MacKeyEventProcessor processor(&stub);

  // Press C before command key.
  processor.InjectKeyEvent(MakeKeyEvent('C', true));
  stub.ExpectOneKeyEvent('C', true);
  stub.ClearKeyEvents();
  EXPECT_EQ(1, processor.NumberOfPressedKeys());
  processor.InjectKeyEvent(MakeKeyEvent(kRightCmd, true));
  stub.ExpectOneKeyEvent(kRightCmd, true);
  stub.ClearKeyEvents();
  EXPECT_EQ(2, processor.NumberOfPressedKeys());
  processor.InjectKeyEvent(MakeKeyEvent(kRightCmd, false));
  stub.ExpectTwoKeyEvents('C', false, kRightCmd, false);
  stub.ClearKeyEvents();
  EXPECT_EQ(0, processor.NumberOfPressedKeys());
  processor.InjectKeyEvent(MakeKeyEvent('C', false));
  stub.ExpectOneKeyEvent('C', false);
  stub.ClearKeyEvents();
  EXPECT_EQ(0, processor.NumberOfPressedKeys());
}

// Test sending multiple keydowns.
TEST(MacKeyEventProcessorTest, MultipleKeydowns) {
  FakeInputStub stub;
  MacKeyEventProcessor processor(&stub);

  // 2 CMD Downs.
  processor.InjectKeyEvent(MakeKeyEvent(kLeftCmd, true));
  stub.ExpectOneKeyEvent(kLeftCmd, true);
  stub.ClearKeyEvents();
  EXPECT_EQ(1, processor.NumberOfPressedKeys());
  processor.InjectKeyEvent(MakeKeyEvent(kLeftCmd, true));
  stub.ExpectOneKeyEvent(kLeftCmd, true);
  stub.ClearKeyEvents();
  EXPECT_EQ(1, processor.NumberOfPressedKeys());
  // C Down.
  processor.InjectKeyEvent(MakeKeyEvent('C', true));
  stub.ExpectOneKeyEvent('C', true);
  stub.ClearKeyEvents();
  EXPECT_EQ(2, processor.NumberOfPressedKeys());
  // CMD Up.
  processor.InjectKeyEvent(MakeKeyEvent(kLeftCmd, false));
  stub.ExpectTwoKeyEvents('C', false, kLeftCmd, false);
  stub.ClearKeyEvents();
  EXPECT_EQ(0, processor.NumberOfPressedKeys());
}

}  // namespace remoting
