// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/event_monitor_mac.h"

#include "ui/events/event.h"
#include "ui/events/event_handler.h"
#include "ui/events/event_constants.h"
#import "ui/events/test/cocoa_test_event_utils.h"
#import "ui/gfx/test/ui_cocoa_test_helper.h"

namespace views {

namespace {

class EventMonitorMacTest : public ui::CocoaTest {
 public:
  EventMonitorMacTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(EventMonitorMacTest);
};

class EventCounter : public ui::EventHandler {
 public:
   EventCounter() : event_count_(0), last_event_type_(ui::ET_UNKNOWN) {}
   int event_count() const { return event_count_; }
   ui::EventType last_event_type() { return last_event_type_; }

  // ui::EventHandler implementation:
  virtual void OnEvent(ui::Event* event) override {
    ++event_count_;
    last_event_type_ = event->type();
  }

 private:
  int event_count_;
  ui::EventType last_event_type_;
};

}  // namespace

TEST_F(EventMonitorMacTest, CountEvents) {
  EventCounter counter;
  NSEvent* event =
      cocoa_test_event_utils::EnterExitEventWithType(NSMouseExited);

  // No monitor installed yet, should not receive events.
  [NSApp sendEvent:event];
  EXPECT_EQ(0, counter.event_count());
  EXPECT_EQ(ui::ET_UNKNOWN, counter.last_event_type());

  // Install monitor, should start receiving event.
  scoped_ptr<EventMonitor> event_monitor(EventMonitor::Create(&counter));
  [NSApp sendEvent:event];
  EXPECT_EQ(1, counter.event_count());
  EXPECT_EQ(ui::ET_MOUSE_EXITED, counter.last_event_type());

  // Uninstall monitor, should stop receiving events.
  event_monitor.reset();
  [NSApp sendEvent:event];
  EXPECT_EQ(1, counter.event_count());
  EXPECT_EQ(ui::ET_MOUSE_EXITED, counter.last_event_type());
}

}  // namespace views
