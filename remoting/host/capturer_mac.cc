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

class scoped_pixel_buffer_object {
 public:
  scoped_pixel_buffer_object();
  ~scoped_pixel_buffer_object();

  bool Init(CGLContextObj cgl_context, int size_in_bytes);
  void Release();

  GLuint get() const { return pixel_buffer_object_; }

 private:
  CGLContextObj cgl_context_;
  GLuint pixel_buffer_object_;

  DISALLOW_COPY_AND_ASSIGN(scoped_pixel_buffer_object);
};

scoped_pixel_buffer_object::scoped_pixel_buffer_object()
    : cgl_context_(NULL),
      pixel_buffer_object_(0) {
}

scoped_pixel_buffer_object::~scoped_pixel_buffer_object() {
  Release();
}

bool scoped_pixel_buffer_object::Init(CGLContextObj cgl_context,
                                      int size_in_bytes) {
  cgl_context_ = cgl_context;
  CGLContextObj CGL_MACRO_CONTEXT = cgl_context_;
  glGenBuffersARB(1, &pixel_buffer_object_);
  if (glGetError() == GL_NO_ERROR) {
    glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, pixel_buffer_object_);
    glBufferDataARB(GL_PIXEL_PACK_BUFFER_ARB, size_in_bytes, NULL,
                    GL_STREAM_READ_ARB);
    glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, 0);
    if (glGetError() != GL_NO_ERROR) {
      Release();
    }
  } else {
    cgl_context_ = NULL;
    pixel_buffer_object_ = 0;
  }
  return pixel_buffer_object_ != 0;
}

void scoped_pixel_buffer_object::Release() {
  if (pixel_buffer_object_) {
    CGLContextObj CGL_MACRO_CONTEXT = cgl_context_;
    glDeleteBuffersARB(1, &pixel_buffer_object_);
    cgl_context_ = NULL;
    pixel_buffer_object_ = 0;
  }
}


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
  void FastBlit(uint8* buffer);
  void SlowBlit(uint8* buffer);
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
  scoped_pixel_buffer_object pixel_buffer_object_;
  scoped_array<uint8> buffers_[kNumBuffers];

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
    pixel_buffer_object_.Release();
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

  pixel_buffer_object_.Init(cgl_context_, buffer_size);
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
  uint8* current_buffer = buffers_[current_buffer_].get();

  if (pixel_buffer_object_.get() != 0) {
    FastBlit(current_buffer);
  } else {
    SlowBlit(current_buffer);
  }

  DataPlanes planes;
  planes.data[0] = current_buffer + height_ * bytes_per_row_;
  planes.strides[0] = -bytes_per_row_;

  scoped_refptr<CaptureData> data(
      new CaptureData(planes, gfx::Size(width_, height_), pixel_format()));
  data->mutable_dirty_rects() = rects;

  current_buffer_ = (current_buffer_ + 1) % kNumBuffers;
  helper_.set_size_most_recent(data->size());

  callback->Run(data);
  delete callback;
}

void CapturerMac::FastBlit(uint8* buffer) {
  CGLContextObj CGL_MACRO_CONTEXT = cgl_context_;
  glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, pixel_buffer_object_.get());
  glReadPixels(0, 0, width_, height_, GL_BGRA, GL_UNSIGNED_BYTE, 0);
  GLubyte* ptr = static_cast<GLubyte*>(
      glMapBufferARB(GL_PIXEL_PACK_BUFFER_ARB, GL_READ_ONLY_ARB));
  if (ptr == NULL) {
    // If the buffer can't be mapped, assume that it's no longer valid, release
    // it and fall back on SlowBlit for this (and subsequent) captures.
    pixel_buffer_object_.Release();
    SlowBlit(buffer);
  } else {
    memcpy(buffer, ptr, height_ * bytes_per_row_);
  }
  if (!glUnmapBufferARB(GL_PIXEL_PACK_BUFFER_ARB)) {
    // If glUnmapBuffer returns false, then the contents of the data store are
    // undefined. This might be because the screen mode has changed, in which
    // case it will be recreated in ScreenConfigurationChanged, but releasing
    // the object here is the best option. Capturing will fall back on SlowBlit
    // until such time as the pixel buffer object is recreated.
    pixel_buffer_object_.Release();
  }
  glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, 0);
}

void CapturerMac::SlowBlit(uint8* buffer) {
  CGLContextObj CGL_MACRO_CONTEXT = cgl_context_;
  glReadBuffer(GL_FRONT);
  glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT);
  glPixelStorei(GL_PACK_ALIGNMENT, 4);  // Force 4-byte alignment.
  glPixelStorei(GL_PACK_ROW_LENGTH, 0);
  glPixelStorei(GL_PACK_SKIP_ROWS, 0);
  glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
  // Read a block of pixels from the frame buffer.
  glReadPixels(0, 0, width_, height_, GL_BGRA, GL_UNSIGNED_BYTE, buffer);
  glPopClientAttrib();
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
