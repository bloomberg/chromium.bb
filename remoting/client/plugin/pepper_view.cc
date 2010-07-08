// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/plugin/pepper_view.h"

#include "base/message_loop.h"
#include "remoting/client/decoder_verbatim.h"

namespace remoting {

PepperView::PepperView(MessageLoop* message_loop, NPDevice* rendering_device,
                       NPP plugin_instance)
  : message_loop_(message_loop),
    rendering_device_(rendering_device),
    plugin_instance_(plugin_instance),
    backing_store_width_(0),
    backing_store_height_(0),
    viewport_x_(0),
    viewport_y_(0),
    viewport_width_(0),
    viewport_height_(0),
    is_static_fill_(false),
    static_fill_color_(0) {
}

PepperView::~PepperView() {
}

void PepperView::Paint() {
  message_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &PepperView::DoPaint));
}

void PepperView::SetSolidFill(uint32 color) {
  message_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &PepperView::DoSetSolidFill, color));
}

void PepperView::UnsetSolidFill() {
  message_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &PepperView::DoUnsetSolidFill));
}

void PepperView::SetViewport(int x, int y, int width, int height) {
  message_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &PepperView::DoSetViewport,
                        x, y, width, height));
}

void PepperView::SetBackingStoreSize(int width, int height) {
  message_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &PepperView::DoSetBackingStoreSize,
                        width, height));
}

void PepperView::HandleBeginUpdateStream(HostMessage* msg) {
  message_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &PepperView::DoHandleBeginUpdateStream, msg));
}

void PepperView::HandleUpdateStreamPacket(HostMessage* msg) {
  message_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &PepperView::DoHandleUpdateStreamPacket, msg));
}

void PepperView::HandleEndUpdateStream(HostMessage* msg) {
  message_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &PepperView::DoHandleEndUpdateStream, msg));
}

void PepperView::DoPaint() {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  LOG(INFO) << "Starting PepperView::DoPaint";

  NPDeviceContext2D context;
  NPDeviceContext2DConfig config;
  rendering_device_->initializeContext(plugin_instance_, &config, &context);

  uint32* output_bitmap = static_cast<uint32*>(context.region);

  // TODO(ajwong): Remove debugging code and actually hook up real painting
  // logic from the decoder.
  LOG(INFO) << "Painting top: " << context.dirty.top
            << " bottom: " << context.dirty.bottom
            << " left: " << context.dirty.left
            << " right: " << context.dirty.right;
  for (int i = context.dirty.top; i < context.dirty.bottom; ++i) {
    for (int j = context.dirty.left; j < context.dirty.right; ++j) {
      *output_bitmap++ = static_fill_color_;
    }
  }

  rendering_device_->flushContext(plugin_instance_, &context, NULL, NULL);
  LOG(INFO) << "Finishing PepperView::DoPaint";
}

void PepperView::DoSetSolidFill(uint32 color) {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  is_static_fill_ = true;
  static_fill_color_ = color;
}

void PepperView::DoUnsetSolidFill() {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  is_static_fill_ = false;
}

void PepperView::DoSetViewport(int x, int y, int width, int height) {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  viewport_x_ = x;
  viewport_y_ = y;
  viewport_width_ = width;
  viewport_height_ = height;
}

void PepperView::DoSetBackingStoreSize(int width, int height) {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  backing_store_width_ = width;
  backing_store_height_ = height;
}

void PepperView::DoHandleBeginUpdateStream(HostMessage* msg) {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  NOTIMPLEMENTED();
}

void PepperView::DoHandleUpdateStreamPacket(HostMessage* msg) {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  NOTIMPLEMENTED();
}

void PepperView::DoHandleEndUpdateStream(HostMessage* msg) {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  NOTIMPLEMENTED();
}

}  // namespace remoting
