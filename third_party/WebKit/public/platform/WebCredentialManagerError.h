// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebCredentialManagerError_h
#define WebCredentialManagerError_h

#include "public/platform/WebString.h"

namespace blink {

struct WebCredentialManagerError {
    // FIXME: This is a placeholder list of error conditions. We'll likely expand the
    // list as the API evolves.
    enum ErrorType {
        ErrorTypeDisabled = 0,
        ErrorTypeUnknown,
        ErrorTypeLast = ErrorTypeUnknown
    };

    WebCredentialManagerError(ErrorType type, WebString message)
        : errorType(type)
        , errorMessage(message)
    {
    }

    ErrorType errorType;
    WebString errorMessage;
};

} // namespace blink

#endif
