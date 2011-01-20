// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/touchui/touch_factory.h"

#include <gdk/gdkx.h>
#include <X11/extensions/XInput2.h>

#include "base/logging.h"

namespace views {

// static
TouchFactory* TouchFactory::GetInstance() {
  return Singleton<TouchFactory>::get();
}

TouchFactory::TouchFactory()
    : touch_device_lookup_(),
      touch_device_list_() {
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

}  // namespace views
