// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webusb/USBError.h"

#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "public/platform/modules/webusb/WebUSBError.h"

namespace blink {

DOMException* USBError::take(ScriptPromiseResolver*, const WebUSBError& webError)
{
    switch (webError.error) {
    case WebUSBError::Error::InvalidState:
        return DOMException::create(InvalidStateError, webError.message);
    case WebUSBError::Error::Network:
        return DOMException::create(NetworkError, webError.message);
    case WebUSBError::Error::NotFound:
        return DOMException::create(NotFoundError, webError.message);
    case WebUSBError::Error::Security:
        return DOMException::create(SecurityError, webError.message);
    }
    ASSERT_NOT_REACHED();
    return DOMException::create(UnknownError);
}

} // namespace blink
