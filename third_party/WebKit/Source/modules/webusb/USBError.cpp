// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/webusb/USBError.h"

#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "public/platform/modules/webusb/WebUSBError.h"

namespace blink {

DOMException* USBError::take(ScriptPromiseResolver*, const WebUSBError& webError)
{
    switch (webError.error) {
    case WebUSBError::Error::Device:
    case WebUSBError::Error::Security:
    case WebUSBError::Error::Service:
    case WebUSBError::Error::Transfer:
        // TODO(rockot): Differentiate between different error types.
        return DOMException::create(AbortError, webError.message);
    }
    ASSERT_NOT_REACHED();
    return DOMException::create(UnknownError);
}

} // namespace blink
