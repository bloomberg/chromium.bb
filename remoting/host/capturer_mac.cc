// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/capturer.h"

#include <ApplicationServices/ApplicationServices.h>
#include <OpenGL/CGLMacro.h>
#include <OpenGL/OpenGL.h>

#include <stddef.h>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "remoting/host/capturer_helper.h"

namespace remoting {

namespace {
// A class to perform capturing for mac.
class CapturerMac : public Capturer {
 public:
  CapturerMac();
  virtual ~CapturerMac();

  // Capturer interface.
  virtual void ScreenConfigurationChanged() OVERRIDE;
  virtual media::VideoFrame::Format pixel_format() const OVERRIDE;
  virtual void ClearInvalidRects() OVERRIDE;
  virtual void InvalidateRects(const InvalidRects& inval_rects) OVERRIDE;
  virtual void InvalidateScreen(const gfx::Size& size) OVERRIDE;
  virtual void InvalidateFullScreen() OVERRIDE;
  virtual void CaptureInvalidRects(CaptureCompletedCallback* callback) OVERRIDE;
  virtual const gfx::Size& size_most_recent() const OVERRIDE;

 private:
  void CaptureRects(const InvalidRects& rects,
                    CaptureCompletedCallback* callback);

  void ScreenRefresh(CGRectCount count, const CGRect *rect_array);
  void ScreenUpdateMove(CGScreenUpdateMoveDelta delta,
                                size_t count,
                                const CGRect *rect_array);
  static void ScreenRefreshCallback(CGRectCount count,
                                    const CGRect *rect_array,
                                    void *user_parameter);
  static void ScreenUpdateMoveCallback(CGScreenUpdateMoveDelta delta,
                                       size_t count,
                                       const CGRect *rect_array,
                                       void *user_parameter);
  static void DisplaysReconfiguredCallback(CGDirectDisplayID display,
                                           CGDisplayChangeSummaryFlags flags,
                                           void *user_parameter);

  void ReleaseBuffers();
  CGLContextObj cgl_context_;
  static const int kNumBuffers = 2;
  scoped_array<uint8> buffers_[kNumBuffers];
  scoped_array<uint8> flip_buffer_;

  // A thread-safe list of invalid rectangles, and the size of the most
  // recently captured screen.
  CapturerHelper helper_;

  // Screen size.
  int width_;
  int height_;

  int bytes_per_row_;

  // The current buffer with valid data for reading.
  int current_buffer_;

  // Format of pixels returned in buffer.
  media::VideoFrame::Format pixel_format_;

  DISALLOW_COPY_AND_ASSIGN(CapturerMac);
};

CapturerMac::CapturerMac()
    : cgl_context_(NULL),
      width_(0),
      height_(0),
      bytes_per_row_(0),
      current_buffer_(0),
      pixel_format_(media::VideoFrame::RGB32) {
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
  InvalidateScreen(gfx::Size(width_, height_));
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
  size_t buffer_size = height_ * bytes_per_row_;
  for (int i = 0; i < kNumBuffers; ++i) {
    buffers_[i].reset(new uint8[buffer_size]);
  }
  flip_buffer_.reset(new uint8[buffer_size]);
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

media::VideoFrame::Format CapturerMac::pixel_format() const {
  return pixel_format_;
}

void CapturerMac::ClearInvalidRects() {
  helper_.ClearInvalidRects();
}

void CapturerMac::InvalidateRects(const InvalidRects& inval_rects) {
  helper_.InvalidateRects(inval_rects);
}

void CapturerMac::InvalidateScreen(const gfx::Size& size) {
  helper_.InvalidateScreen(size);
}

void CapturerMac::InvalidateFullScreen() {
  helper_.InvalidateFullScreen();
}

void CapturerMac::CaptureInvalidRects(CaptureCompletedCallback* callback) {
  InvalidRects rects;
  helper_.SwapInvalidRects(rects);

  CGLContextObj CGL_MACRO_CONTEXT = cgl_context_;
  glReadBuffer(GL_FRONT);
  glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT);

  glPixelStorei(GL_PACK_ALIGNMENT, 4);  // Force 4-byte alignment.
  glPixelStorei(GL_PACK_ROW_LENGTH, 0);
  glPixelStorei(GL_PACK_SKIP_ROWS, 0);
  glPixelStorei(GL_PACK_SKIP_PIXELS, 0);

  // Read a block of pixels from the frame buffer.
  uint8* flip_buffer = flip_buffer_.get();
  uint8* current_buffer = buffers_[current_buffer_].get();
  glReadPixels(0, 0, width_, height_, GL_BGRA, GL_UNSIGNED_BYTE, flip_buffer);
  glPopClientAttrib();

  // OpenGL reads with a vertical flip, and sadly there is no optimized
  // way to get it flipped automatically.
  for (int y = 0; y < height_; ++y) {
    uint8* flip_row = &(flip_buffer[y * bytes_per_row_]);
    uint8* current_row =
        &(current_buffer[(height_ - (y + 1)) * bytes_per_row_]);
    memcpy(current_row, flip_row, bytes_per_row_);
  }

  DataPlanes planes;
  planes.data[0] = buffers_[current_buffer_].get();
  planes.strides[0] = bytes_per_row_;

  scoped_refptr<CaptureData> data(
      new CaptureData(planes, gfx::Size(width_, height_), pixel_format()));
  data->mutable_dirty_rects() = rects;

  current_buffer_ = (current_buffer_ + 1) % kNumBuffers;
  helper_.set_size_most_recent(data->size());

  callback->Run(data);
  delete callback;
}

const gfx::Size& CapturerMac::size_most_recent() const {
  return helper_.size_most_recent();
}

void CapturerMac::ScreenRefresh(CGRectCount count, const CGRect *rect_array) {
  InvalidRects rects;
  for (CGRectCount i = 0; i < count; ++i) {
    rects.insert(gfx::Rect(rect_array[i]));
  }
  InvalidateRects(rects);
}

void CapturerMac::ScreenUpdateMove(CGScreenUpdateMoveDelta delta,
                                   size_t count,
                                   const CGRect *rect_array) {
  InvalidRects rects;
  for (CGRectCount i = 0; i < count; ++i) {
    CGRect rect = rect_array[i];
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

}  // namespace

// static
Capturer* Capturer::Create() {
  return new CapturerMac();
}

}  // namespace remoting
