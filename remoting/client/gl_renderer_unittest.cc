// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/gl_renderer.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "remoting/client/gl_renderer_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

namespace remoting {

class FakeGlRendererDelegate : public GlRendererDelegate {
 public:
  FakeGlRendererDelegate() : weak_factory_(this) {}

  bool CanRenderFrame() override {
    can_render_frame_call_count_++;
    return can_render_frame_;
  }

  void OnFrameRendered() override {
    on_frame_rendered_call_count_++;
    if (on_frame_rendered_callback_) {
      on_frame_rendered_callback_.Run();
    }
  }

  void OnSizeChanged(int width, int height) override {
    canvas_width_ = width;
    canvas_height_ = height;
    on_size_changed_call_count_++;
  }

  void SetOnFrameRenderedCallback(const base::Closure& callback) {
    on_frame_rendered_callback_ = callback;
  }

  int canvas_width() {
    return canvas_width_;
  }

  int canvas_height() {
    return canvas_height_;
  }

  base::WeakPtr<FakeGlRendererDelegate> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  int can_render_frame_call_count() {
    return can_render_frame_call_count_;
  }

  int on_frame_rendered_call_count() {
    return on_frame_rendered_call_count_;
  }

  int on_size_changed_call_count() {
    return on_size_changed_call_count_;
  }

  bool can_render_frame_ = false;

 private:
  int can_render_frame_call_count_ = 0;
  int on_frame_rendered_call_count_ = 0;
  int on_size_changed_call_count_ = 0;

  int canvas_width_ = 0;
  int canvas_height_ = 0;

  base::Closure on_frame_rendered_callback_;

  base::WeakPtrFactory<FakeGlRendererDelegate> weak_factory_;
};

class GlRendererTest : public testing::Test {
 public:
  void SetUp() override;
  void SetDesktopFrameWithSize(const webrtc::DesktopSize& size);
  void PostSetDesktopFrameTasks(const webrtc::DesktopSize& size, int count);

 protected:
  void RequestRender();
  void OnDesktopFrameProcessed();
  void RunTasksInCurrentQueue();
  void RunUntilRendered();
  int on_desktop_frame_processed_call_count() {
    return on_desktop_frame_processed_call_count_;
  }

  base::MessageLoop message_loop_;
  std::unique_ptr<GlRenderer> renderer_;
  FakeGlRendererDelegate delegate_;

 private:
  int on_desktop_frame_processed_call_count_ = 0;
};

void GlRendererTest::SetUp() {
  renderer_.reset(new GlRenderer());
  renderer_->SetDelegate(delegate_.GetWeakPtr());
}

void GlRendererTest::RequestRender() {
  renderer_->RequestRender();
}

void GlRendererTest::SetDesktopFrameWithSize(const webrtc::DesktopSize& size) {
  renderer_->OnFrameReceived(
      base::MakeUnique<webrtc::BasicDesktopFrame>(size),
      base::Bind(&GlRendererTest::OnDesktopFrameProcessed,
                 base::Unretained(this)));
}

void GlRendererTest::PostSetDesktopFrameTasks(
    const webrtc::DesktopSize& size, int count) {
  for (int i = 0; i < count; i++) {
    message_loop_.task_runner()->PostTask(
        FROM_HERE, base::Bind(&GlRendererTest::SetDesktopFrameWithSize,
                              base::Unretained(this), size));
  }
}

void GlRendererTest::OnDesktopFrameProcessed() {
  on_desktop_frame_processed_call_count_++;
}

void GlRendererTest::RunTasksInCurrentQueue() {
  base::RunLoop run_loop;
  message_loop_.task_runner()->PostTask(FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
}

void GlRendererTest::RunUntilRendered() {
  base::RunLoop run_loop;
  delegate_.SetOnFrameRenderedCallback(run_loop.QuitClosure());
  run_loop.Run();
}


TEST_F(GlRendererTest, TestDelegateCanRenderFrame) {
  delegate_.can_render_frame_ = true;
  RequestRender();
  RunTasksInCurrentQueue();
  EXPECT_EQ(1, delegate_.can_render_frame_call_count());
  EXPECT_EQ(1, delegate_.on_frame_rendered_call_count());

  delegate_.can_render_frame_ = false;
  RequestRender();
  RunTasksInCurrentQueue();
  EXPECT_EQ(2, delegate_.can_render_frame_call_count());
  EXPECT_EQ(1, delegate_.on_frame_rendered_call_count());
}

TEST_F(GlRendererTest, TestRequestRenderOnlyScheduleOnce) {
  delegate_.can_render_frame_ = true;

  RequestRender();
  RequestRender();
  RequestRender();
  RunTasksInCurrentQueue();
  EXPECT_EQ(1, delegate_.can_render_frame_call_count());
  EXPECT_EQ(1, delegate_.on_frame_rendered_call_count());

  RequestRender();
  RunTasksInCurrentQueue();
  EXPECT_EQ(2, delegate_.can_render_frame_call_count());
  EXPECT_EQ(2, delegate_.on_frame_rendered_call_count());
}

TEST_F(GlRendererTest, TestDelegateOnSizeChanged) {
  SetDesktopFrameWithSize(webrtc::DesktopSize(16, 16));
  EXPECT_EQ(1, delegate_.on_size_changed_call_count());
  EXPECT_EQ(16, delegate_.canvas_width());
  EXPECT_EQ(16, delegate_.canvas_height());

  SetDesktopFrameWithSize(webrtc::DesktopSize(16, 16));
  EXPECT_EQ(1, delegate_.on_size_changed_call_count());
  EXPECT_EQ(16, delegate_.canvas_width());
  EXPECT_EQ(16, delegate_.canvas_height());

  SetDesktopFrameWithSize(webrtc::DesktopSize(32, 32));
  EXPECT_EQ(2, delegate_.on_size_changed_call_count());
  EXPECT_EQ(32, delegate_.canvas_width());
  EXPECT_EQ(32, delegate_.canvas_height());

  renderer_->RequestCanvasSize();
  EXPECT_EQ(3, delegate_.on_size_changed_call_count());
  EXPECT_EQ(32, delegate_.canvas_width());
  EXPECT_EQ(32, delegate_.canvas_height());
}

TEST_F(GlRendererTest, TestOnFrameReceivedDoneCallbacks) {
  delegate_.can_render_frame_ = true;

  // Implicitly calls RequestRender().

  PostSetDesktopFrameTasks(webrtc::DesktopSize(16, 16), 1);
  RunUntilRendered();
  EXPECT_EQ(1, delegate_.on_frame_rendered_call_count());
  EXPECT_EQ(1, on_desktop_frame_processed_call_count());

  PostSetDesktopFrameTasks(webrtc::DesktopSize(16, 16), 20);
  RunUntilRendered();
  EXPECT_EQ(2, delegate_.on_frame_rendered_call_count());
  EXPECT_EQ(21, on_desktop_frame_processed_call_count());
}

// TODO(yuweih): Add tests to validate the rendered output.

}  // namespace remoting
