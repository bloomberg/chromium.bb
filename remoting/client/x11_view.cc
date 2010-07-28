// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/x11_view.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xrender.h>
#include <X11/extensions/Xcomposite.h>

#include "base/logging.h"
#include "remoting/base/decoder_verbatim.h"

namespace remoting {

X11View::X11View()
    : display_(NULL),
      window_(0),
      width_(0),
      height_(0),
      picture_(0) {
}

X11View::X11View(Display* display, XID window, int width, int height)
    : display_(display),
      window_(window),
      width_(width),
      height_(height),
      picture_(0) {
  media::VideoFrame::CreateFrame(media::VideoFrame::RGB32, width_, height_,
                                 base::TimeDelta(), base::TimeDelta(), &frame_);
  DCHECK(frame_);
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
  XSelectInput(display_, window_, ExposureMask | ButtonPressMask);
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
  // Don't bother attempting to paint if the display hasn't been set up.
  if (!display_ || !window_ || !height_ || !width_ || !frame_) {
    return;
  }

  // TODO(hclam): Paint only the updated regions.
  all_update_rects_.clear();

  // If we have not initialized the render target then do it now.
  if (!picture_)
    InitPaintTarget();

  // Upload the image to a pixmap. And then create a picture from the pixmap
  // and composite the picture over the picture representing the window.

  // Creates a XImage.
  XImage image;
  memset(&image, 0, sizeof(image));
  image.width = width_;
  image.height = height_;
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
  unsigned long pixmap = XCreatePixmap(display_, window_, width_, height_, 32);

  GC gc = XCreateGC(display_, pixmap, 0, NULL);
  XPutImage(display_, pixmap, gc, &image, 0, 0, 0, 0, width_, height_);
  XFreeGC(display_, gc);

  // Creates the picture representing the pixmap.
  XID picture = XRenderCreatePicture(
      display_, pixmap,
      XRenderFindStandardFormat(display_, PictStandardARGB32),
      0, NULL);

  // Composite the picture over the picture representing the window.
  XRenderComposite(display_, PictOpSrc, picture, 0,
                   picture_, 0, 0, 0, 0, 0, 0,
                   width_, height_);

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

void X11View::SetViewport(int x, int y, int width, int height) {
  // TODO(garykac): Implement.
  // NOTIMPLEMENTED();
}

void X11View::SetHostScreenSize(int width, int height) {
  width_ = width;
  height_ = height;
  XResizeWindow(display_, window_, width_, height_);
  media::VideoFrame::CreateFrame(media::VideoFrame::RGB32, width_, height_,
                                 base::TimeDelta(), base::TimeDelta(), &frame_);
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

void X11View::HandleBeginUpdateStream(HostMessage* msg) {
  scoped_ptr<HostMessage> deleter(msg);

  // TODO(hclam): Use the information from the message to create the decoder.
  // We lazily construct the decoder.
  if (!decoder_.get()) {
    decoder_.reset(new DecoderVerbatim());
  }

  // Tell the decoder to do start decoding.
  decoder_->BeginDecode(frame_, &update_rects_,
      NewRunnableMethod(this, &X11View::OnPartialDecodeDone),
      NewRunnableMethod(this, &X11View::OnDecodeDone));
}

void X11View::HandleUpdateStreamPacket(HostMessage* msg) {
  decoder_->PartialDecode(msg);
}

void X11View::HandleEndUpdateStream(HostMessage* msg) {
  scoped_ptr<HostMessage> deleter(msg);
  decoder_->EndDecode();
}

void X11View::OnPartialDecodeDone() {
  // Decoder has produced some output so schedule a paint. We'll get a Paint()
  // call in the near future. Note that we can get UpdateStreamPacket during
  // this short period of time and we will perform decode again and the
  // information in updated rects will be lost.
  // There are several ways to solve this problem.
  // 1. Merge the updated rects and perform one paint.
  // 2. Queue the updated rects and perform two paints.
  // 3. Ignore the updated rects and always paint the full image. Since we
  //    use one frame as output this will always be correct.
  // We will take (1) and simply concat the list of rectangles.
  all_update_rects_.insert(all_update_rects_.begin() +
                           all_update_rects_.size(),
                           update_rects_.begin(), update_rects_.end());

  // TODO(hclam): Make sure we call this method on the right thread. Since
  // decoder is single-threaded we don't have a problem but we better post
  // a task to do the right thing.
  XEvent event;
  event.type = Expose;
  XSendEvent(display_, static_cast<int>(window_), true, ExposureMask, &event);
}

void X11View::OnDecodeDone() {
  // Since we do synchronous decoding here there's nothing in this method.
}

}  // namespace remoting
