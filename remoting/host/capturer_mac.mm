// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/capturer.h"

#include <ApplicationServices/ApplicationServices.h>
#include <Cocoa/Cocoa.h>
#include <dlfcn.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <OpenGL/CGLMacro.h>
#include <OpenGL/OpenGL.h>
#include <stddef.h>

#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/time.h"
#include "remoting/base/capture_data.h"
#include "remoting/base/util.h"
#include "remoting/host/capturer_helper.h"
#include "remoting/proto/control.pb.h"

#if !defined(MAC_OS_X_VERSION_10_6) || \
    MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_6

@interface NSCursor (SnowLeopardSDKDeclarations)
+ (NSCursor*)currentSystemCursor;
@end

@interface NSImage (SnowLeopardSDKDeclarations)
- (CGImageRef)CGImageForProposedRect:(NSRect*)proposedDestRect
                             context:(NSGraphicsContext*)referenceContext
                               hints:(NSDictionary*)hints;
@end

#endif  // MAC_OS_X_VERSION_10_6

namespace remoting {

namespace {

SkIRect CGRectToSkIRect(const CGRect& rect) {
  SkIRect sk_rect = {
    SkScalarRound(rect.origin.x),
    SkScalarRound(rect.origin.y),
    SkScalarRound(rect.origin.x + rect.size.width),
    SkScalarRound(rect.origin.y + rect.size.height)
  };
  return sk_rect;
}

#if (MAC_OS_X_VERSION_MIN_REQUIRED > MAC_OS_X_VERSION_10_5)
// Possibly remove CapturerMac::CgBlitPreLion as well depending on performance
// of CapturerMac::CgBlitPostLion on 10.6.
#error No longer need to import CGDisplayCreateImage.
#else
// Declared here because CGDisplayCreateImage does not exist in the 10.5 SDK,
// which we are currently compiling against, and it is required on 10.7 to do
// screen capture.
typedef CGImageRef (*CGDisplayCreateImageFunc)(CGDirectDisplayID displayID);
#endif

// The amount of time allowed for displays to reconfigure.
const int64 kDisplayReconfigurationTimeoutInSeconds = 10;

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
  // The PBO path is only done on 10.6 (SnowLeopard) and above due to
  // a driver issue that was found on 10.5
  // (specifically on a NVIDIA GeForce 7300 GT).
  // http://crbug.com/87283
  if (base::mac::IsOSLeopardOrEarlier()) {
    return false;
  }
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

// A class representing a full-frame pixel buffer.
class VideoFrameBuffer {
 public:
  VideoFrameBuffer() : bytes_per_row_(0), needs_update_(true) { }

  // If the buffer is marked as needing to be updated (for example after the
  // screen mode changes) and is the wrong size, then release the old buffer
  // and create a new one.
  void Update() {
    if (needs_update_) {
      needs_update_ = false;
      CGDirectDisplayID mainDevice = CGMainDisplayID();
      int width = CGDisplayPixelsWide(mainDevice);
      int height = CGDisplayPixelsHigh(mainDevice);
      if (width != size_.width() || height != size_.height()) {
        size_.set(width, height);
        bytes_per_row_ = width * sizeof(uint32_t);
        size_t buffer_size = width * height * sizeof(uint32_t);
        ptr_.reset(new uint8[buffer_size]);
      }
    }
  }

  SkISize size() const { return size_; }
  int bytes_per_row() const { return bytes_per_row_; }
  uint8* ptr() const { return ptr_.get(); }

  void set_needs_update() { needs_update_ = true; }

 private:
  SkISize size_;
  int bytes_per_row_;
  scoped_array<uint8> ptr_;
  bool needs_update_;

  DISALLOW_COPY_AND_ASSIGN(VideoFrameBuffer);
};

// A class to perform capturing for mac.
class CapturerMac : public Capturer {
 public:
  CapturerMac();
  virtual ~CapturerMac();

  bool Init();

  // Capturer interface.
  virtual void Start(const CursorShapeChangedCallback& callback) OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual void ScreenConfigurationChanged() OVERRIDE;
  virtual media::VideoFrame::Format pixel_format() const OVERRIDE;
  virtual void ClearInvalidRegion() OVERRIDE;
  virtual void InvalidateRegion(const SkRegion& invalid_region) OVERRIDE;
  virtual void InvalidateScreen(const SkISize& size) OVERRIDE;
  virtual void InvalidateFullScreen() OVERRIDE;
  virtual void CaptureInvalidRegion(
      const CaptureCompletedCallback& callback) OVERRIDE;
  virtual const SkISize& size_most_recent() const OVERRIDE;

