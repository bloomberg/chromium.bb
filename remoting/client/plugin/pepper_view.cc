// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/plugin/pepper_view.h"

#include "base/message_loop.h"
#include "remoting/base/tracer.h"
#include "remoting/client/client_context.h"
#include "remoting/client/plugin/chromoting_instance.h"
#include "remoting/client/plugin/pepper_util.h"
#include "third_party/ppapi/cpp/graphics_2d.h"
#include "third_party/ppapi/cpp/image_data.h"
#include "third_party/ppapi/cpp/point.h"
#include "third_party/ppapi/cpp/size.h"

namespace remoting {

PepperView::PepperView(ChromotingInstance* instance, ClientContext* context)
  : instance_(instance),
    context_(context),
    viewport_x_(0),
    viewport_y_(0),
    viewport_width_(0),
    viewport_height_(0),
    is_static_fill_(false),
    static_fill_color_(0) {
}

PepperView::~PepperView() {
}

bool PepperView::Initialize() {
  return true;
}

void PepperView::TearDown() {
}

void PepperView::Paint() {
  if (!instance_->CurrentlyOnPluginThread()) {
    RunTaskOnPluginThread(NewTracedMethod(this, &PepperView::Paint));
    return;
  }

  TraceContext::tracer()->PrintString("Start Paint.");
  // TODO(ajwong): We're assuming the native format is BGRA_PREMUL below. This
  // is wrong.
  if (is_static_fill_) {
    LOG(ERROR) << "Static filling " << static_fill_color_;
    pp::ImageData image(pp::ImageData::GetNativeImageDataFormat(),
                        pp::Size(viewport_width_, viewport_height_),
                        false);
    if (image.is_null()) {
      LOG(ERROR) << "Unable to allocate image of size: "
                 << viewport_width_ << "x" << viewport_height_;
      return;
    }

    for (int y = 0; y < image.size().height(); y++) {
      for (int x = 0; x < image.size().width(); x++) {
        *image.GetAddr32(pp::Point(x, y)) = static_fill_color_;
      }
    }

    // For ReplaceContents, make sure the image size matches the device context
    // size!  Otherwise, this will just silently do nothing.
    graphics2d_.ReplaceContents(&image);
    graphics2d_.Flush(TaskToCompletionCallback(
        NewTracedMethod(this, &PepperView::OnPaintDone)));
  } else {
    // TODO(ajwong): We need to keep a backing store image of the viewport that
    // has the data here which can be redrawn.
    return;
  }
  TraceContext::tracer()->PrintString("End Paint.");
}

void PepperView::PaintFrame(media::VideoFrame* frame, UpdatedRects* rects) {
  DCHECK(instance_->CurrentlyOnPluginThread());

  TraceContext::tracer()->PrintString("Start Paint Frame.");
  // TODO(ajwong): We're assuming the native format is BGRA_PREMUL below. This
  // is wrong.
  pp::ImageData image(pp::ImageData::GetNativeImageDataFormat(),
                      pp::Size(viewport_width_, viewport_height_),
                      false);
  if (image.is_null()) {
    LOG(ERROR) << "Unable to allocate image of size: "
               << frame->width() << "x" << frame->height();
    return;
  }

  uint32_t* frame_data =
      reinterpret_cast<uint32_t*>(frame->data(media::VideoFrame::kRGBPlane));
  int frame_width = static_cast<int>(frame->width());
  int frame_height = static_cast<int>(frame->height());
  int max_height = std::min(frame_height, image.size().height());
  int max_width = std::min(frame_width, image.size().width());
  for (int y = 0; y < max_height; y++) {
    for (int x = 0; x < max_width; x++) {
      // Force alpha to be set to 255.
      *image.GetAddr32(pp::Point(x, y)) =
          frame_data[y*frame_width + x] | 0xFF000000;
    }
  }

  // For ReplaceContents, make sure the image size matches the device context
  // size!  Otherwise, this will just silently do nothing.
  graphics2d_.ReplaceContents(&image);
  graphics2d_.Flush(TaskToCompletionCallback(
      NewTracedMethod(this, &PepperView::OnPaintDone)));

  TraceContext::tracer()->PrintString("End Paint Frame.");
}

void PepperView::SetSolidFill(uint32 color) {
  if (!instance_->CurrentlyOnPluginThread()) {
    RunTaskOnPluginThread(
        NewTracedMethod(this, &PepperView::SetSolidFill, color));
    return;
  }

  is_static_fill_ = true;
  static_fill_color_ = color;
}

void PepperView::UnsetSolidFill() {
  if (!instance_->CurrentlyOnPluginThread()) {
    RunTaskOnPluginThread(
        NewTracedMethod(this, &PepperView::UnsetSolidFill));
    return;
  }

  is_static_fill_ = false;
}

void PepperView::SetConnectionState(ConnectionState state) {
  if (!instance_->CurrentlyOnPluginThread()) {
    RunTaskOnPluginThread(
        NewRunnableMethod(this, &PepperView::SetConnectionState, state));
    return;
  }

  ChromotingScriptableObject* scriptable_obj = instance_->GetScriptableObject();
  switch (state) {
    case CREATED:
      SetSolidFill(kCreatedColor);
      scriptable_obj->SetConnectionInfo(STATUS_CONNECTING, QUALITY_UNKNOWN);
      break;

    case CONNECTED:
      UnsetSolidFill();
      scriptable_obj->SetConnectionInfo(STATUS_CONNECTED, QUALITY_UNKNOWN);
      break;

    case DISCONNECTED:
      SetSolidFill(kDisconnectedColor);
      scriptable_obj->SetConnectionInfo(STATUS_CLOSED, QUALITY_UNKNOWN);
      break;

    case FAILED:
      SetSolidFill(kFailedColor);
      scriptable_obj->SetConnectionInfo(STATUS_FAILED, QUALITY_UNKNOWN);
      break;
  }
}

void PepperView::SetViewport(int x, int y, int width, int height) {
  if (!instance_->CurrentlyOnPluginThread()) {
    RunTaskOnPluginThread(NewTracedMethod(this, &PepperView::SetViewport,
                                          x, y, width, height));
    return;
  }

  // TODO(ajwong): Should we ignore x & y updates?  What do those even mean?

  // TODO(ajwong): What does viewport x, y mean to a plugin anyways?
  viewport_x_ = x;
  viewport_y_ = y;
  viewport_width_ = width;
  viewport_height_ = height;

  graphics2d_ = pp::Graphics2D(pp::Size(viewport_width_, viewport_height_),
                               false);
  if (!instance_->BindGraphics(graphics2d_)) {
    LOG(ERROR) << "Couldn't bind the device context.";
    return;
  }
}

void PepperView::AllocateFrame(media::VideoFrame::Format format,
                               size_t width,
                               size_t height,
                               base::TimeDelta timestamp,
                               base::TimeDelta duration,
                               scoped_refptr<media::VideoFrame>* frame_out,
                               Task* done) {
  // TODO(ajwong): Implement this to be backed by an pp::ImageData rather than
  // generic memory.
  media::VideoFrame::CreateFrame(media::VideoFrame::RGB32,
                                 width, height,
                                 base::TimeDelta(), base::TimeDelta(),
                                 frame_out);
  if (*frame_out) {
    (*frame_out)->AddRef();
  }
  done->Run();
  delete done;
}

void PepperView::ReleaseFrame(media::VideoFrame* frame) {
  if (frame) {
    LOG(WARNING) << "Frame released.";
    frame->Release();
  }
}

void PepperView::OnPartialFrameOutput(media::VideoFrame* frame,
                                      UpdatedRects* rects,
                                      Task* done) {
  if (!instance_->CurrentlyOnPluginThread()) {
    RunTaskOnPluginThread(
        NewTracedMethod(this, &PepperView::OnPartialFrameOutput,
                        make_scoped_refptr(frame), rects, done));
    return;
  }

  TraceContext::tracer()->PrintString("Calling PaintFrame");
  // TODO(ajwong): Clean up this API to be async so we don't need to use a
  // member variable as a hack.
  PaintFrame(frame, rects);
  done->Run();
  delete done;
}

void PepperView::OnPaintDone() {
  // TODO(ajwong):Probably should set some variable to allow repaints to
  // actually paint.
  TraceContext::tracer()->PrintString("Paint flushed");
  return;
}

}  // namespace remoting
