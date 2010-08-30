// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/plugin/pepper_view.h"

#include "base/message_loop.h"
#include "remoting/client/plugin/chromoting_instance.h"
#include "remoting/client/plugin/pepper_util.h"
#include "third_party/ppapi/cpp/graphics_2d.h"
#include "third_party/ppapi/cpp/image_data.h"
#include "third_party/ppapi/cpp/point.h"
#include "third_party/ppapi/cpp/size.h"

namespace remoting {

PepperView::PepperView(ChromotingInstance* instance)
    : instance_(instance),
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
    RunTaskOnPluginThread(NewRunnableMethod(this, &PepperView::Paint));
    return;
  }

  // TODO(ajwong): We're assuming the native format is BGRA_PREMUL below. This
  // is wrong.
  pp::ImageData image(pp::ImageData::GetNativeImageDataFormat(),
                      pp::Size(viewport_width_, viewport_height_),
                      false);
  if (image.is_null()) {
    LOG(ERROR) << "Unable to allocate image of size: "
               << viewport_width_ << "x" << viewport_height_;
    return;
  }

  if (is_static_fill_) {
    for (int y = 0; y < image.size().height(); y++) {
      for (int x = 0; x < image.size().width(); x++) {
        *image.GetAddr32(pp::Point(x, y)) = static_fill_color_;
      }
    }
  } else if (frame_) {
    uint32_t* frame_data =
        reinterpret_cast<uint32_t*>(frame_->data(media::VideoFrame::kRGBPlane));
    int max_height = std::min(frame_height_, image.size().height());
    int max_width = std::min(frame_width_, image.size().width());
    for (int y = 0; y < max_height; y++) {
      for (int x = 0; x < max_width; x++) {
        // Force alpha to be set to 255.
        *image.GetAddr32(pp::Point(x, y)) =
            frame_data[y*frame_width_ + x] | 0xFF000000;
      }
    }
  } else {
    // Nothing to paint. escape!
    //
    // TODO(ajwong): This is an ugly control flow. fix.
    return;
  }
  device_context_.ReplaceContents(&image);
  device_context_.Flush(TaskToCompletionCallback(
      NewRunnableMethod(this, &PepperView::OnPaintDone)));
}

void PepperView::SetSolidFill(uint32 color) {
  if (!instance_->CurrentlyOnPluginThread()) {
    RunTaskOnPluginThread(
        NewRunnableMethod(this, &PepperView::SetSolidFill, color));
    return;
  }

  is_static_fill_ = true;
  static_fill_color_ = color;
}

void PepperView::UnsetSolidFill() {
  if (!instance_->CurrentlyOnPluginThread()) {
    RunTaskOnPluginThread(
        NewRunnableMethod(this, &PepperView::UnsetSolidFill));
    return;
  }

  is_static_fill_ = false;
}

void PepperView::SetViewport(int x, int y, int width, int height) {
  if (!instance_->CurrentlyOnPluginThread()) {
    RunTaskOnPluginThread(NewRunnableMethod(this, &PepperView::SetViewport,
                                          x, y, width, height));
    return;
  }

  // TODO(ajwong): Should we ignore x & y updates?  What do those even mean?

  // TODO(ajwong): What does viewport x, y mean to a plugin anyways?
  viewport_x_ = x;
  viewport_y_ = y;
  viewport_width_ = width;
  viewport_height_ = height;

  device_context_ =
      pp::Graphics2D(pp::Size(viewport_width_, viewport_height_), false);
  if (!instance_->BindGraphics(device_context_)) {
    LOG(ERROR) << "Couldn't bind the device context.";
    return;
  }
}

void PepperView::SetHostScreenSize(int width, int height) {
  if (!instance_->CurrentlyOnPluginThread()) {
    RunTaskOnPluginThread(NewRunnableMethod(this,
                                            &PepperView::SetHostScreenSize,
                                            width, height));
    return;
  }

  frame_width_ = width;
  frame_height_ = height;

  // Reset |frame_| - it will be recreated by the next update stream.
  frame_ = NULL;
}

void PepperView::HandleBeginUpdateStream(ChromotingHostMessage* msg) {
  if (!instance_->CurrentlyOnPluginThread()) {
    RunTaskOnPluginThread(
        NewRunnableMethod(this, &PepperView::HandleBeginUpdateStream,
                          msg));
    return;
  }

  scoped_ptr<ChromotingHostMessage> deleter(msg);

  // Make sure the |frame_| is initialized.
  if (!frame_) {
    media::VideoFrame::CreateFrame(media::VideoFrame::RGB32,
                                   frame_width_, frame_height_,
                                   base::TimeDelta(), base::TimeDelta(),
                                   &frame_);
    CHECK(frame_);
  }
}

void PepperView::HandleUpdateStreamPacket(ChromotingHostMessage* msg) {
  if (!instance_->CurrentlyOnPluginThread()) {
    RunTaskOnPluginThread(
        NewRunnableMethod(this, &PepperView::HandleUpdateStreamPacket,
                          msg));
    return;
  }

  // Lazily initialize the decoder.
  SetupDecoder(msg->update_stream_packet().begin_rect().encoding());
  if (!decoder_->IsStarted()) {
    BeginDecoding(NewRunnableMethod(this, &PepperView::OnPartialDecodeDone),
                  NewRunnableMethod(this, &PepperView::OnDecodeDone));
  }

  Decode(msg);
}

void PepperView::HandleEndUpdateStream(ChromotingHostMessage* msg) {
  if (!instance_->CurrentlyOnPluginThread()) {
    RunTaskOnPluginThread(
        NewRunnableMethod(this, &PepperView::HandleEndUpdateStream,
                          msg));
    return;
  }

  scoped_ptr<ChromotingHostMessage> deleter(msg);
  EndDecoding();
}

void PepperView::OnPaintDone() {
  // TODO(ajwong):Probably should set some variable to allow repaints to
  // actually paint.
  return;
}

void PepperView::OnPartialDecodeDone() {
  all_update_rects_.insert(all_update_rects_.begin() +
                           all_update_rects_.size(),
                           update_rects_.begin(), update_rects_.end());
  Paint();
  // TODO(ajwong): Need to block here to be synchronous.
}


void PepperView::OnDecodeDone() {
}

}  // namespace remoting
