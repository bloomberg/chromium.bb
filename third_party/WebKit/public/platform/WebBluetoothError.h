// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebBluetoothError_h
#define WebBluetoothError_h

#include "WebString.h"

namespace blink {

// Error object when a bluetooth request can not be satisfied used to create
// DOMExceptions.
struct WebBluetoothError {
    enum ErrorType {
        NotFoundError,
        SecurityError
    };

    WebBluetoothError(ErrorType errorType, const WebString& message)
        : errorType(errorType)
        , message(message)
    {
    }

    ErrorType errorType;
    WebString message;
};

} // namespace blink

#endif // WebBluetoothError_h
