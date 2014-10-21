// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <linux/input.h>

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop/message_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/ozone/evdev/key_event_converter_evdev.h"
#include "ui/events/ozone/evdev/keyboard_evdev.h"

namespace ui {

const char kTestDevicePath[] = "/dev/input/test-device";

class MockKeyEventConverterEvdev : public KeyEventConverterEvdev {
 public:
  MockKeyEventConverterEvdev(int fd, KeyboardEvdev* keyboard)
      : KeyEventConverterEvdev(fd,
                               base::FilePath(kTestDevicePath),
                               1,
                               keyboard) {
    Start();
  }
  virtual ~MockKeyEventConverterEvdev() {};

 private:
  DISALLOW_COPY_AND_ASSIGN(MockKeyEventConverterEvdev);
};

}  // namespace ui

// Test fixture.
class KeyEventConverterEvdevTest : public testing::Test {
 public:
  KeyEventConverterEvdevTest() {}

  // Overridden from testing::Test:
  virtual void SetUp() override {

    // Set up pipe to satisfy message pump (unused).
    int evdev_io[2];
    if (pipe(evdev_io))
      PLOG(FATAL) << "failed pipe";
    events_in_ = evdev_io[0];
    events_out_ = evdev_io[1];

    modifiers_.reset(new ui::EventModifiersEvdev());
    keyboard_.reset(new ui::KeyboardEvdev(
        modifiers_.get(),
        base::Bind(&KeyEventConverterEvdevTest::DispatchEventForTest,
                   base::Unretained(this))));
    device_.reset(
        new ui::MockKeyEventConverterEvdev(events_in_, keyboard_.get()));
  }
  virtual void TearDown() override {
    device_.reset();
    keyboard_.reset();
    modifiers_.reset();
    close(events_in_);
    close(events_out_);
  }

  ui::MockKeyEventConverterEvdev* device() { return device_.get(); }
  ui::EventModifiersEvdev* modifiers() { return modifiers_.get(); }

  unsigned size() { return dispatched_events_.size(); }
  ui::KeyEvent* dispatched_event(unsigned index) {
    DCHECK_GT(dispatched_events_.size(), index);
    ui::Event* ev = dispatched_events_[index];
    DCHECK(ev->IsKeyEvent());
    return static_cast<ui::KeyEvent*>(ev);
  }

 private:
  void DispatchEventForTest(scoped_ptr<ui::Event> event) {
    dispatched_events_.push_back(event.release());
  }

  base::MessageLoopForUI ui_loop_;

  scoped_ptr<ui::EventModifiersEvdev> modifiers_;
  scoped_ptr<ui::KeyboardEvdev> keyboard_;
  scoped_ptr<ui::MockKeyEventConverterEvdev> device_;

  ScopedVector<ui::Event> dispatched_events_;

  int events_out_;
  int events_in_;

  DISALLOW_COPY_AND_ASSIGN(KeyEventConverterEvdevTest);
};

TEST_F(KeyEventConverterEvdevTest, KeyPress) {
  ui::MockKeyEventConverterEvdev* dev = device();

  struct input_event mock_kernel_queue[] = {
      {{0, 0}, EV_MSC, MSC_SCAN, 0x7002a},
      {{0, 0}, EV_KEY, KEY_BACKSPACE, 1},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_MSC, MSC_SCAN, 0x7002a},
      {{0, 0}, EV_KEY, KEY_BACKSPACE, 0},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
  };

  dev->ProcessEvents(mock_kernel_queue, arraysize(mock_kernel_queue));
  EXPECT_EQ(2u, size());

  ui::KeyEvent* event;

  event = dispatched_event(0);
  EXPECT_EQ(ui::ET_KEY_PRESSED, event->type());
  EXPECT_EQ(ui::VKEY_BACK, event->key_code());
  EXPECT_EQ(0, event->flags());

  event = dispatched_event(1);
  EXPECT_EQ(ui::ET_KEY_RELEASED, event->type());
  EXPECT_EQ(ui::VKEY_BACK, event->key_code());
  EXPECT_EQ(0, event->flags());
}

