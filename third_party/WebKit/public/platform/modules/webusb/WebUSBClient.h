// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebUSBClient_h
#define WebUSBClient_h

#include "public/platform/WebCallbacks.h"
#include "public/platform/WebVector.h"

namespace blink {

class WebUSBDevice;
struct WebUSBDeviceEnumerationOptions;
struct WebUSBError;

using WebUSBClientGetDevicesCallbacks = WebCallbacks<WebPassOwnPtr<WebVector<WebUSBDevice*>>, const WebUSBError&>;

class WebUSBClient {
public:
    virtual ~WebUSBClient() { }

    // Enumerates available devices.
    // Ownership of the WebUSBClientGetDevicesCallbacks is transferred to the client.
    virtual void getDevices(const WebUSBDeviceEnumerationOptions&, WebUSBClientGetDevicesCallbacks*) = 0;
};

} // namespace blink

#endif // WebUSBClient_h
