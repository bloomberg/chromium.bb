// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/x_server_pixel_buffer.h"

#include <gdk/gdk.h>
#include <sys/shm.h>

#include "base/logging.h"

namespace remoting {

XServerPixelBuffer::XServerPixelBuffer()
    : display_(NULL), root_window_(0), x_image_(NULL),
      shm_segment_info_(NULL), shm_pixmap_(0), shm_gc_(NULL) {
}

XServerPixelBuffer::~XServerPixelBuffer() {
  Release();
}

void XServerPixelBuffer::Release() {
  if (x_image_) {
    XDestroyImage(x_image_);
    x_image_ = NULL;
  }
  if (shm_pixmap_) {
    XFreePixmap(display_, shm_pixmap_);
    shm_pixmap_ = 0;
  }
  if (shm_gc_) {
    XFreeGC(display_, shm_gc_);
    shm_gc_ = NULL;
  }
  if (shm_segment_info_) {
    if (shm_segment_info_->shmaddr != reinterpret_cast<char*>(-1))
      shmdt(shm_segment_info_->shmaddr);
    if (shm_segment_info_->shmid != -1)
      shmctl(shm_segment_info_->shmid, IPC_RMID, 0);
    delete shm_segment_info_;
    shm_segment_info_ = NULL;
  }
}

void XServerPixelBuffer::Init(Display* display) {
  Release();
  display_ = display;
  int default_screen = DefaultScreen(display_);
  root_window_ = RootWindow(display_, default_screen);
  InitShm(default_screen);
}

void XServerPixelBuffer::InitShm(int screen) {
  XWindowAttributes root_attr;
  XGetWindowAttributes(display_, root_window_, &root_attr);
  int width = root_attr.width, height = root_attr.height;
  Visual* default_visual = DefaultVisual(display_, screen);
  int default_depth = DefaultDepth(display_, screen);

  int major, minor;
  Bool havePixmaps;
  if (!XShmQueryVersion(display_, &major, &minor, &havePixmaps))
    // Shared memory not supported. CaptureRect will use the XImage API instead.
    return;

  bool using_shm = false;
  shm_segment_info_ = new XShmSegmentInfo;
  shm_segment_info_->shmid = -1;
  shm_segment_info_->shmaddr = reinterpret_cast<char*>(-1);
  shm_segment_info_->readOnly = False;
  x_image_ = XShmCreateImage(display_, default_visual, default_depth, ZPixmap,
                             0, shm_segment_info_, width, height);
  if (x_image_) {
    shm_segment_info_->shmid = shmget(
        IPC_PRIVATE, x_image_->bytes_per_line * x_image_->height,
        IPC_CREAT | 0666);
    if (shm_segment_info_->shmid != -1) {
      shm_segment_info_->shmaddr = x_image_->data =
          reinterpret_cast<char*>(shmat(shm_segment_info_->shmid, 0, 0));
      if (x_image_->data != reinterpret_cast<char*>(-1)) {
        gdk_error_trap_push();
        using_shm = XShmAttach(display_, shm_segment_info_);
        XSync(display_, False);
        if (gdk_error_trap_pop() != 0)
          using_shm = false;
      }
    }
  }

  if (!using_shm) {
    VLOG(1) << "Not using shared memory.";
    Release();
    return;
  }

  if (havePixmaps)
    havePixmaps = InitPixmaps(width, height, default_depth);

  shmctl(shm_segment_info_->shmid, IPC_RMID, 0);
  shm_segment_info_->shmid = -1;

  VLOG(1) << "Using X shared memory extension v" << major << "." << minor
          << " with" << (havePixmaps?"":"out") << " pixmaps.";
}

bool XServerPixelBuffer::InitPixmaps(int width, int height, int depth) {
  if (XShmPixmapFormat(display_) != ZPixmap)
    return false;

  gdk_error_trap_push();
  shm_pixmap_ = XShmCreatePixmap(display_, root_window_,
                                 shm_segment_info_->shmaddr,
                                 shm_segment_info_,
                                 width, height, depth);
  XSync(display_, False);
  if (gdk_error_trap_pop() != 0) {
    // |shm_pixmap_| is not not valid because the request was not processed
    // by the X Server, so zero it.
    shm_pixmap_ = 0;
    return false;
  }

  gdk_error_trap_push();
  XGCValues shm_gc_values;
  shm_gc_values.subwindow_mode = IncludeInferiors;
  shm_gc_values.graphics_exposures = False;
  shm_gc_ = XCreateGC(display_, root_window_,
                      GCSubwindowMode | GCGraphicsExposures,
                      &shm_gc_values);
  XSync(display_, False);
  if (gdk_error_trap_pop() != 0) {
    XFreePixmap(display_, shm_pixmap_);
    shm_pixmap_ = 0;
    shm_gc_ = 0;  // See shm_pixmap_ comment above.
    return false;
  }

  return true;
}

void XServerPixelBuffer::Synchronize() {
  if (shm_segment_info_ && !shm_pixmap_) {
    // XShmGetImage can fail if the display is being reconfigured.
    gdk_error_trap_push();
    XShmGetImage(display_, root_window_, x_image_, 0, 0, AllPlanes);
    gdk_error_trap_pop();
  }
}

uint8* XServerPixelBuffer::CaptureRect(const SkIRect& rect) {
  if (shm_segment_info_) {
    if (shm_pixmap_) {
      XCopyArea(display_, root_window_, shm_pixmap_, shm_gc_,
                rect.fLeft, rect.fTop, rect.width(), rect.height(),
                rect.fLeft, rect.fTop);
      XSync(display_, False);
    }
    return reinterpret_cast<uint8*>(x_image_->data) +
        rect.fTop * x_image_->bytes_per_line +
        rect.fLeft * x_image_->bits_per_pixel / 8;
  } else {
    if (x_image_)
      XDestroyImage(x_image_);
    x_image_ = XGetImage(display_, root_window_, rect.fLeft, rect.fTop,
                         rect.width(), rect.height(), AllPlanes, ZPixmap);
    return reinterpret_cast<uint8*>(x_image_->data);
  }
}

int XServerPixelBuffer::GetStride() const {
  return x_image_->bytes_per_line;
}

int XServerPixelBuffer::GetDepth() const {
  return x_image_->depth;
}

int XServerPixelBuffer::GetBitsPerPixel() const {
  return x_image_->bits_per_pixel;
}

int XServerPixelBuffer::GetRedMask() const {
  return x_image_->red_mask;
}

int XServerPixelBuffer::GetBlueMask() const {
  return x_image_->blue_mask;
}

int XServerPixelBuffer::GetGreenMask() const {
  return x_image_->green_mask;
}

int XServerPixelBuffer::GetRedShift() const {
  return ffs(x_image_->red_mask) - 1;
}

int XServerPixelBuffer::GetBlueShift() const {
  return ffs(x_image_->blue_mask) - 1;
}

int XServerPixelBuffer::GetGreenShift() const {
  return ffs(x_image_->green_mask) - 1;
}

bool XServerPixelBuffer::IsRgb() const {
  return GetRedShift() == 16 && GetGreenShift() == 8 && GetBlueShift() == 0;
}

}  // namespace remoting
