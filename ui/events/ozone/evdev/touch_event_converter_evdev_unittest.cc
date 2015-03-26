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
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/ozone/evdev/device_event_dispatcher_evdev.h"
#include "ui/events/ozone/evdev/touch_event_converter_evdev.h"
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

class MockTouchEventConverterEvdev : public TouchEventConverterEvdev {
 public:
  MockTouchEventConverterEvdev(int fd,
                               base::FilePath path,
                               DeviceEventDispatcherEvdev* dispatcher);
  ~MockTouchEventConverterEvdev() override {}

  void ConfigureReadMock(struct input_event* queue,
                         long read_this_many,
                         long queue_index);

  // Actually dispatch the event reader code.
  void ReadNow() {
    OnFileCanReadWithoutBlocking(read_pipe_);
    base::RunLoop().RunUntilIdle();
  }

  void Initialize(const EventDeviceInfo& device_info) override {}
  bool Reinitialize() override { return true; }

 private:
  int read_pipe_;
  int write_pipe_;

  DISALLOW_COPY_AND_ASSIGN(MockTouchEventConverterEvdev);
};

class MockDeviceEventDispatcherEvdev : public DeviceEventDispatcherEvdev {
 public:
  MockDeviceEventDispatcherEvdev(
      const base::Callback<void(const TouchEventParams& params)>& callback)
      : callback_(callback) {}
  ~MockDeviceEventDispatcherEvdev() override {}

  // DeviceEventDispatcherEvdev:
  void DispatchKeyEvent(const KeyEventParams& params) override {}
  void DispatchMouseMoveEvent(const MouseMoveEventParams& params) override {}
  void DispatchMouseButtonEvent(const MouseButtonEventParams& params) override {
  }
  void DispatchMouseWheelEvent(const MouseWheelEventParams& params) override {}
  void DispatchScrollEvent(const ScrollEventParams& params) override {}
  void DispatchTouchEvent(const TouchEventParams& params) override {
    callback_.Run(params);
  }

  void DispatchKeyboardDevicesUpdated(
      const std::vector<KeyboardDevice>& devices) override {}
  void DispatchTouchscreenDevicesUpdated(
      const std::vector<TouchscreenDevice>& devices) override {}
  void DispatchMouseDevicesUpdated(
      const std::vector<InputDevice>& devices) override {}
  void DispatchTouchpadDevicesUpdated(
      const std::vector<InputDevice>& devices) override {}

 private:
  base::Callback<void(const TouchEventParams& params)> callback_;
};

MockTouchEventConverterEvdev::MockTouchEventConverterEvdev(
    int fd,
    base::FilePath path,
    DeviceEventDispatcherEvdev* dispatcher)
    : TouchEventConverterEvdev(fd, path, 1, INPUT_DEVICE_UNKNOWN, dispatcher) {
  pressure_min_ = 30;
  pressure_max_ = 60;

  // TODO(rjkroege): Check test axes.
  x_min_tuxels_ = 0;
  x_num_tuxels_ = std::numeric_limits<int>::max();
  y_min_tuxels_ = 0;
  y_num_tuxels_ = std::numeric_limits<int>::max();

  int fds[2];

  if (pipe(fds))
    PLOG(FATAL) << "failed pipe";

  EXPECT_FALSE(SetNonBlocking(fds[0]) || SetNonBlocking(fds[1]))
    << "failed to set non-blocking: " << strerror(errno);

  read_pipe_ = fds[0];
  write_pipe_ = fds[1];

  events_.resize(MAX_FINGERS);
}

void MockTouchEventConverterEvdev::ConfigureReadMock(struct input_event* queue,
                                                     long read_this_many,
                                                     long queue_index) {
  int nwrite = HANDLE_EINTR(write(write_pipe_,
                                  queue + queue_index,
                                  sizeof(struct input_event) * read_this_many));
  DCHECK(nwrite ==
         static_cast<int>(sizeof(struct input_event) * read_this_many))
      << "write() failed, errno: " << errno;
}

}  // namespace ui

// Test fixture.
class TouchEventConverterEvdevTest : public testing::Test {
 public:
  TouchEventConverterEvdevTest() {}

