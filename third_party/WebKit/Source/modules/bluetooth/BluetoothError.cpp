// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/bluetooth/BluetoothError.h"

#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "public/platform/modules/bluetooth/WebBluetoothError.h"
#include "wtf/OwnPtr.h"

namespace blink {

DOMException* BluetoothError::take(ScriptPromiseResolver*, WebBluetoothError* webErrorRawPointer)
{
    OwnPtr<WebBluetoothError> webError = adoptPtr(webErrorRawPointer);
    switch (webError->errorType) {
    case WebBluetoothError::AbortError:
        return DOMException::create(AbortError, webError->message);
    case WebBluetoothError::InvalidModificationError:
        return DOMException::create(InvalidModificationError, webError->message);
    case WebBluetoothError::InvalidStateError:
        return DOMException::create(InvalidStateError, webError->message);
    case WebBluetoothError::NetworkError:
        return DOMException::create(NetworkError, webError->message);
    case WebBluetoothError::NotFoundError:
        return DOMException::create(NotFoundError, webError->message);
    case WebBluetoothError::NotSupportedError:
        return DOMException::create(NotSupportedError, webError->message);
    case WebBluetoothError::SecurityError:
        return DOMException::create(SecurityError, webError->message);
    case WebBluetoothError::SyntaxError:
        return DOMException::create(SyntaxError, webError->message);
    }
    ASSERT_NOT_REACHED();
    return DOMException::create(UnknownError);
}

void BluetoothError::dispose(WebBluetoothError* webErrorRaw)
{
    delete webErrorRaw;
}

} // namespace blink
