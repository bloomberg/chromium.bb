// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <unistd.h>

#include <vector>

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/posix/eintr_wrapper.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"
#include "ui/events/ozone/evdev/tablet_event_converter_evdev.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/events/platform/platform_event_source.h"

namespace {

static int SetNonBlocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1)
    flags = 0;
  return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

const char kTestDevicePath[] = "/dev/input/test-device";

}  // namespace

namespace ui {

class MockTabletEventConverterEvdev : public TabletEventConverterEvdev {
 public:
  MockTabletEventConverterEvdev(int fd,
                                base::FilePath path,
                                EventModifiersEvdev* modifiers,
                                CursorDelegateEvdev* cursor);
  virtual ~MockTabletEventConverterEvdev(){};

  void ConfigureReadMock(struct input_event* queue,
                         long read_this_many,
                         long queue_index);

  unsigned size() { return dispatched_events_.size(); }
  MouseEvent* event(unsigned index) {
    DCHECK_GT(dispatched_events_.size(), index);
    Event* ev = dispatched_events_[index];
    DCHECK(ev->IsMouseEvent());
    return static_cast<MouseEvent*>(ev);
  }

  // Actually dispatch the event reader code.
  void ReadNow() {
    OnFileCanReadWithoutBlocking(read_pipe_);
    base::RunLoop().RunUntilIdle();
  }

  void DispatchCallback(scoped_ptr<Event> event) {
    dispatched_events_.push_back(event.release());
  }

 private:
  int read_pipe_;
  int write_pipe_;

  ScopedVector<Event> dispatched_events_;

  DISALLOW_COPY_AND_ASSIGN(MockTabletEventConverterEvdev);
};

class MockTabletCursorEvdev : public CursorDelegateEvdev {
 public:
  MockTabletCursorEvdev() { cursor_display_bounds_ = gfx::Rect(1024, 768); }
  virtual ~MockTabletCursorEvdev() {}

  // CursorDelegateEvdev:
  void MoveCursorTo(gfx::AcceleratedWidget widget,
                    const gfx::PointF& location) override {
    NOTREACHED();
  }
  void MoveCursorTo(const gfx::PointF& location) override {
    cursor_location_ = location;
  }
  void MoveCursor(const gfx::Vector2dF& delta) override { NOTREACHED(); }
  bool IsCursorVisible() override { return 1; }
  gfx::PointF GetLocation() override { return cursor_location_; }
  gfx::Rect GetCursorDisplayBounds() override { return cursor_display_bounds_; }

 private:
  gfx::PointF cursor_location_;
  gfx::Rect cursor_display_bounds_;
  DISALLOW_COPY_AND_ASSIGN(MockTabletCursorEvdev);
};

MockTabletEventConverterEvdev::MockTabletEventConverterEvdev(
    int fd,
    base::FilePath path,
    EventModifiersEvdev* modifiers,
    CursorDelegateEvdev* cursor)
    : TabletEventConverterEvdev(
          fd,
          path,
          1,
          INPUT_DEVICE_UNKNOWN,
          modifiers,
          cursor,
          EventDeviceInfo(),
          base::Bind(&MockTabletEventConverterEvdev::DispatchCallback,
                     base::Unretained(this))) {
  // Real values taken from Wacom Intuos 4
  x_abs_min_ = 0;
  x_abs_range_ = 65024;
  y_abs_min_ = 0;
  y_abs_range_ = 40640;

  int fds[2];

  if (pipe(fds))
    PLOG(FATAL) << "failed pipe";

  DCHECK(SetNonBlocking(fds[0]) == 0)
      << "SetNonBlocking for pipe fd[0] failed, errno: " << errno;
  DCHECK(SetNonBlocking(fds[1]) == 0)
      << "SetNonBlocking for pipe fd[0] failed, errno: " << errno;
  read_pipe_ = fds[0];
  write_pipe_ = fds[1];
}

void MockTabletEventConverterEvdev::ConfigureReadMock(struct input_event* queue,
                                                      long read_this_many,
                                                      long queue_index) {
  int nwrite = HANDLE_EINTR(write(write_pipe_, queue + queue_index,
                                  sizeof(struct input_event) * read_this_many));
  DCHECK(nwrite ==
         static_cast<int>(sizeof(struct input_event) * read_this_many))
      << "write() failed, errno: " << errno;
}

}  // namespace ui