  // Overridden from testing::Test:
  void SetUp() override {
    // Set up pipe to satisfy message pump (unused).
    int evdev_io[2];
    if (pipe(evdev_io))
      PLOG(FATAL) << "failed pipe";
    events_in_ = evdev_io[0];
    events_out_ = evdev_io[1];

    // Device creation happens on a worker thread since it may involve blocking
    // operations. Simulate that by creating it before creating a UI message
    // loop.
    dispatcher_.reset(new ui::MockDeviceEventDispatcherEvdev(
        base::Bind(&TouchEventConverterEvdevTest::DispatchCallback,
                   base::Unretained(this))));
    device_ = new ui::MockTouchEventConverterEvdev(
        events_in_, base::FilePath(kTestDevicePath), dispatcher_.get());
    loop_ = new base::MessageLoopForUI;

    ui::DeviceDataManager::CreateInstance();
  }

  void TearDown() override {
    delete device_;
    delete loop_;
  }

  ui::MockTouchEventConverterEvdev* device() { return device_; }

  unsigned size() { return dispatched_events_.size(); }
  const ui::TouchEventParams& dispatched_event(unsigned index) {
    DCHECK_GT(dispatched_events_.size(), index);
    return dispatched_events_[index];
  }

 private:
  base::MessageLoop* loop_;
  ui::MockTouchEventConverterEvdev* device_;
  scoped_ptr<ui::MockDeviceEventDispatcherEvdev> dispatcher_;

  int events_out_;
  int events_in_;

  void DispatchCallback(const ui::TouchEventParams& params) {
    dispatched_events_.push_back(params);
  }
  std::vector<ui::TouchEventParams> dispatched_events_;

  DISALLOW_COPY_AND_ASSIGN(TouchEventConverterEvdevTest);
};

// TODO(rjkroege): Test for valid handling of time stamps.
TEST_F(TouchEventConverterEvdevTest, TouchDown) {
  ui::MockTouchEventConverterEvdev* dev = device();

  struct input_event mock_kernel_queue[] = {
    {{0, 0}, EV_ABS, ABS_MT_TRACKING_ID, 684},
    {{0, 0}, EV_ABS, ABS_MT_TOUCH_MAJOR, 3},
    {{0, 0}, EV_ABS, ABS_MT_PRESSURE, 45},
    {{0, 0}, EV_ABS, ABS_MT_POSITION_X, 42},
    {{0, 0}, EV_ABS, ABS_MT_POSITION_Y, 51}, {{0, 0}, EV_SYN, SYN_REPORT, 0}
  };

  dev->ConfigureReadMock(mock_kernel_queue, 1, 0);
  dev->ReadNow();
  EXPECT_EQ(0u, size());

  dev->ConfigureReadMock(mock_kernel_queue, 2, 1);
  dev->ReadNow();
  EXPECT_EQ(0u, size());

  dev->ConfigureReadMock(mock_kernel_queue, 3, 3);
  dev->ReadNow();
  EXPECT_EQ(1u, size());

  ui::TouchEventParams event = dispatched_event(0);

  EXPECT_EQ(ui::ET_TOUCH_PRESSED, event.type);
  EXPECT_EQ(base::TimeDelta::FromMicroseconds(0), event.timestamp);
  EXPECT_EQ(42, event.location.x());
  EXPECT_EQ(51, event.location.y());
  EXPECT_EQ(0, event.touch_id);
  EXPECT_FLOAT_EQ(1.5f, event.radii.x());
  EXPECT_FLOAT_EQ(.5f, event.pressure);
}

TEST_F(TouchEventConverterEvdevTest, NoEvents) {
  ui::MockTouchEventConverterEvdev* dev = device();
  dev->ConfigureReadMock(NULL, 0, 0);
  EXPECT_EQ(0u, size());
}

