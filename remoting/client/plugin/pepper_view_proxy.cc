// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/plugin/pepper_view_proxy.h"

#include "base/message_loop.h"
#include "remoting/base/tracer.h"
#include "remoting/client/client_context.h"
#include "remoting/client/plugin/chromoting_instance.h"
#include "remoting/client/plugin/pepper_util.h"

namespace remoting {

PepperViewProxy::PepperViewProxy(ChromotingInstance* instance, PepperView* view)
  : instance_(instance),
    view_(view) {
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
  if (instance_ && !instance_->CurrentlyOnPluginThread()) {
    RunTaskOnPluginThread(NewTracedMethod(this, &PepperViewProxy::TearDown));
    return;
  }

  if (view_)
    view_->TearDown();
}

void PepperViewProxy::Paint() {
  if (instance_ && !instance_->CurrentlyOnPluginThread()) {
    RunTaskOnPluginThread(NewTracedMethod(this, &PepperViewProxy::Paint));
    return;
  }

  if (view_)
    view_->Paint();
}

void PepperViewProxy::SetSolidFill(uint32 color) {
  if (instance_ && !instance_->CurrentlyOnPluginThread()) {
    RunTaskOnPluginThread(
        NewTracedMethod(this, &PepperViewProxy::SetSolidFill, color));
    return;
  }

  if (view_)
    view_->SetSolidFill(color);
}

void PepperViewProxy::UnsetSolidFill() {
  if (instance_ && !instance_->CurrentlyOnPluginThread()) {
    RunTaskOnPluginThread(
        NewTracedMethod(this, &PepperViewProxy::UnsetSolidFill));
    return;
  }

  if (view_)
    view_->UnsetSolidFill();
}

void PepperViewProxy::SetConnectionState(ConnectionState state) {
  if (instance_ && !instance_->CurrentlyOnPluginThread()) {
    RunTaskOnPluginThread(
        NewRunnableMethod(this, &PepperViewProxy::SetConnectionState, state));
    return;
  }

  if (view_)
    view_->SetConnectionState(state);
}

void PepperViewProxy::UpdateLoginStatus(bool success, const std::string& info) {
  if (instance_ && !instance_->CurrentlyOnPluginThread()) {
    RunTaskOnPluginThread(NewTracedMethod(this,
                                          &PepperViewProxy::UpdateLoginStatus,
                                          success, info));
    return;
  }

  if (view_)
    view_->UpdateLoginStatus(success, info);
}

void PepperViewProxy::SetViewport(int x, int y, int width, int height) {
  if (instance_ && !instance_->CurrentlyOnPluginThread()) {
    RunTaskOnPluginThread(NewTracedMethod(this, &PepperViewProxy::SetViewport,
                                          x, y, width, height));
    return;
  }

  if (view_)
    view_->SetViewport(x, y, width, height);
}

void PepperViewProxy::AllocateFrame(
    media::VideoFrame::Format format,
    size_t width,
    size_t height,
    base::TimeDelta timestamp,
    base::TimeDelta duration,
    scoped_refptr<media::VideoFrame>* frame_out,
    Task* done) {
  if (instance_ && !instance_->CurrentlyOnPluginThread()) {
    RunTaskOnPluginThread(
        NewTracedMethod(this, &PepperViewProxy::AllocateFrame, format, width,
                        height, timestamp, duration, frame_out, done));
    return;
  }

  if (view_) {
    view_->AllocateFrame(format, width, height, timestamp, duration, frame_out,
                         done);
  }
}

void PepperViewProxy::ReleaseFrame(media::VideoFrame* frame) {
  if (instance_ && !instance_->CurrentlyOnPluginThread()) {
    RunTaskOnPluginThread(
        NewTracedMethod(this, &PepperViewProxy::ReleaseFrame,
                        make_scoped_refptr(frame)));
    return;
  }

  if (view_)
    view_->ReleaseFrame(frame);
}

void PepperViewProxy::OnPartialFrameOutput(media::VideoFrame* frame,
                                           UpdatedRects* rects,
                                           Task* done) {
  if (instance_ && !instance_->CurrentlyOnPluginThread()) {
    RunTaskOnPluginThread(
        NewTracedMethod(this, &PepperViewProxy::OnPartialFrameOutput,
                        make_scoped_refptr(frame), rects, done));
    return;
  }

  if (view_)
    view_->OnPartialFrameOutput(frame, rects, done);
}

void PepperViewProxy::Detach() {
  DCHECK(instance_->CurrentlyOnPluginThread());
  instance_ = NULL;
  view_ = NULL;
}

}  // namespace remoting
