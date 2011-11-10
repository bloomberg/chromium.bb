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
        FROM_HERE, base::Bind(&PepperViewProxy::TearDown, this));
    return;
  }

  if (view_)
    view_->TearDown();
}

void PepperViewProxy::Paint() {
  if (instance_ && !plugin_message_loop_->BelongsToCurrentThread()) {
    plugin_message_loop_->PostTask(
        FROM_HERE, base::Bind(&PepperViewProxy::Paint, this));
    return;
  }

  if (view_)
    view_->Paint();
}

void PepperViewProxy::SetSolidFill(uint32 color) {
  if (instance_ && !plugin_message_loop_->BelongsToCurrentThread()) {
    plugin_message_loop_->PostTask(FROM_HERE, base::Bind(
        &PepperViewProxy::SetSolidFill, this, color));
    return;
  }

  if (view_)
    view_->SetSolidFill(color);
}

void PepperViewProxy::UnsetSolidFill() {
  if (instance_ && !plugin_message_loop_->BelongsToCurrentThread()) {
    plugin_message_loop_->PostTask(
        FROM_HERE, base::Bind(&PepperViewProxy::UnsetSolidFill, this));
    return;
  }

  if (view_)
    view_->UnsetSolidFill();
}

void PepperViewProxy::SetConnectionState(
    protocol::ConnectionToHost::State state,
    protocol::ConnectionToHost::Error error) {
  if (instance_ && !plugin_message_loop_->BelongsToCurrentThread()) {
    plugin_message_loop_->PostTask(FROM_HERE, base::Bind(
        &PepperViewProxy::SetConnectionState, this, state, error));
    return;
  }

  if (view_)
    view_->SetConnectionState(state, error);
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
    const SkISize& size,
    scoped_refptr<media::VideoFrame>* frame_out,
    const base::Closure& done) {
  if (instance_ && !plugin_message_loop_->BelongsToCurrentThread()) {
    plugin_message_loop_->PostTask(FROM_HERE, base::Bind(
        &PepperViewProxy::AllocateFrame, this, format, size, frame_out, done));
    return;
  }

  if (view_) {
    view_->AllocateFrame(format, size, frame_out, done);
  }
}

void PepperViewProxy::ReleaseFrame(media::VideoFrame* frame) {
  if (instance_ && !plugin_message_loop_->BelongsToCurrentThread()) {
    plugin_message_loop_->PostTask(FROM_HERE, base::Bind(
        &PepperViewProxy::ReleaseFrame, this, make_scoped_refptr(frame)));
    return;
  }

  if (view_)
    view_->ReleaseFrame(frame);
}

void PepperViewProxy::OnPartialFrameOutput(media::VideoFrame* frame,
                                           RectVector* rects,
                                           const base::Closure& done) {
  if (instance_ && !plugin_message_loop_->BelongsToCurrentThread()) {
    plugin_message_loop_->PostTask(FROM_HERE, base::Bind(
        &PepperViewProxy::OnPartialFrameOutput, this,
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
