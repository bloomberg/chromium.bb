// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/touchui/touch_factory.h"

#include <gdk/gdkx.h>
#include <X11/extensions/XInput2.h>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "ui/base/x/x11_util.h"

// The X cursor is hidden if it is idle for kCursorIdleSeconds seconds.
static int kCursorIdleSeconds = 5;

namespace views {

// static
TouchFactory* TouchFactory::GetInstance() {
  return Singleton<TouchFactory>::get();
}

TouchFactory::TouchFactory()
    : is_cursor_visible_(true),
      cursor_timer_(),
      touch_device_list_() {
  Pixmap blank;
  XColor black;
  static char nodata[] = { 0,0,0,0,0,0,0,0 };
  black.red = black.green = black.blue = 0;
  Display* display = ui::GetXDisplay();

  blank = XCreateBitmapFromData(display, ui::GetX11RootWindow(), nodata, 8, 8);
  invisible_cursor_ = XCreatePixmapCursor(display, blank, blank,
                                          &black, &black, 0, 0);

  SetCursorVisible(false, false);
}

TouchFactory::~TouchFactory() {
  SetCursorVisible(true, false);
  XFreeCursor(ui::GetXDisplay(), invisible_cursor_);
}

void TouchFactory::SetTouchDeviceList(
    const std::vector<unsigned int>& devices) {
  touch_device_lookup_.reset();
  touch_device_list_.clear();
  for (std::vector<unsigned int>::const_iterator iter = devices.begin();
       iter != devices.end(); ++iter) {
    DCHECK(*iter < touch_device_lookup_.size());
    touch_device_lookup_[*iter] = true;
    touch_device_list_.push_back(*iter);
  }
}

bool TouchFactory::IsTouchDevice(unsigned deviceid) {
  return deviceid < touch_device_lookup_.size() ?
      touch_device_lookup_[deviceid] : false;
}

bool TouchFactory::GrabTouchDevices(Display* display, ::Window window) {
  if (touch_device_list_.empty())
    return true;

  unsigned char mask[(XI_LASTEVENT + 7) / 8];
  bool success = true;

  memset(mask, 0, sizeof(mask));
  XISetMask(mask, XI_ButtonPress);
  XISetMask(mask, XI_ButtonRelease);
  XISetMask(mask, XI_Motion);

  XIEventMask evmask;
  evmask.mask_len = sizeof(mask);
  evmask.mask = mask;
  for (std::vector<int>::const_iterator iter =
       touch_device_list_.begin();
       iter != touch_device_list_.end(); ++iter) {
    evmask.deviceid = *iter;
    Status status = XIGrabDevice(display, *iter, window, CurrentTime, None,
                                 GrabModeAsync, GrabModeAsync, False, &evmask);
    success = success && status == GrabSuccess;
  }

  return success;
}

bool TouchFactory::UngrabTouchDevices(Display* display) {
  bool success = true;
  for (std::vector<int>::const_iterator iter =
       touch_device_list_.begin();
       iter != touch_device_list_.end(); ++iter) {
    Status status = XIUngrabDevice(display, *iter, CurrentTime);
    success = success && status == GrabSuccess;
  }
  return success;
}

void TouchFactory::SetCursorVisible(bool show, bool start_timer) {
  // The cursor is going to be shown. Reset the timer for hiding it.
  if (show && start_timer) {
    cursor_timer_.Stop();
    cursor_timer_.Start(base::TimeDelta::FromSeconds(kCursorIdleSeconds),
        this, &TouchFactory::HideCursorForInactivity);
  } else {
    cursor_timer_.Stop();
  }

  if (show == is_cursor_visible_)
    return;

  is_cursor_visible_ = show;

  GdkDisplay* display = gdk_display_get_default();
  if (!display)
    return;

  Display* xdisplay = GDK_DISPLAY_XDISPLAY(display);
  Window window = DefaultRootWindow(xdisplay);

  if (is_cursor_visible_) {
    XUndefineCursor(xdisplay, window);
  } else {
    XDefineCursor(xdisplay, window, invisible_cursor_);
  }
}

}  // namespace views
