// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/mouse_shape_pump.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "remoting/base/auto_thread.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/host/host_mock_objects.h"
#include "remoting/proto/control.pb.h"
#include "remoting/proto/video.pb.h"
#include "remoting/protocol/protocol_mock_objects.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor_monitor.h"

using ::remoting::protocol::MockClientStub;

using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::InvokeWithoutArgs;

namespace remoting {

static const int kCursorWidth = 64;
static const int kCursorHeight = 32;
static const int kHotspotX = 11;
static const int kHotspotY = 12;

class ThreadCheckMouseCursorMonitor : public webrtc::MouseCursorMonitor  {
 public:
  ThreadCheckMouseCursorMonitor(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : task_runner_(task_runner), callback_(nullptr) {
  }
  ~ThreadCheckMouseCursorMonitor() override {
    EXPECT_TRUE(task_runner_->BelongsToCurrentThread());
  }

  void Init(Callback* callback, Mode mode) override {
    EXPECT_TRUE(task_runner_->BelongsToCurrentThread());
    EXPECT_FALSE(callback_);
    EXPECT_TRUE(callback);

    callback_ = callback;
  }

  void Capture() override {
    EXPECT_TRUE(task_runner_->BelongsToCurrentThread());
    ASSERT_TRUE(callback_);

    scoped_ptr<webrtc::MouseCursor> mouse_cursor(new webrtc::MouseCursor(
        new webrtc::BasicDesktopFrame(
            webrtc::DesktopSize(kCursorWidth, kCursorHeight)),
        webrtc::DesktopVector(kHotspotX, kHotspotY)));

    callback_->OnMouseCursor(mouse_cursor.release());
  }

 private:
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  Callback* callback_;

  DISALLOW_COPY_AND_ASSIGN(ThreadCheckMouseCursorMonitor);
};

class MouseShapePumpTest : public testing::Test {
 public:
  void SetUp() override;
  void TearDown() override;

  void SetCursorShape(const protocol::CursorShapeInfo& cursor_shape);

 protected:
  base::MessageLoop message_loop_;
  base::RunLoop run_loop_;
  scoped_refptr<AutoThreadTaskRunner> capture_task_runner_;
  scoped_refptr<AutoThreadTaskRunner> main_task_runner_;
  scoped_ptr<MouseShapePump> pump_;

  MockClientStub client_stub_;
};

void MouseShapePumpTest::SetUp() {
  main_task_runner_ = new AutoThreadTaskRunner(
      message_loop_.task_runner(), run_loop_.QuitClosure());
  capture_task_runner_ = AutoThread::Create("capture", main_task_runner_);
}

void MouseShapePumpTest::TearDown() {
  pump_.reset();

  // Release the task runners, so that the test can quit.
  capture_task_runner_ = nullptr;
  main_task_runner_ = nullptr;

  // Run the MessageLoop until everything has torn down.
  run_loop_.Run();
}

void MouseShapePumpTest::SetCursorShape(
    const protocol::CursorShapeInfo& cursor_shape) {
  EXPECT_TRUE(cursor_shape.has_width());
  EXPECT_EQ(kCursorWidth, cursor_shape.width());
  EXPECT_TRUE(cursor_shape.has_height());
  EXPECT_EQ(kCursorHeight, cursor_shape.height());
  EXPECT_TRUE(cursor_shape.has_hotspot_x());
  EXPECT_EQ(kHotspotX, cursor_shape.hotspot_x());
  EXPECT_TRUE(cursor_shape.has_hotspot_y());
  EXPECT_EQ(kHotspotY, cursor_shape.hotspot_y());
  EXPECT_TRUE(cursor_shape.has_data());
  EXPECT_EQ(kCursorWidth * kCursorHeight * webrtc::DesktopFrame::kBytesPerPixel,
            static_cast<int>(cursor_shape.data().size()));
}

// This test mocks MouseCursorMonitor and ClientStub to verify that the
// MouseShapePump sends the cursor successfully.
TEST_F(MouseShapePumpTest, FirstCursor) {
  scoped_ptr<ThreadCheckMouseCursorMonitor> cursor_monitor(
      new ThreadCheckMouseCursorMonitor(capture_task_runner_));

  base::RunLoop run_loop;

  // Stop the |run_loop| once it has captured the cursor.
  EXPECT_CALL(client_stub_, SetCursorShape(_))
      .WillOnce(DoAll(
          Invoke(this, &MouseShapePumpTest::SetCursorShape),
          InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit)))
      .RetiresOnSaturation();

  // Start the pump.
  pump_.reset(new MouseShapePump(capture_task_runner_, cursor_monitor.Pass(),
                                 &client_stub_));

  run_loop.Run();
}

}  // namespace remoting