// Test fixture.
class TabletEventConverterEvdevTest : public testing::Test {
 public:
  TabletEventConverterEvdevTest() {}

  // Overridden from testing::Test:
  virtual void SetUp() override {
    // Set up pipe to satisfy message pump (unused).
    int evdev_io[2];
    if (pipe(evdev_io))
      PLOG(FATAL) << "failed pipe";
    events_in_ = evdev_io[0];
    events_out_ = evdev_io[1];

    cursor_.reset(new ui::MockTabletCursorEvdev());
    modifiers_.reset(new ui::EventModifiersEvdev());
    device_.reset(new ui::MockTabletEventConverterEvdev(
        events_in_, base::FilePath(kTestDevicePath), modifiers_.get(),
        cursor_.get()));
  }

  virtual void TearDown() override {
    modifiers_.reset();
    cursor_.reset();
    device_.reset();
  }

  ui::MockTabletEventConverterEvdev* device() { return device_.get(); }
  ui::CursorDelegateEvdev* cursor() { return cursor_.get(); }
  ui::EventModifiersEvdev* modifiers() { return modifiers_.get(); }

 private:
  scoped_ptr<ui::MockTabletEventConverterEvdev> device_;
  scoped_ptr<ui::MockTabletCursorEvdev> cursor_;
  scoped_ptr<ui::EventModifiersEvdev> modifiers_;

  int events_out_;
  int events_in_;

  DISALLOW_COPY_AND_ASSIGN(TabletEventConverterEvdevTest);
};

#define EPSILON 20

// Uses real data captured from Wacom Intuos 4
TEST_F(TabletEventConverterEvdevTest, MoveTopLeft) {
  ui::MockTabletEventConverterEvdev* dev = device();

  struct input_event mock_kernel_queue[] = {
      {{0, 0}, EV_ABS, ABS_Y, 616},
      {{0, 0}, EV_ABS, ABS_DISTANCE, 62},
      {{0, 0}, EV_ABS, ABS_TILT_X, 50},
      {{0, 0}, EV_ABS, ABS_TILT_Y, 7},
      {{0, 0}, EV_ABS, ABS_MISC, 1050626},
      {{0, 0}, EV_KEY, BTN_TOOL_PEN, 1},
      {{0, 0}, EV_MSC, MSC_SERIAL, 159403517},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
      {{0, 0}, EV_ABS, ABS_Y, 0},
      {{0, 0}, EV_ABS, ABS_DISTANCE, 0},
      {{0, 0}, EV_ABS, ABS_TILT_X, 0},
      {{0, 0}, EV_KEY, BTN_TOOL_PEN, 0},
      {{0, 0}, EV_ABS, ABS_MISC, 0},
      {{0, 0}, EV_MSC, MSC_SERIAL, 159403517},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
  };

  dev->ProcessEvents(mock_kernel_queue, arraysize(mock_kernel_queue));
  EXPECT_EQ(1u, dev->size());

  ui::MouseEvent* event = dev->event(0);
  EXPECT_EQ(ui::ET_MOUSE_MOVED, event->type());

  EXPECT_LT(cursor()->GetLocation().x(), EPSILON);
  EXPECT_LT(cursor()->GetLocation().y(), EPSILON);
}