TEST_F(TouchEventConverterEvdevTest, TouchMove) {
  ui::MockTouchEventConverterEvdev* dev = device();

  struct input_event mock_kernel_queue_press[] = {
    {{0, 0}, EV_ABS, ABS_MT_TRACKING_ID, 684},
    {{0, 0}, EV_ABS, ABS_MT_TOUCH_MAJOR, 3},
    {{0, 0}, EV_ABS, ABS_MT_PRESSURE, 45},
    {{0, 0}, EV_ABS, ABS_MT_POSITION_X, 42},
    {{0, 0}, EV_ABS, ABS_MT_POSITION_Y, 51}, {{0, 0}, EV_SYN, SYN_REPORT, 0}
  };

  struct input_event mock_kernel_queue_move1[] = {
    {{0, 0}, EV_ABS, ABS_MT_PRESSURE, 50},
    {{0, 0}, EV_ABS, ABS_MT_POSITION_X, 42},
    {{0, 0}, EV_ABS, ABS_MT_POSITION_Y, 43}, {{0, 0}, EV_SYN, SYN_REPORT, 0}
  };

  struct input_event mock_kernel_queue_move2[] = {
    {{0, 0}, EV_ABS, ABS_MT_POSITION_Y, 42}, {{0, 0}, EV_SYN, SYN_REPORT, 0}
  };

  // Setup and discard a press.
  dev->ConfigureReadMock(mock_kernel_queue_press, 6, 0);
  dev->ReadNow();
  EXPECT_EQ(1u, size());

  dev->ConfigureReadMock(mock_kernel_queue_move1, 4, 0);
  dev->ReadNow();
  EXPECT_EQ(2u, size());
  ui::TouchEventParams event = dispatched_event(1);

  EXPECT_EQ(ui::ET_TOUCH_MOVED, event.type);
  EXPECT_EQ(base::TimeDelta::FromMicroseconds(0), event.timestamp);
  EXPECT_EQ(42, event.location.x());
  EXPECT_EQ(43, event.location.y());
  EXPECT_EQ(0, event.touch_id);
  EXPECT_FLOAT_EQ(2.f / 3.f, event.pressure);

  dev->ConfigureReadMock(mock_kernel_queue_move2, 2, 0);
  dev->ReadNow();
  EXPECT_EQ(3u, size());
  event = dispatched_event(2);

  EXPECT_EQ(ui::ET_TOUCH_MOVED, event.type);
  EXPECT_EQ(base::TimeDelta::FromMicroseconds(0), event.timestamp);
  EXPECT_EQ(42, event.location.x());
  EXPECT_EQ(42, event.location.y());
  EXPECT_EQ(0, event.touch_id);
  EXPECT_FLOAT_EQ(2.f / 3.f, event.pressure);
}

TEST_F(TouchEventConverterEvdevTest, TouchRelease) {
  ui::MockTouchEventConverterEvdev* dev = device();

  struct input_event mock_kernel_queue_press[] = {
    {{0, 0}, EV_ABS, ABS_MT_TRACKING_ID, 684},
    {{0, 0}, EV_ABS, ABS_MT_TOUCH_MAJOR, 3},
    {{0, 0}, EV_ABS, ABS_MT_PRESSURE, 45},
    {{0, 0}, EV_ABS, ABS_MT_POSITION_X, 42},
    {{0, 0}, EV_ABS, ABS_MT_POSITION_Y, 51}, {{0, 0}, EV_SYN, SYN_REPORT, 0}
  };

  struct input_event mock_kernel_queue_release[] = {
    {{0, 0}, EV_ABS, ABS_MT_TRACKING_ID, -1}, {{0, 0}, EV_SYN, SYN_REPORT, 0}
  };

  // Setup and discard a press.
  dev->ConfigureReadMock(mock_kernel_queue_press, 6, 0);
  dev->ReadNow();
  EXPECT_EQ(1u, size());
  ui::TouchEventParams event = dispatched_event(0);

  dev->ConfigureReadMock(mock_kernel_queue_release, 2, 0);
  dev->ReadNow();
  EXPECT_EQ(2u, size());
  event = dispatched_event(1);

  EXPECT_EQ(ui::ET_TOUCH_RELEASED, event.type);
  EXPECT_EQ(base::TimeDelta::FromMicroseconds(0), event.timestamp);
  EXPECT_EQ(42, event.location.x());
  EXPECT_EQ(51, event.location.y());
  EXPECT_EQ(0, event.touch_id);
  EXPECT_FLOAT_EQ(.5f, event.pressure);
}

