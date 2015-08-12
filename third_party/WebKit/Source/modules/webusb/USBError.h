// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef USBError_h
#define USBError_h

#include "platform/heap/Handle.h"
#include "public/platform/modules/webusb/WebUSBError.h"
#include "wtf/PassOwnPtr.h"

namespace blink {

class DOMException;
class ScriptPromiseResolver;

// USBError is used with CallbackPromiseAdapter to receive WebUSBError
// responses. See CallbackPromiseAdapter class comments.
class USBError {
    WTF_MAKE_NONCOPYABLE(USBError);
public:
    // Interface required by CallbackPromiseAdapter:
    using WebType = const WebUSBError&;
    static DOMException* take(ScriptPromiseResolver*, const WebUSBError&);

private:
    USBError() = delete;
};

} // namespace blink

#endif // USBError_h