 private:
  void CaptureCursor();

  void GlBlitFast(const VideoFrameBuffer& buffer, const SkRegion& region);
  void GlBlitSlow(const VideoFrameBuffer& buffer);
  void CgBlitPreLion(const VideoFrameBuffer& buffer, const SkRegion& region);
  void CgBlitPostLion(const VideoFrameBuffer& buffer, const SkRegion& region);
  void CaptureRegion(const SkRegion& region,
                     const CaptureCompletedCallback& callback);

  void ScreenRefresh(CGRectCount count, const CGRect *rect_array);
  void ScreenUpdateMove(CGScreenUpdateMoveDelta delta,
                        size_t count,
                        const CGRect *rect_array);
  void DisplaysReconfigured(CGDirectDisplayID display,
                            CGDisplayChangeSummaryFlags flags);
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
  VideoFrameBuffer buffers_[kNumBuffers];

  // A thread-safe list of invalid rectangles, and the size of the most
  // recently captured screen.
  CapturerHelper helper_;

  // Callback notified whenever the cursor shape is changed.
  CursorShapeChangedCallback cursor_shape_changed_callback_;

  // Image of the last cursor that we sent to the client.
  base::mac::ScopedCFTypeRef<CGImageRef> current_cursor_;

  // The current buffer with valid data for reading.
  int current_buffer_;

  // The previous buffer into which we captured, or NULL for the first capture
  // for a particular screen resolution.
  uint8* last_buffer_;

  // Contains an invalid region from the previous capture.
  SkRegion last_invalid_region_;

  // Format of pixels returned in buffer.
  media::VideoFrame::Format pixel_format_;

  // Acts as a critical section around our display configuration data
  // structures. Specifically cgl_context_ and pixel_buffer_object_.
  base::WaitableEvent display_configuration_capture_event_;

  // Will be non-null on lion.
  CGDisplayCreateImageFunc display_create_image_func_;

  // Power management assertion to prevent the screen from sleeping.
  IOPMAssertionID power_assertion_id_display_;

  // Power management assertion to indicate that the user is active.
  IOPMAssertionID power_assertion_id_user_;

