// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_LIBUSB_LIBUSB_H_
#define THIRD_PARTY_LIBUSB_LIBUSB_H_

#if defined(USE_SYSTEM_LIBUSB)
#include <libusb.h>
#else
// Relative to '.' which is in the include path.
#include "src/libusb/libusb.h"
#endif

#endif  // THIRD_PARTY_LIBUSB_LIBUSB_H_