TEST_F(TabletEventConverterEvdevTest, MoveTopRight) {
  ui::MockTabletEventConverterEvdev* dev = device();

  struct input_event mock_kernel_queue[] = {
      {{0, 0}, EV_ABS, ABS_X, 65024},
      {{0, 0}, EV_ABS, ABS_Y, 33},
      {{0, 0}, EV_ABS, ABS_DISTANCE, 62},
      {{0, 0}, EV_ABS, ABS_TILT_X, 109},
      {{0, 0}, EV_ABS, ABS_TILT_Y, 59},
      {{0, 0}, EV_ABS, ABS_MISC, 1050626},
      {{0, 0}, EV_KEY, BTN_TOOL_PEN, 1},
      {{0, 0}, EV_MSC, MSC_SERIAL, 159403517},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
      {{0, 0}, EV_ABS, ABS_X, 0},
      {{0, 0}, EV_ABS, ABS_DISTANCE, 0},
      {{0, 0}, EV_ABS, ABS_TILT_X, 0},
      {{0, 0}, EV_ABS, ABS_TILT_Y, 0},
      {{0, 0}, EV_KEY, BTN_TOOL_PEN, 0},
      {{0, 0}, EV_ABS, ABS_MISC, 0},
      {{0, 0}, EV_MSC, MSC_SERIAL, 159403517},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
  };

  dev->ProcessEvents(mock_kernel_queue, arraysize(mock_kernel_queue));
  EXPECT_EQ(1u, dev->size());

  ui::MouseEvent* event = dev->event(0);
  EXPECT_EQ(ui::ET_MOUSE_MOVED, event->type());

  EXPECT_GT(cursor()->GetLocation().x(),
            cursor()->GetCursorDisplayBounds().width() - EPSILON);
  EXPECT_LT(cursor()->GetLocation().y(), EPSILON);
}

TEST_F(TabletEventConverterEvdevTest, MoveBottomLeft) {
  ui::MockTabletEventConverterEvdev* dev = device();

  struct input_event mock_kernel_queue[] = {
      {{0, 0}, EV_ABS, ABS_Y, 40640},
      {{0, 0}, EV_ABS, ABS_DISTANCE, 62},
      {{0, 0}, EV_ABS, ABS_TILT_X, 95},
      {{0, 0}, EV_ABS, ABS_TILT_Y, 44},
      {{0, 0}, EV_ABS, ABS_MISC, 1050626},
      {{0, 0}, EV_KEY, BTN_TOOL_PEN, 1},
      {{0, 0}, EV_MSC, MSC_SERIAL, 159403517},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
      {{0, 0}, EV_ABS, ABS_Y, 0},
      {{0, 0}, EV_ABS, ABS_DISTANCE, 0},
      {{0, 0}, EV_ABS, ABS_TILT_X, 0},
      {{0, 0}, EV_ABS, ABS_TILT_Y, 0},
      {{0, 0}, EV_KEY, BTN_TOOL_PEN, 0},
      {{0, 0}, EV_ABS, ABS_MISC, 0},
      {{0, 0}, EV_MSC, MSC_SERIAL, 159403517},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
  };

  dev->ProcessEvents(mock_kernel_queue, arraysize(mock_kernel_queue));
  EXPECT_EQ(1u, dev->size());

  ui::MouseEvent* event = dev->event(0);
  EXPECT_EQ(ui::ET_MOUSE_MOVED, event->type());

  EXPECT_LT(cursor()->GetLocation().x(), EPSILON);
  EXPECT_GT(cursor()->GetLocation().y(),
            cursor()->GetCursorDisplayBounds().height() - EPSILON);
}