TEST_F(KeyEventConverterEvdevTest, KeyRepeat) {
  ui::MockKeyEventConverterEvdev* dev = device();

  struct input_event mock_kernel_queue[] = {
      {{0, 0}, EV_MSC, MSC_SCAN, 0x7002a},
      {{0, 0}, EV_KEY, KEY_BACKSPACE, 1},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_MSC, MSC_SCAN, 0x7002a},
      {{0, 0}, EV_KEY, KEY_BACKSPACE, 2},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_MSC, MSC_SCAN, 0x7002a},
      {{0, 0}, EV_KEY, KEY_BACKSPACE, 2},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_MSC, MSC_SCAN, 0x7002a},
      {{0, 0}, EV_KEY, KEY_BACKSPACE, 0},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
  };

  dev->ProcessEvents(mock_kernel_queue, arraysize(mock_kernel_queue));
  EXPECT_EQ(4u, size());

  ui::KeyEvent* event;

  event = dispatched_event(0);
  EXPECT_EQ(ui::ET_KEY_PRESSED, event->type());
  EXPECT_EQ(ui::VKEY_BACK, event->key_code());
  EXPECT_EQ(0, event->flags());

  event = dispatched_event(1);
  EXPECT_EQ(ui::ET_KEY_PRESSED, event->type());
  EXPECT_EQ(ui::VKEY_BACK, event->key_code());
  EXPECT_EQ(0, event->flags());

  event = dispatched_event(2);
  EXPECT_EQ(ui::ET_KEY_PRESSED, event->type());
  EXPECT_EQ(ui::VKEY_BACK, event->key_code());
  EXPECT_EQ(0, event->flags());

  event = dispatched_event(3);
  EXPECT_EQ(ui::ET_KEY_RELEASED, event->type());
  EXPECT_EQ(ui::VKEY_BACK, event->key_code());
  EXPECT_EQ(0, event->flags());
}

TEST_F(KeyEventConverterEvdevTest, NoEvents) {
  ui::MockKeyEventConverterEvdev* dev = device();
  dev->ProcessEvents(NULL, 0);
  EXPECT_EQ(0u, size());
}

TEST_F(KeyEventConverterEvdevTest, KeyWithModifier) {
  ui::MockKeyEventConverterEvdev* dev = device();

  struct input_event mock_kernel_queue[] = {
      {{0, 0}, EV_MSC, MSC_SCAN, 0x700e1},
      {{0, 0}, EV_KEY, KEY_LEFTSHIFT, 1},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_MSC, MSC_SCAN, 0x70004},
      {{0, 0}, EV_KEY, KEY_A, 1},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_MSC, MSC_SCAN, 0x70004},
      {{0, 0}, EV_KEY, KEY_A, 0},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_MSC, MSC_SCAN, 0x700e1},
      {{0, 0}, EV_KEY, KEY_LEFTSHIFT, 0},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
  };

  dev->ProcessEvents(mock_kernel_queue, arraysize(mock_kernel_queue));
  EXPECT_EQ(4u, size());

  ui::KeyEvent* event;

  event = dispatched_event(0);
  EXPECT_EQ(ui::ET_KEY_PRESSED, event->type());
  EXPECT_EQ(ui::VKEY_SHIFT, event->key_code());
  EXPECT_EQ(ui::EF_SHIFT_DOWN, event->flags());

  event = dispatched_event(1);
  EXPECT_EQ(ui::ET_KEY_PRESSED, event->type());
  EXPECT_EQ(ui::VKEY_A, event->key_code());
  EXPECT_EQ(ui::EF_SHIFT_DOWN, event->flags());

  event = dispatched_event(2);
  EXPECT_EQ(ui::ET_KEY_RELEASED, event->type());
  EXPECT_EQ(ui::VKEY_A, event->key_code());
  EXPECT_EQ(ui::EF_SHIFT_DOWN, event->flags());

  event = dispatched_event(3);
  EXPECT_EQ(ui::ET_KEY_RELEASED, event->type());
  EXPECT_EQ(ui::VKEY_SHIFT, event->key_code());
  EXPECT_EQ(0, event->flags());
}

