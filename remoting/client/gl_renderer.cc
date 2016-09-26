// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/gl_renderer.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "remoting/client/gl_canvas.h"
#include "remoting/client/gl_math.h"
#include "remoting/client/gl_renderer_delegate.h"
#include "remoting/client/sys_opengl.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

namespace remoting {

GlRenderer::GlRenderer() :
    weak_factory_(this) {
  weak_ptr_ = weak_factory_.GetWeakPtr();
  thread_checker_.DetachFromThread();
}

GlRenderer::~GlRenderer() {
}

void GlRenderer::SetDelegate(base::WeakPtr<GlRendererDelegate> delegate) {
  DCHECK(!delegate_);
  delegate_ = delegate;
}

void GlRenderer::RequestCanvasSize() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (delegate_) {
    delegate_->OnSizeChanged(canvas_width_, canvas_height_);
  }
}

void GlRenderer::OnPixelTransformationChanged(
    const std::array<float, 9>& matrix) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!canvas_) {
    LOG(WARNING) << "Trying to set transformation matrix when the canvas is "
        "not ready.";
    return;
  }
  canvas_->SetTransformationMatrix(matrix);
  RequestRender();
}

void GlRenderer::OnCursorMoved(float x, float y) {
  DCHECK(thread_checker_.CalledOnValidThread());
  cursor_.SetCursorPosition(x, y);
  RequestRender();
}

void GlRenderer::OnCursorInputFeedback(float x, float y, float diameter) {
  DCHECK(thread_checker_.CalledOnValidThread());
  cursor_feedback_.StartAnimation(x, y, diameter);
  RequestRender();
}

void GlRenderer::OnCursorVisibilityChanged(bool visible) {
  DCHECK(thread_checker_.CalledOnValidThread());
  cursor_.SetCursorVisible(visible);
  RequestRender();
}

void GlRenderer::OnFrameReceived(std::unique_ptr<webrtc::DesktopFrame> frame,
                                 const base::Closure& done) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(frame->size().width() > 0 && frame->size().height() > 0);
  if (canvas_width_ != frame->size().width() ||
      canvas_height_ != frame->size().height()) {
    if (delegate_) {
      delegate_->OnSizeChanged(frame->size().width(), frame->size().height());
    }
    canvas_width_ = frame->size().width();
    canvas_height_ = frame->size().height();
  }

  desktop_.SetVideoFrame(*frame);
  pending_done_callbacks_.push(done);
  RequestRender();
}

void GlRenderer::OnCursorShapeChanged(const protocol::CursorShapeInfo& shape) {
  DCHECK(thread_checker_.CalledOnValidThread());
  cursor_.SetCursorShape(shape);
  RequestRender();
}

void GlRenderer::OnSurfaceCreated(int gl_version) {
  DCHECK(thread_checker_.CalledOnValidThread());
#ifndef NDEBUG
  // Set the background clear color to bright green for debugging purposes.
  glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
#else
  // Set the background clear color to black.
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
#endif
  canvas_.reset(new GlCanvas(gl_version));
  desktop_.SetCanvas(canvas_.get());
  cursor_.SetCanvas(canvas_.get());
  cursor_feedback_.SetCanvas(canvas_.get());
}

void GlRenderer::OnSurfaceChanged(int view_width, int view_height) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!canvas_) {
    LOG(WARNING) << "Trying to set the view size when the canvas is not ready.";
    return;
  }
  canvas_->SetViewSize(view_width, view_height);
  RequestRender();
}

void GlRenderer::OnSurfaceDestroyed() {
  DCHECK(thread_checker_.CalledOnValidThread());
  cursor_feedback_.SetCanvas(nullptr);
  cursor_.SetCanvas(nullptr);
  desktop_.SetCanvas(nullptr);
  canvas_.reset();
}

base::WeakPtr<GlRenderer> GlRenderer::GetWeakPtr() {
  return weak_ptr_;
}

void GlRenderer::RequestRender() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (render_scheduled_) {
    return;
  }
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&GlRenderer::OnRender, weak_ptr_));
  render_scheduled_ = true;
}

void GlRenderer::OnRender() {
  DCHECK(thread_checker_.CalledOnValidThread());
  render_scheduled_ = false;
  if (!delegate_ || !delegate_->CanRenderFrame()) {
    return;
  }

  if (canvas_) {
    glClear(GL_COLOR_BUFFER_BIT);

    // Layers will be drawn from bottom to top.
    desktop_.Draw();

    // |cursor_feedback_| should be drawn before |cursor_| so that the cursor
    // won't be covered by the feedback animation.
    if (cursor_feedback_.Draw()) {
      RequestRender();
    }

    cursor_.Draw();
  }

  delegate_->OnFrameRendered();

  while (!pending_done_callbacks_.empty()) {
    pending_done_callbacks_.front().Run();
    pending_done_callbacks_.pop();
  }
}

}  // namespace remoting
