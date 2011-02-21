// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/capturer_mac.h"

#include <stddef.h>

#include <OpenGL/CGLMacro.h>

namespace remoting {

CapturerMac::CapturerMac(MessageLoop* message_loop)
    : Capturer(message_loop),
      cgl_context_(NULL) {
  // TODO(dmaclach): move this initialization out into session_manager,
  // or at least have session_manager call into here to initialize it.
  CGError err =
      CGRegisterScreenRefreshCallback(CapturerMac::ScreenRefreshCallback,
                                      this);
  DCHECK_EQ(err, kCGErrorSuccess);
  err = CGScreenRegisterMoveCallback(CapturerMac::ScreenUpdateMoveCallback,
                                     this);
  DCHECK_EQ(err, kCGErrorSuccess);
  err = CGDisplayRegisterReconfigurationCallback(
      CapturerMac::DisplaysReconfiguredCallback, this);
  DCHECK_EQ(err, kCGErrorSuccess);
  ScreenConfigurationChanged();
}

CapturerMac::~CapturerMac() {
  ReleaseBuffers();
  CGUnregisterScreenRefreshCallback(CapturerMac::ScreenRefreshCallback, this);
  CGScreenUnregisterMoveCallback(CapturerMac::ScreenUpdateMoveCallback, this);
  CGDisplayRemoveReconfigurationCallback(
      CapturerMac::DisplaysReconfiguredCallback, this);
}

void CapturerMac::ReleaseBuffers() {
  if (cgl_context_) {
    CGLDestroyContext(cgl_context_);
    cgl_context_ = NULL;
  }
}

void CapturerMac::ScreenConfigurationChanged() {
  ReleaseBuffers();
  CGDirectDisplayID mainDevice = CGMainDisplayID();

  width_ = CGDisplayPixelsWide(mainDevice);
  height_ = CGDisplayPixelsHigh(mainDevice);
  pixel_format_ = media::VideoFrame::RGB32;
  bytes_per_row_ = width_ * sizeof(uint32_t);
  size_t buffer_size = height() * bytes_per_row_;
  for (int i = 0; i < kNumBuffers; ++i) {
    buffers_[i].reset(new uint8[buffer_size]);
  }
  CGLPixelFormatAttribute attributes[] = {
    kCGLPFAFullScreen,
    kCGLPFADisplayMask,
    (CGLPixelFormatAttribute)CGDisplayIDToOpenGLDisplayMask(mainDevice),
    (CGLPixelFormatAttribute)0
  };
  CGLPixelFormatObj pixel_format = NULL;
  GLint matching_pixel_format_count = 0;
  CGLError err = CGLChoosePixelFormat(attributes,
                                      &pixel_format,
                                      &matching_pixel_format_count);
  DCHECK_EQ(err, kCGLNoError);
  err = CGLCreateContext(pixel_format, NULL, &cgl_context_);
  DCHECK_EQ(err, kCGLNoError);
  CGLDestroyPixelFormat(pixel_format);
  CGLSetFullScreen(cgl_context_);
  CGLSetCurrentContext(cgl_context_);
}

void CapturerMac::CalculateInvalidRects() {
  // Since the Mac gets its list of invalid rects via calls to InvalidateRect(),
  // this step only needs to perform post-processing optimizations on the rect
  // list (if needed).
}

void CapturerMac::CaptureRects(const InvalidRects& rects,
                               CaptureCompletedCallback* callback) {
  CGLContextObj CGL_MACRO_CONTEXT = cgl_context_;
  glReadBuffer(GL_FRONT);
  glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT);

  glPixelStorei(GL_PACK_ALIGNMENT, 4);  // Force 4-byte alignment.
  glPixelStorei(GL_PACK_ROW_LENGTH, 0);
  glPixelStorei(GL_PACK_SKIP_ROWS, 0);
  glPixelStorei(GL_PACK_SKIP_PIXELS, 0);

  // Read a block of pixels from the frame buffer.
  glReadPixels(0, 0, width(), height(), GL_BGRA, GL_UNSIGNED_BYTE,
               buffers_[current_buffer_].get());
  glPopClientAttrib();

  DataPlanes planes;
  planes.data[0] = buffers_[current_buffer_].get();
  planes.strides[0] = bytes_per_row_;

  scoped_refptr<CaptureData> data(new CaptureData(planes,
                                                  width(),
                                                  height(),
                                                  pixel_format()));
  data->mutable_dirty_rects() = rects;
  FinishCapture(data, callback);
}

void CapturerMac::ScreenRefresh(CGRectCount count, const CGRect *rect_array) {
  InvalidRects rects;
  for (CGRectCount i = 0; i < count; ++i) {
    CGRect rect = rect_array[i];
    rect.origin.y = height() - rect.size.height;
    rects.insert(gfx::Rect(rect));
  }
  InvalidateRects(rects);
}

void CapturerMac::ScreenUpdateMove(CGScreenUpdateMoveDelta delta,
                                   size_t count,
                                   const CGRect *rect_array) {
  InvalidRects rects;
  for (CGRectCount i = 0; i < count; ++i) {
    CGRect rect = rect_array[i];
    rect.origin.y = height() - rect.size.height;
    rects.insert(gfx::Rect(rect));
    rect = CGRectOffset(rect, delta.dX, delta.dY);
    rects.insert(gfx::Rect(rect));
  }
  InvalidateRects(rects);
}

void CapturerMac::ScreenRefreshCallback(CGRectCount count,
                                        const CGRect *rect_array,
                                        void *user_parameter) {
  CapturerMac *capturer = reinterpret_cast<CapturerMac *>(user_parameter);
  capturer->ScreenRefresh(count, rect_array);
}

void CapturerMac::ScreenUpdateMoveCallback(CGScreenUpdateMoveDelta delta,
                                           size_t count,
                                           const CGRect *rect_array,
                                           void *user_parameter) {
  CapturerMac *capturer = reinterpret_cast<CapturerMac *>(user_parameter);
  capturer->ScreenUpdateMove(delta, count, rect_array);
}

void CapturerMac::DisplaysReconfiguredCallback(
    CGDirectDisplayID display,
    CGDisplayChangeSummaryFlags flags,
    void *user_parameter) {
  if ((display == CGMainDisplayID()) &&
      !(flags & kCGDisplayBeginConfigurationFlag)) {
    CapturerMac *capturer = reinterpret_cast<CapturerMac *>(user_parameter);
    capturer->ScreenConfigurationChanged();
  }
}

// static
Capturer* Capturer::Create(MessageLoop* message_loop) {
  return new CapturerMac(message_loop);
}

}  // namespace remoting