TEST_F(TouchEventConverterEvdevTest, TwoFingerGesture) {
  ui::MockTouchEventConverterEvdev* dev = device();

  struct input_event mock_kernel_queue_press0[] = {
    {{0, 0}, EV_ABS, ABS_MT_TRACKING_ID, 684},
    {{0, 0}, EV_ABS, ABS_MT_TOUCH_MAJOR, 3},
    {{0, 0}, EV_ABS, ABS_MT_PRESSURE, 45},
    {{0, 0}, EV_ABS, ABS_MT_POSITION_X, 42},
    {{0, 0}, EV_ABS, ABS_MT_POSITION_Y, 51}, {{0, 0}, EV_SYN, SYN_REPORT, 0}
  };
  // Setup and discard a press.
  dev->ConfigureReadMock(mock_kernel_queue_press0, 6, 0);
  dev->ReadNow();
  EXPECT_EQ(1u, size());

  struct input_event mock_kernel_queue_move0[] = {
    {{0, 0}, EV_ABS, ABS_MT_POSITION_X, 40}, {{0, 0}, EV_SYN, SYN_REPORT, 0}
  };
  // Setup and discard a move.
  dev->ConfigureReadMock(mock_kernel_queue_move0, 2, 0);
  dev->ReadNow();
  EXPECT_EQ(2u, size());

  struct input_event mock_kernel_queue_move0press1[] = {
    {{0, 0}, EV_ABS, ABS_MT_POSITION_X, 40}, {{0, 0}, EV_SYN, SYN_REPORT, 0},
    {{0, 0}, EV_ABS, ABS_MT_SLOT, 1}, {{0, 0}, EV_ABS, ABS_MT_TRACKING_ID, 686},
    {{0, 0}, EV_ABS, ABS_MT_TOUCH_MAJOR, 3},
    {{0, 0}, EV_ABS, ABS_MT_PRESSURE, 45},
    {{0, 0}, EV_ABS, ABS_MT_POSITION_X, 101},
    {{0, 0}, EV_ABS, ABS_MT_POSITION_Y, 102}, {{0, 0}, EV_SYN, SYN_REPORT, 0}
  };
  // Move on 0, press on 1.
  dev->ConfigureReadMock(mock_kernel_queue_move0press1, 9, 0);
  dev->ReadNow();
  EXPECT_EQ(4u, size());
  ui::TouchEventParams ev0 = dispatched_event(2);
  ui::TouchEventParams ev1 = dispatched_event(3);

  // Move
  EXPECT_EQ(ui::ET_TOUCH_MOVED, ev0.type);
  EXPECT_EQ(base::TimeDelta::FromMicroseconds(0), ev0.timestamp);
  EXPECT_EQ(40, ev0.location.x());
  EXPECT_EQ(51, ev0.location.y());
  EXPECT_EQ(0, ev0.touch_id);
  EXPECT_FLOAT_EQ(.5f, ev0.pressure);

  // Press
  EXPECT_EQ(ui::ET_TOUCH_PRESSED, ev1.type);
  EXPECT_EQ(base::TimeDelta::FromMicroseconds(0), ev1.timestamp);
  EXPECT_EQ(101, ev1.location.x());
  EXPECT_EQ(102, ev1.location.y());
  EXPECT_EQ(1, ev1.touch_id);
  EXPECT_FLOAT_EQ(.5f, ev1.pressure);

  // Stationary 0, Moves 1.
  struct input_event mock_kernel_queue_stationary0_move1[] = {
    {{0, 0}, EV_ABS, ABS_MT_POSITION_X, 40}, {{0, 0}, EV_SYN, SYN_REPORT, 0}
  };
  dev->ConfigureReadMock(mock_kernel_queue_stationary0_move1, 2, 0);
  dev->ReadNow();
  EXPECT_EQ(5u, size());
  ev1 = dispatched_event(4);

  EXPECT_EQ(ui::ET_TOUCH_MOVED, ev1.type);
  EXPECT_EQ(base::TimeDelta::FromMicroseconds(0), ev1.timestamp);
  EXPECT_EQ(40, ev1.location.x());
  EXPECT_EQ(102, ev1.location.y());
  EXPECT_EQ(1, ev1.touch_id);

  EXPECT_FLOAT_EQ(.5f, ev1.pressure);

  // Move 0, stationary 1.
  struct input_event mock_kernel_queue_move0_stationary1[] = {
    {{0, 0}, EV_ABS, ABS_MT_SLOT, 0}, {{0, 0}, EV_ABS, ABS_MT_POSITION_X, 39},
    {{0, 0}, EV_SYN, SYN_REPORT, 0}
  };
  dev->ConfigureReadMock(mock_kernel_queue_move0_stationary1, 3, 0);
  dev->ReadNow();
  EXPECT_EQ(6u, size());
  ev0 = dispatched_event(5);

  EXPECT_EQ(ui::ET_TOUCH_MOVED, ev0.type);
  EXPECT_EQ(base::TimeDelta::FromMicroseconds(0), ev0.timestamp);
  EXPECT_EQ(39, ev0.location.x());
  EXPECT_EQ(51, ev0.location.y());
  EXPECT_EQ(0, ev0.touch_id);
  EXPECT_FLOAT_EQ(.5f, ev0.pressure);

  // Release 0, move 1.
  struct input_event mock_kernel_queue_release0_move1[] = {
    {{0, 0}, EV_ABS, ABS_MT_TRACKING_ID, -1}, {{0, 0}, EV_ABS, ABS_MT_SLOT, 1},
    {{0, 0}, EV_ABS, ABS_MT_POSITION_X, 38}, {{0, 0}, EV_SYN, SYN_REPORT, 0}
  };
  dev->ConfigureReadMock(mock_kernel_queue_release0_move1, 4, 0);
  dev->ReadNow();
  EXPECT_EQ(8u, size());
  ev0 = dispatched_event(6);
  ev1 = dispatched_event(7);

  EXPECT_EQ(ui::ET_TOUCH_RELEASED, ev0.type);
  EXPECT_EQ(base::TimeDelta::FromMicroseconds(0), ev0.timestamp);
  EXPECT_EQ(39, ev0.location.x());
  EXPECT_EQ(51, ev0.location.y());
  EXPECT_EQ(0, ev0.touch_id);
  EXPECT_FLOAT_EQ(.5f, ev0.pressure);

  EXPECT_EQ(ui::ET_TOUCH_MOVED, ev1.type);
  EXPECT_EQ(base::TimeDelta::FromMicroseconds(0), ev1.timestamp);
  EXPECT_EQ(38, ev1.location.x());
  EXPECT_EQ(102, ev1.location.y());
  EXPECT_EQ(1, ev1.touch_id);
  EXPECT_FLOAT_EQ(.5f, ev1.pressure);

  // Release 1.
  struct input_event mock_kernel_queue_release1[] = {
    {{0, 0}, EV_ABS, ABS_MT_TRACKING_ID, -1}, {{0, 0}, EV_SYN, SYN_REPORT, 0},
  };
  dev->ConfigureReadMock(mock_kernel_queue_release1, 2, 0);
  dev->ReadNow();
  EXPECT_EQ(9u, size());
  ev1 = dispatched_event(8);

  EXPECT_EQ(ui::ET_TOUCH_RELEASED, ev1.type);
  EXPECT_EQ(base::TimeDelta::FromMicroseconds(0), ev1.timestamp);
  EXPECT_EQ(38, ev1.location.x());
  EXPECT_EQ(102, ev1.location.y());
  EXPECT_EQ(1, ev1.touch_id);
  EXPECT_FLOAT_EQ(.5f, ev1.pressure);
}