TEST_F(TabletEventConverterEvdevTest, MoveBottomRight) {
  ui::MockTabletEventConverterEvdev* dev = device();

  struct input_event mock_kernel_queue[] = {
      {{0, 0}, EV_ABS, ABS_X, 65024},
      {{0, 0}, EV_ABS, ABS_Y, 40640},
      {{0, 0}, EV_ABS, ABS_DISTANCE, 62},
      {{0, 0}, EV_ABS, ABS_TILT_X, 127},
      {{0, 0}, EV_ABS, ABS_TILT_Y, 89},
      {{0, 0}, EV_ABS, ABS_MISC, 1050626},
      {{0, 0}, EV_KEY, BTN_TOOL_PEN, 1},
      {{0, 0}, EV_MSC, MSC_SERIAL, 159403517},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
      {{0, 0}, EV_ABS, ABS_X, 0},
      {{0, 0}, EV_ABS, ABS_Y, 0},
      {{0, 0}, EV_ABS, ABS_DISTANCE, 0},
      {{0, 0}, EV_ABS, ABS_TILT_X, 0},
      {{0, 0}, EV_ABS, ABS_TILT_Y, 0},
      {{0, 0}, EV_KEY, BTN_TOOL_PEN, 0},
      {{0, 0}, EV_ABS, ABS_MISC, 0},
      {{0, 0}, EV_MSC, MSC_SERIAL, 159403517},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
  };

  dev->ProcessEvents(mock_kernel_queue, arraysize(mock_kernel_queue));
  EXPECT_EQ(1u, dev->size());

  ui::MouseEvent* event = dev->event(0);
  EXPECT_EQ(ui::ET_MOUSE_MOVED, event->type());

  EXPECT_GT(cursor()->GetLocation().x(),
            cursor()->GetCursorDisplayBounds().height() - EPSILON);
  EXPECT_GT(cursor()->GetLocation().y(),
            cursor()->GetCursorDisplayBounds().height() - EPSILON);
}

TEST_F(TabletEventConverterEvdevTest, Tap) {
  ui::MockTabletEventConverterEvdev* dev = device();

  struct input_event mock_kernel_queue[] = {
      {{0, 0}, EV_ABS, ABS_X, 31628},
      {{0, 0}, EV_ABS, ABS_Y, 21670},
      {{0, 0}, EV_ABS, ABS_DISTANCE, 62},
      {{0, 0}, EV_ABS, ABS_TILT_X, 114},
      {{0, 0}, EV_ABS, ABS_TILT_Y, 85},
      {{0, 0}, EV_ABS, ABS_MISC, 1050626},
      {{0, 0}, EV_KEY, BTN_TOOL_PEN, 1},
      {{0, 0}, EV_MSC, MSC_SERIAL, 159403517},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
      {{0, 0}, EV_ABS, ABS_X, 32094},
      {{0, 0}, EV_ABS, ABS_DISTANCE, 17},
      {{0, 0}, EV_ABS, ABS_PRESSURE, 883},
      {{0, 0}, EV_ABS, ABS_TILT_Y, 68},
      {{0, 0}, EV_KEY, BTN_TOUCH, 1},
      {{0, 0}, EV_MSC, MSC_SERIAL, 159403517},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
      {{0, 0}, EV_ABS, ABS_X, 32036},
      {{0, 0}, EV_ABS, ABS_Y, 21658},
      {{0, 0}, EV_ABS, ABS_DISTANCE, 19},
      {{0, 0}, EV_ABS, ABS_PRESSURE, 0},
      {{0, 0}, EV_KEY, BTN_TOUCH, 0},
      {{0, 0}, EV_MSC, MSC_SERIAL, 159403517},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
      {{0, 0}, EV_ABS, ABS_X, 0},
      {{0, 0}, EV_ABS, ABS_Y, 0},
      {{0, 0}, EV_ABS, ABS_DISTANCE, 0},
      {{0, 0}, EV_ABS, ABS_TILT_X, 0},
      {{0, 0}, EV_ABS, ABS_TILT_Y, 0},
      {{0, 0}, EV_KEY, BTN_TOOL_PEN, 0},
      {{0, 0}, EV_ABS, ABS_MISC, 0},
      {{0, 0}, EV_MSC, MSC_SERIAL, 159403517},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
  };

  dev->ProcessEvents(mock_kernel_queue, arraysize(mock_kernel_queue));
  EXPECT_EQ(3u, dev->size());

  ui::MouseEvent* event = dev->event(0);
  EXPECT_EQ(ui::ET_MOUSE_MOVED, event->type());
  event = dev->event(1);
  EXPECT_EQ(ui::ET_MOUSE_PRESSED, event->type());
  EXPECT_EQ(true, event->IsLeftMouseButton());
  event = dev->event(2);
  EXPECT_EQ(ui::ET_MOUSE_RELEASED, event->type());
  EXPECT_EQ(true, event->IsLeftMouseButton());
}

