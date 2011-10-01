// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/plugin/pepper_view_proxy.h"

#include "base/message_loop.h"
#include "remoting/client/client_context.h"
#include "remoting/client/plugin/chromoting_instance.h"

namespace remoting {

PepperViewProxy::PepperViewProxy(ChromotingInstance* instance, PepperView* view,
                                 base::MessageLoopProxy* plugin_message_loop)
  : instance_(instance),
    view_(view),
    plugin_message_loop_(plugin_message_loop) {
}

PepperViewProxy::~PepperViewProxy() {
}

bool PepperViewProxy::Initialize() {
  // This method needs a return value so we can't post a task and process on
  // another thread so just return true since PepperView doesn't do anything
  // either.
  return true;
}

void PepperViewProxy::TearDown() {
  if (instance_ && !plugin_message_loop_->BelongsToCurrentThread()) {
    plugin_message_loop_->PostTask(
        FROM_HERE, NewRunnableMethod(this, &PepperViewProxy::TearDown));
    return;
  }

  if (view_)
    view_->TearDown();
}

void PepperViewProxy::Paint() {
  if (instance_ && !plugin_message_loop_->BelongsToCurrentThread()) {
    plugin_message_loop_->PostTask(
        FROM_HERE, NewRunnableMethod(this, &PepperViewProxy::Paint));
    return;
  }

  if (view_)
    view_->Paint();
}

void PepperViewProxy::SetSolidFill(uint32 color) {
  if (instance_ && !plugin_message_loop_->BelongsToCurrentThread()) {
    plugin_message_loop_->PostTask(FROM_HERE, NewRunnableMethod(
        this, &PepperViewProxy::SetSolidFill, color));
    return;
  }

  if (view_)
    view_->SetSolidFill(color);
}

void PepperViewProxy::UnsetSolidFill() {
  if (instance_ && !plugin_message_loop_->BelongsToCurrentThread()) {
    plugin_message_loop_->PostTask(
        FROM_HERE, NewRunnableMethod(this, &PepperViewProxy::UnsetSolidFill));
    return;
  }

  if (view_)
    view_->UnsetSolidFill();
}

void PepperViewProxy::SetConnectionState(
    protocol::ConnectionToHost::State state,
    protocol::ConnectionToHost::Error error) {
  if (instance_ && !plugin_message_loop_->BelongsToCurrentThread()) {
    plugin_message_loop_->PostTask(FROM_HERE, NewRunnableMethod(
        this, &PepperViewProxy::SetConnectionState, state, error));
    return;
  }

  if (view_)
    view_->SetConnectionState(state, error);
}

void PepperViewProxy::UpdateLoginStatus(bool success, const std::string& info) {
  if (instance_ && !plugin_message_loop_->BelongsToCurrentThread()) {
    plugin_message_loop_->PostTask(FROM_HERE, NewRunnableMethod(
        this, &PepperViewProxy::UpdateLoginStatus, success, info));
    return;
  }

  if (view_)
    view_->UpdateLoginStatus(success, info);
}

double PepperViewProxy::GetHorizontalScaleRatio() const {
  // This method returns a value, so must run synchronously, so must be
  // called only on the pepper thread.
  DCHECK(plugin_message_loop_->BelongsToCurrentThread());

  if (view_)
    return view_->GetHorizontalScaleRatio();
  return 1.0;
}

double PepperViewProxy::GetVerticalScaleRatio() const {
  // This method returns a value, so must run synchronously, so must be
  // called only on the pepper thread.
  DCHECK(plugin_message_loop_->BelongsToCurrentThread());

  if (view_)
    return view_->GetVerticalScaleRatio();
  return 1.0;
}

void PepperViewProxy::AllocateFrame(
    media::VideoFrame::Format format,
    size_t width,
    size_t height,
    base::TimeDelta timestamp,
    base::TimeDelta duration,
    scoped_refptr<media::VideoFrame>* frame_out,
    Task* done) {
  if (instance_ && !plugin_message_loop_->BelongsToCurrentThread()) {
    plugin_message_loop_->PostTask(FROM_HERE, NewRunnableMethod(
        this, &PepperViewProxy::AllocateFrame, format, width,
        height, timestamp, duration, frame_out, done));
    return;
  }

  if (view_) {
    view_->AllocateFrame(format, width, height, timestamp, duration, frame_out,
                         done);
  }
}

void PepperViewProxy::ReleaseFrame(media::VideoFrame* frame) {
  if (instance_ && !plugin_message_loop_->BelongsToCurrentThread()) {
    plugin_message_loop_->PostTask(FROM_HERE, NewRunnableMethod(
        this, &PepperViewProxy::ReleaseFrame, make_scoped_refptr(frame)));
    return;
  }

  if (view_)
    view_->ReleaseFrame(frame);
}

void PepperViewProxy::OnPartialFrameOutput(media::VideoFrame* frame,
                                           RectVector* rects,
                                           Task* done) {
  if (instance_ && !plugin_message_loop_->BelongsToCurrentThread()) {
    plugin_message_loop_->PostTask(FROM_HERE, NewRunnableMethod(
        this, &PepperViewProxy::OnPartialFrameOutput,
        make_scoped_refptr(frame), rects, done));
    return;
  }

  if (view_)
    view_->OnPartialFrameOutput(frame, rects, done);
}

void PepperViewProxy::Detach() {
  DCHECK(plugin_message_loop_->BelongsToCurrentThread());
  instance_ = NULL;
  view_ = NULL;
}

}  // namespace remoting