  DISALLOW_COPY_AND_ASSIGN(CapturerMac);
};

CapturerMac::CapturerMac()
    : cgl_context_(NULL),
      current_buffer_(0),
      last_buffer_(NULL),
      pixel_format_(media::VideoFrame::RGB32),
      display_configuration_capture_event_(false, true),
      display_create_image_func_(NULL),
      power_assertion_id_display_(kIOPMNullAssertionID),
      power_assertion_id_user_(kIOPMNullAssertionID) {
}

CapturerMac::~CapturerMac() {
  ReleaseBuffers();
  CGUnregisterScreenRefreshCallback(CapturerMac::ScreenRefreshCallback, this);
  CGScreenUnregisterMoveCallback(CapturerMac::ScreenUpdateMoveCallback, this);
  CGError err = CGDisplayRemoveReconfigurationCallback(
      CapturerMac::DisplaysReconfiguredCallback, this);
  if (err != kCGErrorSuccess) {
    LOG(ERROR) << "CGDisplayRemoveReconfigurationCallback " << err;
  }
}

bool CapturerMac::Init() {
  CGError err =
      CGRegisterScreenRefreshCallback(CapturerMac::ScreenRefreshCallback,
                                      this);
  if (err != kCGErrorSuccess) {
    LOG(ERROR) << "CGRegisterScreenRefreshCallback " << err;
    return false;
  }

  err = CGScreenRegisterMoveCallback(CapturerMac::ScreenUpdateMoveCallback,
                                     this);
  if (err != kCGErrorSuccess) {
    LOG(ERROR) << "CGScreenRegisterMoveCallback " << err;
    return false;
  }
  err = CGDisplayRegisterReconfigurationCallback(
      CapturerMac::DisplaysReconfiguredCallback, this);
  if (err != kCGErrorSuccess) {
    LOG(ERROR) << "CGDisplayRegisterReconfigurationCallback " << err;
    return false;
  }

  if (base::mac::IsOSLionOrLater()) {
    display_create_image_func_ =
        reinterpret_cast<CGDisplayCreateImageFunc>(
            dlsym(RTLD_NEXT, "CGDisplayCreateImage"));
    if (!display_create_image_func_) {
      LOG(ERROR) << "Unable to load CGDisplayCreateImage on Lion";
      return false;
    }
  }
  ScreenConfigurationChanged();
  return true;
}

void CapturerMac::ReleaseBuffers() {
  if (cgl_context_) {
    pixel_buffer_object_.Release();
    CGLDestroyContext(cgl_context_);
    cgl_context_ = NULL;
  }
  // The buffers might be in use by the encoder, so don't delete them here.
  // Instead, mark them as "needs update"; next time the buffers are used by
  // the capturer, they will be recreated if necessary.
  for (int i = 0; i < kNumBuffers; ++i) {
    buffers_[i].set_needs_update();
  }
}

void CapturerMac::Start(
    const CursorShapeChangedCallback& callback) {
  cursor_shape_changed_callback_ = callback;

  // Create power management assertions to wake the display and prevent it from
  // going to sleep on user idle.
  IOPMAssertionCreate(kIOPMAssertionTypeNoDisplaySleep,
                      kIOPMAssertionLevelOn,
                      &power_assertion_id_display_);
  IOPMAssertionCreate(CFSTR("UserIsActive"),
                      kIOPMAssertionLevelOn,
                      &power_assertion_id_user_);
}

void CapturerMac::Stop() {
  if (power_assertion_id_display_ != kIOPMNullAssertionID) {
    IOPMAssertionRelease(power_assertion_id_display_);
    power_assertion_id_display_ = kIOPMNullAssertionID;
  }
  if (power_assertion_id_user_ != kIOPMNullAssertionID) {
    IOPMAssertionRelease(power_assertion_id_user_);
    power_assertion_id_user_ = kIOPMNullAssertionID;
  }
}

void CapturerMac::ScreenConfigurationChanged() {
  ReleaseBuffers();
  helper_.ClearInvalidRegion();
  last_buffer_ = NULL;

  CGDirectDisplayID mainDevice = CGMainDisplayID();
  int width = CGDisplayPixelsWide(mainDevice);
  int height = CGDisplayPixelsHigh(mainDevice);
  InvalidateScreen(SkISize::Make(width, height));

  if (!CGDisplayUsesOpenGLAcceleration(mainDevice)) {
    VLOG(3) << "OpenGL support not available.";
    return;
  }

  if (display_create_image_func_ != NULL) {
    // No need for any OpenGL support on Lion
    return;
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

  size_t buffer_size = width * height * sizeof(uint32_t);
  pixel_buffer_object_.Init(cgl_context_, buffer_size);
}

media::VideoFrame::Format CapturerMac::pixel_format() const {
  return pixel_format_;
}

void CapturerMac::ClearInvalidRegion() {
  helper_.ClearInvalidRegion();
}

void CapturerMac::InvalidateRegion(const SkRegion& invalid_region) {
  helper_.InvalidateRegion(invalid_region);
}

void CapturerMac::InvalidateScreen(const SkISize& size) {
  helper_.InvalidateScreen(size);
}

void CapturerMac::InvalidateFullScreen() {
  helper_.InvalidateFullScreen();
}

void CapturerMac::CaptureInvalidRegion(
    const CaptureCompletedCallback& callback) {
  // Only allow captures when the display configuration is not occurring.
  scoped_refptr<CaptureData> data;

  // Critical section shared with DisplaysReconfigured(...).
  CHECK(display_configuration_capture_event_.TimedWait(
      base::TimeDelta::FromSeconds(kDisplayReconfigurationTimeoutInSeconds)));
  SkRegion region;
  helper_.SwapInvalidRegion(&region);
  VideoFrameBuffer& current_buffer = buffers_[current_buffer_];
  current_buffer.Update();

  bool flip = false;  // GL capturers need flipping.
  if (display_create_image_func_ != NULL) {
    // Lion requires us to use their new APIs for doing screen capture.
    CgBlitPostLion(current_buffer, region);
  } else if (cgl_context_) {
    flip = true;
    if (pixel_buffer_object_.get() != 0) {
      GlBlitFast(current_buffer, region);
    } else {
      // See comment in scoped_pixel_buffer_object::Init about why the slow
      // path is always used on 10.5.
      GlBlitSlow(current_buffer);
    }
  } else {
    CgBlitPreLion(current_buffer, region);
  }

  DataPlanes planes;
  planes.data[0] = current_buffer.ptr();
  planes.strides[0] = current_buffer.bytes_per_row();
  if (flip) {
    planes.strides[0] = -planes.strides[0];
    planes.data[0] +=
        (current_buffer.size().height() - 1) * current_buffer.bytes_per_row();
  }

  data = new CaptureData(planes, current_buffer.size(), pixel_format());
  data->mutable_dirty_region() = region;

  current_buffer_ = (current_buffer_ + 1) % kNumBuffers;
  helper_.set_size_most_recent(data->size());
  display_configuration_capture_event_.Signal();

  CaptureCursor();

  callback.Run(data);
}

void CapturerMac::CaptureCursor() {
  if (cursor_shape_changed_callback_.is_null()) {
    return;
    }

  NSCursor* cursor = [NSCursor currentSystemCursor];
  if (cursor == nil) {
    return;
  }

  NSImage* nsimage = [cursor image];
  NSPoint hotspot = [cursor hotSpot];
  NSSize size = [nsimage size];
  CGImageRef image = [nsimage CGImageForProposedRect:NULL
                                             context:nil
                                               hints:nil];
  if (image == nil) {
    return;
  }

  if (CGImageGetBitsPerPixel(image) != 32 ||
      CGImageGetBytesPerRow(image) != (size.width * 4) ||
      CGImageGetBitsPerComponent(image) != 8) {
    return;
  }

  // Compare the current cursor with the last one we sent to the client
  // and exit if the cursor is the same.
  if (current_cursor_.get() != NULL) {
    CGImageRef current = current_cursor_.get();
    if (CGImageGetWidth(image) == CGImageGetWidth(current) &&
        CGImageGetHeight(image) == CGImageGetHeight(current) &&
        CGImageGetBitsPerPixel(image) == CGImageGetBitsPerPixel(current) &&
        CGImageGetBytesPerRow(image) == CGImageGetBytesPerRow(current) &&
        CGImageGetBitsPerComponent(image) ==
            CGImageGetBitsPerComponent(current)) {
      CGDataProviderRef provider_new = CGImageGetDataProvider(image);
      base::mac::ScopedCFTypeRef<CFDataRef> data_ref_new(
          CGDataProviderCopyData(provider_new));
      CGDataProviderRef provider_current = CGImageGetDataProvider(current);
      base::mac::ScopedCFTypeRef<CFDataRef> data_ref_current(
          CGDataProviderCopyData(provider_current));

      if (data_ref_new.get() != NULL && data_ref_current.get() != NULL) {
        int data_size = CFDataGetLength(data_ref_new);
        CHECK(data_size == CFDataGetLength(data_ref_current));
        const uint8* data_new = CFDataGetBytePtr(data_ref_new);
        const uint8* data_current = CFDataGetBytePtr(data_ref_current);
        if (memcmp(data_new, data_current, data_size) == 0) {
          return;
        }
      }
    }
  }

  VLOG(3) << "Sending cursor: " << size.width << "x" << size.height;

  CGDataProviderRef provider = CGImageGetDataProvider(image);
  base::mac::ScopedCFTypeRef<CFDataRef> image_data_ref(
      CGDataProviderCopyData(provider));
  if (image_data_ref.get() == NULL) {
    return;
  }
  const uint8* cursor_src_data = CFDataGetBytePtr(image_data_ref);
  int data_size = CFDataGetLength(image_data_ref);

  // Create a CursorShapeInfo proto that describes the cursor and pass it to
  // the client.
  scoped_ptr<protocol::CursorShapeInfo> cursor_proto(
      new protocol::CursorShapeInfo());
  cursor_proto->mutable_data()->resize(data_size);
  uint8* cursor_tgt_data = const_cast<uint8*>(reinterpret_cast<const uint8*>(
      cursor_proto->mutable_data()->data()));

  memcpy(cursor_tgt_data, cursor_src_data, data_size);

  cursor_proto->set_width(size.width);
  cursor_proto->set_height(size.height);
  cursor_proto->set_hotspot_x(hotspot.x);
  cursor_proto->set_hotspot_y(hotspot.y);
  cursor_shape_changed_callback_.Run(cursor_proto.Pass());

  // Record the last cursor image that we sent.
  current_cursor_.reset(CGImageCreateCopy(image));
}

void CapturerMac::GlBlitFast(const VideoFrameBuffer& buffer,
           const SkRegion& region) {
  const int buffer_height = buffer.size().height();
  const int buffer_width = buffer.size().width();

  // Clip to the size of our current screen.
  SkIRect clip_rect = SkIRect::MakeWH(buffer_width, buffer_height);
  if (last_buffer_) {
    // We are doing double buffer for the capture data so we just need to copy
    // the invalid region from the previous capture in the current buffer.
    // TODO(hclam): We can reduce the amount of copying here by subtracting
    // |capturer_helper_|s region from |last_invalid_region_|.
    // http://crbug.com/92354

    // Since the image obtained from OpenGL is upside-down, need to do some
    // magic here to copy the correct rectangle.
    const int y_offset = (buffer_height - 1) * buffer.bytes_per_row();
    for(SkRegion::Iterator i(last_invalid_region_); !i.done(); i.next()) {
      SkIRect copy_rect = i.rect();
      if (copy_rect.intersect(clip_rect)) {
        CopyRect(last_buffer_ + y_offset,
                 -buffer.bytes_per_row(),
                 buffer.ptr() + y_offset,
                 -buffer.bytes_per_row(),
                 4,  // Bytes for pixel for RGBA.
                 copy_rect);
      }
    }
  }
  last_buffer_ = buffer.ptr();
  last_invalid_region_ = region;

  CGLContextObj CGL_MACRO_CONTEXT = cgl_context_;
  glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, pixel_buffer_object_.get());
  glReadPixels(0, 0, buffer_width, buffer_height, GL_BGRA, GL_UNSIGNED_BYTE, 0);
  GLubyte* ptr = static_cast<GLubyte*>(
      glMapBufferARB(GL_PIXEL_PACK_BUFFER_ARB, GL_READ_ONLY_ARB));
  if (ptr == NULL) {
    // If the buffer can't be mapped, assume that it's no longer valid and
    // release it.
    pixel_buffer_object_.Release();
  } else {
    // Copy only from the dirty rects. Since the image obtained from OpenGL is
    // upside-down we need to do some magic here to copy the correct rectangle.
    const int y_offset = (buffer_height - 1) * buffer.bytes_per_row();
    for(SkRegion::Iterator i(region); !i.done(); i.next()) {
      SkIRect copy_rect = i.rect();
      if (copy_rect.intersect(clip_rect)) {
        CopyRect(ptr + y_offset,
           -buffer.bytes_per_row(),
           buffer.ptr() + y_offset,
           -buffer.bytes_per_row(),
           4,  // Bytes for pixel for RGBA.
           copy_rect);
      }
    }
  }
  if (!glUnmapBufferARB(GL_PIXEL_PACK_BUFFER_ARB)) {
    // If glUnmapBuffer returns false, then the contents of the data store are
    // undefined. This might be because the screen mode has changed, in which
    // case it will be recreated in ScreenConfigurationChanged, but releasing
    // the object here is the best option. Capturing will fall back on
    // GlBlitSlow until such time as the pixel buffer object is recreated.
    pixel_buffer_object_.Release();
  }
  glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, 0);
}

void CapturerMac::GlBlitSlow(const VideoFrameBuffer& buffer) {
  CGLContextObj CGL_MACRO_CONTEXT = cgl_context_;
  glReadBuffer(GL_FRONT);
  glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT);
  glPixelStorei(GL_PACK_ALIGNMENT, 4);  // Force 4-byte alignment.
  glPixelStorei(GL_PACK_ROW_LENGTH, 0);
  glPixelStorei(GL_PACK_SKIP_ROWS, 0);
  glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
  // Read a block of pixels from the frame buffer.
  glReadPixels(0, 0, buffer.size().width(), buffer.size().height(),
               GL_BGRA, GL_UNSIGNED_BYTE, buffer.ptr());
  glPopClientAttrib();
}

