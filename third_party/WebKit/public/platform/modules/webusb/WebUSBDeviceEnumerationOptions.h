// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebUSBDeviceEnumerationOptions_h
#define WebUSBDeviceEnumerationOptions_h

#include "public/platform/WebVector.h"
#include "public/platform/modules/webusb/WebUSBDeviceFilter.h"

namespace blink {

// Options which constrain the set of devices returned in device enumeration.
struct WebUSBDeviceEnumerationOptions {
    WebVector<WebUSBDeviceFilter> filters;
};

} // namespace blink

#endif // WebUSBDeviceEnumerationOptions_h
