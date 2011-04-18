// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/touchui/touch_factory.h"

#include <X11/cursorfont.h>
#include <X11/extensions/XInput.h>
#include <X11/extensions/XInput2.h>
#include <X11/extensions/XIproto.h>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "ui/base/x/x11_util.h"

// The X cursor is hidden if it is idle for kCursorIdleSeconds seconds.
static int kCursorIdleSeconds = 5;

namespace views {

namespace {

// Given the TouchParam, return the correspoding valuator index using
// the X device information through Atom name matching.
char FindTPValuator(Display* display,
                    XIDeviceInfo* info,
                    TouchFactory::TouchParam touch_param) {
  // Lookup table for mapping TouchParam to Atom string used in X.
  // A full set of Atom strings can be found at xserver-properties.h.
  static struct {
    TouchFactory::TouchParam tp;
    const char* atom;
  } kTouchParamAtom[] = {
    { TouchFactory::TP_TOUCH_MAJOR, "Abs MT Touch Major" },
    { TouchFactory::TP_TOUCH_MINOR, "Abs MT Touch Minor" },
    { TouchFactory::TP_ORIENTATION, "Abs MT Orientation" },
    { TouchFactory::TP_LAST_ENTRY, NULL },
  };

  const char* atom_tp = NULL;

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTouchParamAtom); i++) {
    if (touch_param == kTouchParamAtom[i].tp) {
      atom_tp = kTouchParamAtom[i].atom;
      break;
    }
  }

  if (!atom_tp)
    return -1;

  for (int i = 0; i < info->num_classes; i++) {
    if (info->classes[i]->type != XIValuatorClass)
      continue;
    XIValuatorClassInfo* v =
        reinterpret_cast<XIValuatorClassInfo*>(info->classes[i]);

    const char* atom = XGetAtomName(display, v->label);

    if (atom && strcmp(atom, atom_tp) == 0)
      return v->number;
  }

  return -1;
}

}  // namespace

// static
TouchFactory* TouchFactory::GetInstance() {
  return Singleton<TouchFactory>::get();
}

TouchFactory::TouchFactory()
    : is_cursor_visible_(true),
      cursor_timer_(),
      touch_device_list_() {
  char nodata[] = { 0, 0, 0, 0, 0, 0, 0, 0 };
  XColor black;
  black.red = black.green = black.blue = 0;
  Display* display = ui::GetXDisplay();
  Pixmap blank = XCreateBitmapFromData(display, ui::GetX11RootWindow(),
                                       nodata, 8, 8);
  invisible_cursor_ = XCreatePixmapCursor(display, blank, blank,
                                          &black, &black, 0, 0);
  arrow_cursor_ = XCreateFontCursor(display, XC_arrow);

  SetCursorVisible(false, false);

  // Detect touch devices.
  // NOTE: The new API for retrieving the list of devices (XIQueryDevice) does
  // not provide enough information to detect a touch device. As a result, the
  // old version of query function (XListInputDevices) is used instead.
  // If XInput2 is not supported, this will return null (with count of -1) so
  // we assume there cannot be any touch devices.
  int count = 0;
  XDeviceInfo* devlist = XListInputDevices(display, &count);
  for (int i = 0; i < count; i++) {
    const char* devtype = XGetAtomName(display, devlist[i].type);
    if (devtype && !strcmp(devtype, XI_TOUCHSCREEN)) {
      touch_device_lookup_[devlist[i].id] = true;
      touch_device_list_.push_back(devlist[i].id);
    }
  }
  if (devlist)
    XFreeDeviceList(devlist);

  SetupValuator();
}

TouchFactory::~TouchFactory() {
  SetCursorVisible(true, false);
  Display* display = ui::GetXDisplay();
  XFreeCursor(display, invisible_cursor_);
  XFreeCursor(display, arrow_cursor_);
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

  SetupValuator();
}

bool TouchFactory::IsTouchDevice(unsigned deviceid) const {
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

  Display* display = ui::GetXDisplay();
  Window window = DefaultRootWindow(display);

  if (is_cursor_visible_) {
    XDefineCursor(display, window, arrow_cursor_);
  } else {
    XDefineCursor(display, window, invisible_cursor_);
  }
}

void TouchFactory::SetupValuator() {
  memset(valuator_lookup_, -1, sizeof(valuator_lookup_));

  Display* display = ui::GetXDisplay();
  int ndevice;
  XIDeviceInfo* info_list = XIQueryDevice(display, XIAllDevices, &ndevice);

  for (int i = 0; i < ndevice; i++) {
    XIDeviceInfo* info = info_list + i;

    if (!IsTouchDevice(info->deviceid))
      continue;

    for (int i = 0; i < TP_LAST_ENTRY; i++) {
      TouchParam tp = static_cast<TouchParam>(i);
      valuator_lookup_[info->deviceid][i] = FindTPValuator(display, info, tp);
    }
  }

  if (info_list)
    XIFreeDeviceInfo(info_list);
}

bool TouchFactory::ExtractTouchParam(const XEvent& xev,
                                     TouchParam tp,
                                     float* value) {
  XIDeviceEvent* xiev = static_cast<XIDeviceEvent*>(xev.xcookie.data);
  if (xiev->sourceid >= kMaxDeviceNum)
    return false;
  int v = valuator_lookup_[xiev->sourceid][tp];
  if (v >= 0 && XIMaskIsSet(xiev->valuators.mask, v)) {
    *value = xiev->valuators.values[v];
    return true;
  }

  return false;
}

}  // namespace views
