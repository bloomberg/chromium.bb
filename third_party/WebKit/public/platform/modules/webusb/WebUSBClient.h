// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebUSBClient_h
#define WebUSBClient_h

#include "public/platform/WebCallbacks.h"
#include "public/platform/WebVector.h"

namespace blink {

class WebUSBDevice;
struct WebUSBDeviceRequestOptions;
struct WebUSBError;

using WebUSBClientGetDevicesCallbacks = WebCallbacks<WebPassOwnPtr<WebVector<WebUSBDevice*>>, const WebUSBError&>;
using WebUSBClientRequestDeviceCallbacks = WebCallbacks<WebPassOwnPtr<WebUSBDevice>, const WebUSBError&>;

class WebUSBClient {
public:
    virtual ~WebUSBClient() { }

    // Enumerates available devices.
    // Ownership of the WebUSBClientGetDevicesCallbacks is transferred to the client.
    virtual void getDevices(WebUSBClientGetDevicesCallbacks*) = 0;

    // Requests access to a device.
    // Ownership of the WebUSBClientRequestDeviceCallbacks is transferred to the client.
    virtual void requestDevice(const WebUSBDeviceRequestOptions&, WebUSBClientRequestDeviceCallbacks*) = 0;
};

} // namespace blink

#endif // WebUSBClient_h