TEST_F(TouchEventConverterEvdevTest, TypeA) {
  ui::MockTouchEventConverterEvdev* dev = device();

  struct input_event mock_kernel_queue_press0[] = {
    {{0, 0}, EV_ABS, ABS_MT_TOUCH_MAJOR, 3},
    {{0, 0}, EV_ABS, ABS_MT_PRESSURE, 45},
    {{0, 0}, EV_ABS, ABS_MT_POSITION_X, 42},
    {{0, 0}, EV_ABS, ABS_MT_POSITION_Y, 51},
    {{0, 0}, EV_SYN, SYN_MT_REPORT, 0},
    {{0, 0}, EV_ABS, ABS_MT_PRESSURE, 45},
    {{0, 0}, EV_ABS, ABS_MT_POSITION_X, 61},
    {{0, 0}, EV_ABS, ABS_MT_POSITION_Y, 71},
    {{0, 0}, EV_SYN, SYN_MT_REPORT, 0},
    {{0, 0}, EV_SYN, SYN_REPORT, 0}
  };

  // Check that two events are generated.
  dev->ConfigureReadMock(mock_kernel_queue_press0, 10, 0);
  dev->ReadNow();
  EXPECT_EQ(2u, size());
}

TEST_F(TouchEventConverterEvdevTest, Unsync) {
  ui::MockTouchEventConverterEvdev* dev = device();

  struct input_event mock_kernel_queue_press0[] = {
    {{0, 0}, EV_ABS, ABS_MT_TRACKING_ID, 684},
    {{0, 0}, EV_ABS, ABS_MT_TOUCH_MAJOR, 3},
    {{0, 0}, EV_ABS, ABS_MT_PRESSURE, 45},
    {{0, 0}, EV_ABS, ABS_MT_POSITION_X, 42},
    {{0, 0}, EV_ABS, ABS_MT_POSITION_Y, 51}, {{0, 0}, EV_SYN, SYN_REPORT, 0}
  };

  dev->ConfigureReadMock(mock_kernel_queue_press0, 6, 0);
  dev->ReadNow();
  EXPECT_EQ(1u, size());

  // Prepare a move with a drop.
  struct input_event mock_kernel_queue_move0[] = {
    {{0, 0}, EV_SYN, SYN_DROPPED, 0},
    {{0, 0}, EV_ABS, ABS_MT_POSITION_X, 40}, {{0, 0}, EV_SYN, SYN_REPORT, 0}
  };

  // Verify that we didn't receive it/
  dev->ConfigureReadMock(mock_kernel_queue_move0, 3, 0);
  dev->ReadNow();
  EXPECT_EQ(1u, size());

  struct input_event mock_kernel_queue_move1[] = {
    {{0, 0}, EV_ABS, ABS_MT_POSITION_X, 40}, {{0, 0}, EV_SYN, SYN_REPORT, 0}
  };

  // Verify that it re-syncs after a SYN_REPORT.
  dev->ConfigureReadMock(mock_kernel_queue_move1, 2, 0);
  dev->ReadNow();
  EXPECT_EQ(2u, size());
}