void CapturerMac::CgBlitPreLion(const VideoFrameBuffer& buffer,
                                const SkRegion& region) {
  const int buffer_height = buffer.size().height();
  const int buffer_width = buffer.size().width();

    // Clip to the size of our current screen.
  SkIRect clip_rect = SkIRect::MakeWH(buffer_width, buffer_height);

  if (last_buffer_)
    memcpy(buffer.ptr(), last_buffer_, buffer.bytes_per_row() * buffer_height);
  last_buffer_ = buffer.ptr();
  CGDirectDisplayID main_display = CGMainDisplayID();
  uint8* display_base_address =
      reinterpret_cast<uint8*>(CGDisplayBaseAddress(main_display));
  int src_bytes_per_row = CGDisplayBytesPerRow(main_display);
  int src_bytes_per_pixel = CGDisplayBitsPerPixel(main_display) / 8;
  // TODO(hclam): We can reduce the amount of copying here by subtracting
  // |capturer_helper_|s region from |last_invalid_region_|.
  // http://crbug.com/92354
  for(SkRegion::Iterator i(region); !i.done(); i.next()) {
    SkIRect copy_rect = i.rect();
    if (copy_rect.intersect(clip_rect)) {
      CopyRect(display_base_address,
               src_bytes_per_row,
               buffer.ptr(),
               buffer.bytes_per_row(),
               src_bytes_per_pixel,
               copy_rect);
    }
  }
}

