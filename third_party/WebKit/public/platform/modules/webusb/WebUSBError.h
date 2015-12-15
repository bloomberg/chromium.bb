// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebUSBError_h
#define WebUSBError_h

#include "public/platform/WebString.h"

namespace blink {

// Error object used to create DOMExceptions when a Web USB request cannot be
// satisfied.
struct WebUSBError {
    enum class Error {
        Network,
        NotFound,
        Security,
    };

    WebUSBError(Error error, const WebString& message)
        : error(error)
        , message(message)
    {
    }

    Error error;
    WebString message;
};

} // namespace blink

#endif // WebUSBError_h