// crbug.com/407386
TEST_F(TouchEventConverterEvdevTest,
       DontChangeMultitouchPositionFromLegacyAxes) {
  ui::MockTouchEventConverterEvdev* dev = device();

  struct input_event mock_kernel_queue[] = {
      {{0, 0}, EV_ABS, ABS_MT_SLOT, 0},
      {{0, 0}, EV_ABS, ABS_MT_TRACKING_ID, 100},
      {{0, 0}, EV_ABS, ABS_MT_POSITION_X, 999},
      {{0, 0}, EV_ABS, ABS_MT_POSITION_Y, 888},
      {{0, 0}, EV_ABS, ABS_MT_PRESSURE, 55},
      {{0, 0}, EV_ABS, ABS_MT_SLOT, 1},
      {{0, 0}, EV_ABS, ABS_MT_TRACKING_ID, 200},
      {{0, 0}, EV_ABS, ABS_MT_PRESSURE, 44},
      {{0, 0}, EV_ABS, ABS_MT_POSITION_X, 777},
      {{0, 0}, EV_ABS, ABS_MT_POSITION_Y, 666},
      {{0, 0}, EV_ABS, ABS_X, 999},
      {{0, 0}, EV_ABS, ABS_Y, 888},
      {{0, 0}, EV_ABS, ABS_PRESSURE, 55},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
  };

  // Check that two events are generated.
  dev->ConfigureReadMock(mock_kernel_queue, arraysize(mock_kernel_queue), 0);
  dev->ReadNow();

  const unsigned int kExpectedEventCount = 2;
  EXPECT_EQ(kExpectedEventCount, size());
  if (kExpectedEventCount != size())
    return;

  ui::TouchEventParams ev0 = dispatched_event(0);
  ui::TouchEventParams ev1 = dispatched_event(1);

  EXPECT_EQ(0, ev0.touch_id);
  EXPECT_EQ(999, ev0.location.x());
  EXPECT_EQ(888, ev0.location.y());
  EXPECT_FLOAT_EQ(0.8333333f, ev0.pressure);

  EXPECT_EQ(1, ev1.touch_id);
  EXPECT_EQ(777, ev1.location.x());
  EXPECT_EQ(666, ev1.location.y());
  EXPECT_FLOAT_EQ(0.4666666f, ev1.pressure);
}

// crbug.com/446939
TEST_F(TouchEventConverterEvdevTest, CheckSlotLimit) {
  ui::MockTouchEventConverterEvdev* dev = device();

  struct input_event mock_kernel_queue[] = {
      {{0, 0}, EV_ABS, ABS_MT_SLOT, 0},
      {{0, 0}, EV_ABS, ABS_MT_TRACKING_ID, 100},
      {{0, 0}, EV_ABS, ABS_MT_POSITION_X, 999},
      {{0, 0}, EV_ABS, ABS_MT_POSITION_Y, 888},
      {{0, 0}, EV_ABS, ABS_MT_SLOT, ui::TouchEventConverterEvdev::MAX_FINGERS},
      {{0, 0}, EV_ABS, ABS_MT_TRACKING_ID, 200},
      {{0, 0}, EV_ABS, ABS_MT_POSITION_X, 777},
      {{0, 0}, EV_ABS, ABS_MT_POSITION_Y, 666},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
  };

  // Check that one 1 event is generated
  dev->ConfigureReadMock(mock_kernel_queue, arraysize(mock_kernel_queue), 0);
  dev->ReadNow();
  EXPECT_EQ(1u, size());
}
