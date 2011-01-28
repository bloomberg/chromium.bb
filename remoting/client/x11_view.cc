// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/x11_view.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xrender.h>
#include <X11/extensions/Xcomposite.h>

#include "base/logging.h"
#include "base/task.h"

namespace remoting {

X11View::X11View()
    : display_(NULL),
      window_(0),
      picture_(0) {
}

X11View::~X11View() {
  DCHECK(!display_);
  DCHECK(!window_);
}

bool X11View::Initialize() {
  display_ = XOpenDisplay(NULL);
  if (!display_) {
    return false;
  }

  // Get properties of the screen.
  int screen = DefaultScreen(display_);
  int root_window = RootWindow(display_, screen);

  // Creates the window.
  window_ = XCreateSimpleWindow(display_, root_window, 1, 1, 640, 480, 0,
                                BlackPixel(display_, screen),
                                BlackPixel(display_, screen));
  DCHECK(window_);
  XStoreName(display_, window_, "X11 Remoting");

  // Specifies what kind of messages we want to receive.
  XSelectInput(display_,
               window_,
               ExposureMask
               | KeyPressMask | KeyReleaseMask
               | ButtonPressMask | ButtonReleaseMask
               | PointerMotionMask);

  XMapWindow(display_, window_);
  return true;
}

void X11View::TearDown() {
  if (display_ && window_) {
    // Shutdown the window system.
    XDestroyWindow(display_, window_);
    XCloseDisplay(display_);
  }
  display_ = NULL;
  window_ = 0;
}

void X11View::Paint() {
  NOTIMPLEMENTED() << "Not sure if we need this call anymore.";
}

void X11View::PaintRect(media::VideoFrame* frame, const gfx::Rect& clip) {
  // Don't bother attempting to paint if the display hasn't been set up.
  if (!display_ || !window_ || !frame) {
    return;
  }

  // If we have not initialized the render target then do it now.
  if (!picture_)
    InitPaintTarget();

  // Upload the image to a pixmap. And then create a picture from the pixmap
  // and composite the picture over the picture representing the window.

  // Creates a XImage.
  XImage image;
  memset(&image, 0, sizeof(image));
  image.width = frame->width();
  image.height = frame->height();
  image.depth = 32;
  image.bits_per_pixel = 32;
  image.format = ZPixmap;
  image.byte_order = LSBFirst;
  image.bitmap_unit = 8;
  image.bitmap_bit_order = LSBFirst;
  image.bytes_per_line = frame_->stride(media::VideoFrame::kRGBPlane);
  image.red_mask = 0xff;
  image.green_mask = 0xff00;
  image.blue_mask = 0xff0000;
  image.data = reinterpret_cast<char*>(
      frame_->data(media::VideoFrame::kRGBPlane));

  // Creates a pixmap and uploads from the XImage.
  unsigned long pixmap = XCreatePixmap(display_, window_,
                                       frame->width(), frame->height(), 32);

  GC gc = XCreateGC(display_, pixmap, 0, NULL);
  XPutImage(display_, pixmap, gc, &image, clip.x(), clip.y(),
            clip.x(), clip.y(), clip.width(), clip.height());
  XFreeGC(display_, gc);

  // Creates the picture representing the pixmap.
  XID picture = XRenderCreatePicture(
      display_, pixmap,
      XRenderFindStandardFormat(display_, PictStandardARGB32),
      0, NULL);

  // Composite the picture over the picture representing the window.
  XRenderComposite(display_, PictOpSrc, picture, 0,
                   picture_, 0, 0, 0, 0, clip.x(), clip.y(),
                   clip.width(), clip.height());

  XRenderFreePicture(display_, picture);
  XFreePixmap(display_, pixmap);
}

void X11View::SetSolidFill(uint32 color) {
  // TODO(garykac): Implement.
  // NOTIMPLEMENTED();
}

void X11View::UnsetSolidFill() {
  // TODO(garykac): Implement.
  // NOTIMPLEMENTED();
}

void X11View::SetConnectionState(ConnectionState s) {
  // TODO(garykac): Implement.
}

void X11View::UpdateLoginStatus(bool success, const std::string& info) {
  NOTIMPLEMENTED();
}

void X11View::SetViewport(int x, int y, int width, int height) {
  // TODO(garykac): Implement.
}

void X11View::InitPaintTarget() {
  // Testing XRender support.
  int dummy;
  bool xrender_support = XRenderQueryExtension(display_, &dummy, &dummy);
  CHECK(xrender_support) << "XRender is not supported!";

  XWindowAttributes attr;
  XGetWindowAttributes(display_, window_, &attr);

  XRenderPictFormat* pictformat = XRenderFindVisualFormat(
      display_,
      attr.visual);
  CHECK(pictformat) << "XRENDER does not support default visual";

  picture_ = XRenderCreatePicture(display_, window_, pictformat, 0, NULL);
  CHECK(picture_) << "Backing picture not created";
}

void X11View::AllocateFrame(media::VideoFrame::Format format,
                            size_t width,
                            size_t height,
                            base::TimeDelta timestamp,
                            base::TimeDelta duration,
                            scoped_refptr<media::VideoFrame>* frame_out,
                            Task* done) {
  // TODO(ajwong): Implement this to use the native X window rather than
  // just a generic frame buffer.
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

void X11View::ReleaseFrame(media::VideoFrame* frame) {
  if (frame) {
    LOG(WARNING) << "Frame released.";
    frame->Release();
  }
}

void X11View::OnPartialFrameOutput(media::VideoFrame* frame,
                                   UpdatedRects* rects,
                                   Task* done) {
  // TODO(hclam): Make sure we call this method on the right thread. Since
  // decoder is single-threaded we don't have a problem but we better post
  // a task to do the right thing.

  for (UpdatedRects::iterator it = rects->begin(); it != rects->end(); ++it) {
    PaintRect(frame, *it);
  }

  // TODO(ajwong): Shouldn't we only expose the part of the window that was
  // damanged?
  XEvent event;
  event.type = Expose;
  XSendEvent(display_, static_cast<int>(window_), true, ExposureMask, &event);

  done->Run();
  delete done;
}

}  // namespace remoting