void CapturerMac::CgBlitPostLion(const VideoFrameBuffer& buffer,
                                 const SkRegion& region) {
  const int buffer_height = buffer.size().height();
  const int buffer_width = buffer.size().width();

  // Clip to the size of our current screen.
  SkIRect clip_rect = SkIRect::MakeWH(buffer_width, buffer_height);

  if (last_buffer_)
    memcpy(buffer.ptr(), last_buffer_, buffer.bytes_per_row() * buffer_height);
  last_buffer_ = buffer.ptr();
  CGDirectDisplayID display = CGMainDisplayID();
  base::mac::ScopedCFTypeRef<CGImageRef> image(
      display_create_image_func_(display));
  if (image.get() == NULL)
    return;
  CGDataProviderRef provider = CGImageGetDataProvider(image);
  base::mac::ScopedCFTypeRef<CFDataRef> data(CGDataProviderCopyData(provider));
  if (data.get() == NULL)
    return;
  const uint8* display_base_address = CFDataGetBytePtr(data);
  int src_bytes_per_row = CGImageGetBytesPerRow(image);
  int src_bytes_per_pixel = CGImageGetBitsPerPixel(image) / 8;
  // TODO(hclam): We can reduce the amount of copying here by subtracting
  // |capturer_helper_|s region from |last_invalid_region_|.
  // http://crbug.com/92354
  for(SkRegion::Iterator i(region); !i.done(); i.next()) {
    SkIRect copy_rect = i.rect();
    if (copy_rect.intersect(clip_rect)) {
      CopyRect(display_base_address,
               src_bytes_per_row,
               buffer.ptr(),
               buffer.bytes_per_row(),
               src_bytes_per_pixel,
               copy_rect);
    }
  }
}

