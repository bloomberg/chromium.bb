// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebUSBDeviceRequestOptions_h
#define WebUSBDeviceRequestOptions_h

#include "public/platform/WebVector.h"
#include "public/platform/modules/webusb/WebUSBDeviceFilter.h"

namespace blink {

// Options which constrain the kind of devices the user is prompted to select.
struct WebUSBDeviceRequestOptions {
    WebVector<WebUSBDeviceFilter> filters;
};

} // namespace blink

#endif // WebUSBDeviceRequestOptions_h