TEST_F(KeyEventConverterEvdevTest, KeyWithDuplicateModifier) {
  ui::MockKeyEventConverterEvdev* dev = device();

  struct input_event mock_kernel_queue[] = {
      {{0, 0}, EV_MSC, MSC_SCAN, 0x700e1},
      {{0, 0}, EV_KEY, KEY_LEFTCTRL, 1},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_MSC, MSC_SCAN, 0x700e5},
      {{0, 0}, EV_KEY, KEY_RIGHTCTRL, 1},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_MSC, MSC_SCAN, 0x7001d},
      {{0, 0}, EV_KEY, KEY_Z, 1},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_MSC, MSC_SCAN, 0x7001d},
      {{0, 0}, EV_KEY, KEY_Z, 0},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_MSC, MSC_SCAN, 0x700e1},
      {{0, 0}, EV_KEY, KEY_LEFTCTRL, 0},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_MSC, MSC_SCAN, 0x700e5},
      {{0, 0}, EV_KEY, KEY_RIGHTCTRL, 0},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
  };

  dev->ProcessEvents(mock_kernel_queue, arraysize(mock_kernel_queue));
  EXPECT_EQ(6u, size());

  ui::KeyEvent* event;

  event = dispatched_event(0);
  EXPECT_EQ(ui::ET_KEY_PRESSED, event->type());
  EXPECT_EQ(ui::VKEY_CONTROL, event->key_code());
  EXPECT_EQ(ui::EF_CONTROL_DOWN, event->flags());

  event = dispatched_event(1);
  EXPECT_EQ(ui::ET_KEY_PRESSED, event->type());
  EXPECT_EQ(ui::VKEY_CONTROL, event->key_code());
  EXPECT_EQ(ui::EF_CONTROL_DOWN, event->flags());

  event = dispatched_event(2);
  EXPECT_EQ(ui::ET_KEY_PRESSED, event->type());
  EXPECT_EQ(ui::VKEY_Z, event->key_code());
  EXPECT_EQ(ui::EF_CONTROL_DOWN, event->flags());

  event = dispatched_event(3);
  EXPECT_EQ(ui::ET_KEY_RELEASED, event->type());
  EXPECT_EQ(ui::VKEY_Z, event->key_code());
  EXPECT_EQ(ui::EF_CONTROL_DOWN, event->flags());

  event = dispatched_event(4);
  EXPECT_EQ(ui::ET_KEY_RELEASED, event->type());
  EXPECT_EQ(ui::VKEY_CONTROL, event->key_code());
  EXPECT_EQ(ui::EF_CONTROL_DOWN, event->flags());

  event = dispatched_event(5);
  EXPECT_EQ(ui::ET_KEY_RELEASED, event->type());
  EXPECT_EQ(ui::VKEY_CONTROL, event->key_code());
  EXPECT_EQ(0, event->flags());
}

TEST_F(KeyEventConverterEvdevTest, KeyWithLock) {
  ui::MockKeyEventConverterEvdev* dev = device();

  struct input_event mock_kernel_queue[] = {
      {{0, 0}, EV_MSC, MSC_SCAN, 0x70039},
      {{0, 0}, EV_KEY, KEY_CAPSLOCK, 1},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_MSC, MSC_SCAN, 0x70039},
      {{0, 0}, EV_KEY, KEY_CAPSLOCK, 0},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_MSC, MSC_SCAN, 0x70014},
      {{0, 0}, EV_KEY, KEY_Q, 1},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_MSC, MSC_SCAN, 0x70014},
      {{0, 0}, EV_KEY, KEY_Q, 0},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_MSC, MSC_SCAN, 0x70039},
      {{0, 0}, EV_KEY, KEY_CAPSLOCK, 1},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_MSC, MSC_SCAN, 0x70039},
      {{0, 0}, EV_KEY, KEY_CAPSLOCK, 0},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
  };

  dev->ProcessEvents(mock_kernel_queue, arraysize(mock_kernel_queue));
  EXPECT_EQ(6u, size());

  ui::KeyEvent* event;

  event = dispatched_event(0);
  EXPECT_EQ(ui::ET_KEY_PRESSED, event->type());
  EXPECT_EQ(ui::VKEY_CAPITAL, event->key_code());
  EXPECT_EQ(ui::EF_CAPS_LOCK_DOWN, event->flags());

  event = dispatched_event(1);
  EXPECT_EQ(ui::ET_KEY_RELEASED, event->type());
  EXPECT_EQ(ui::VKEY_CAPITAL, event->key_code());
  EXPECT_EQ(ui::EF_CAPS_LOCK_DOWN, event->flags());

  event = dispatched_event(2);
  EXPECT_EQ(ui::ET_KEY_PRESSED, event->type());
  EXPECT_EQ(ui::VKEY_Q, event->key_code());
  EXPECT_EQ(ui::EF_CAPS_LOCK_DOWN, event->flags());

  event = dispatched_event(3);
  EXPECT_EQ(ui::ET_KEY_RELEASED, event->type());
  EXPECT_EQ(ui::VKEY_Q, event->key_code());
  EXPECT_EQ(ui::EF_CAPS_LOCK_DOWN, event->flags());

  event = dispatched_event(4);
  EXPECT_EQ(ui::ET_KEY_PRESSED, event->type());
  EXPECT_EQ(ui::VKEY_CAPITAL, event->key_code());
  EXPECT_EQ(0, event->flags());

  event = dispatched_event(5);
  EXPECT_EQ(ui::ET_KEY_RELEASED, event->type());
  EXPECT_EQ(ui::VKEY_CAPITAL, event->key_code());
  EXPECT_EQ(0, event->flags());
}

TEST_F(KeyEventConverterEvdevTest, UnmappedKeyPress) {
  ui::MockKeyEventConverterEvdev* dev = device();

  struct input_event mock_kernel_queue[] = {
      {{0, 0}, EV_KEY, BTN_TOUCH, 1},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_KEY, BTN_TOUCH, 0},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
  };

  dev->ProcessEvents(mock_kernel_queue, arraysize(mock_kernel_queue));
  EXPECT_EQ(0u, size());
}