const SkISize& CapturerMac::size_most_recent() const {
  return helper_.size_most_recent();
}

void CapturerMac::ScreenRefresh(CGRectCount count, const CGRect *rect_array) {
  SkIRect skirect_array[count];
  for (CGRectCount i = 0; i < count; ++i) {
    skirect_array[i] = CGRectToSkIRect(rect_array[i]);
  }
  SkRegion region;
  region.setRects(skirect_array, count);
  InvalidateRegion(region);
}

void CapturerMac::ScreenUpdateMove(CGScreenUpdateMoveDelta delta,
                                   size_t count,
                                   const CGRect *rect_array) {
  SkIRect skirect_new_array[count];
  for (CGRectCount i = 0; i < count; ++i) {
    CGRect rect = rect_array[i];
    rect = CGRectOffset(rect, delta.dX, delta.dY);
    skirect_new_array[i] = CGRectToSkIRect(rect);
  }
  SkRegion region;
  region.setRects(skirect_new_array, count);
  InvalidateRegion(region);
}

void CapturerMac::DisplaysReconfigured(CGDirectDisplayID display,
                                       CGDisplayChangeSummaryFlags flags) {
  if (display == CGMainDisplayID()) {
    if (flags & kCGDisplayBeginConfigurationFlag) {
      // Wait on |display_configuration_capture_event_| to prevent more
      // captures from occurring on a different thread while the displays
      // are being reconfigured.
      CHECK(display_configuration_capture_event_.TimedWait(
                base::TimeDelta::FromSeconds(
                    kDisplayReconfigurationTimeoutInSeconds)));
    } else {
      ScreenConfigurationChanged();
      // Now that the configuration has changed, signal the event.
      display_configuration_capture_event_.Signal();
    }
  }
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
  CapturerMac *capturer = reinterpret_cast<CapturerMac *>(user_parameter);
  capturer->DisplaysReconfigured(display, flags);
}

}  // namespace

// static
Capturer* Capturer::Create() {
  CapturerMac* capturer = new CapturerMac();
  if (!capturer->Init()) {
    delete capturer;
    capturer = NULL;
  }
  return capturer;
}

}  // namespace remoting