TEST_F(TabletEventConverterEvdevTest, StylusButtonPress) {
  ui::MockTabletEventConverterEvdev* dev = device();

  struct input_event mock_kernel_queue[] = {
      {{0, 0}, EV_ABS, ABS_X, 30055},
      {{0, 0}, EV_ABS, ABS_Y, 18094},
      {{0, 0}, EV_ABS, ABS_DISTANCE, 62},
      {{0, 0}, EV_ABS, ABS_TILT_X, 99},
      {{0, 0}, EV_ABS, ABS_TILT_Y, 68},
      {{0, 0}, EV_ABS, ABS_MISC, 1050626},
      {{0, 0}, EV_KEY, BTN_TOOL_PEN, 1},
      {{0, 0}, EV_MSC, MSC_SERIAL, 159403517},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
      {{0, 0}, EV_ABS, ABS_X, 29380},
      {{0, 0}, EV_KEY, BTN_STYLUS2, 1},
      {{0, 0}, EV_MSC, MSC_SERIAL, 159403517},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
      {{0, 0}, EV_ABS, ABS_X, 29355},
      {{0, 0}, EV_ABS, ABS_Y, 20091},
      {{0, 0}, EV_ABS, ABS_DISTANCE, 34},
      {{0, 0}, EV_KEY, BTN_STYLUS2, 0},
      {{0, 0}, EV_MSC, MSC_SERIAL, 159403517},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
      {{0, 0}, EV_ABS, ABS_X, 0},
      {{0, 0}, EV_ABS, ABS_Y, 0},
      {{0, 0}, EV_ABS, ABS_DISTANCE, 0},
      {{0, 0}, EV_ABS, ABS_TILT_X, 0},
      {{0, 0}, EV_ABS, ABS_TILT_Y, 0},
      {{0, 0}, EV_KEY, BTN_TOOL_PEN, 0},
      {{0, 0}, EV_ABS, ABS_MISC, 0},
      {{0, 0}, EV_MSC, MSC_SERIAL, 159403517},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
  };

  dev->ProcessEvents(mock_kernel_queue, arraysize(mock_kernel_queue));
  EXPECT_EQ(3u, dev->size());

  ui::MouseEvent* event = dev->event(0);
  EXPECT_EQ(ui::ET_MOUSE_MOVED, event->type());
  event = dev->event(1);
  EXPECT_EQ(ui::ET_MOUSE_PRESSED, event->type());
  EXPECT_EQ(true, event->IsRightMouseButton());
  event = dev->event(2);
  EXPECT_EQ(ui::ET_MOUSE_RELEASED, event->type());
  EXPECT_EQ(true, event->IsRightMouseButton());
}

// Should only get an event if BTN_TOOL received
TEST_F(TabletEventConverterEvdevTest, CheckStylusFiltering) {
  ui::MockTabletEventConverterEvdev* dev = device();

  struct input_event mock_kernel_queue[] = {
      {{0, 0}, EV_ABS, ABS_X, 0},
      {{0, 0}, EV_ABS, ABS_Y, 0},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
  };

  dev->ProcessEvents(mock_kernel_queue, arraysize(mock_kernel_queue));
  EXPECT_EQ(0u, dev->size());
}
