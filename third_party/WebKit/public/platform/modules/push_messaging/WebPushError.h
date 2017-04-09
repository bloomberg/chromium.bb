// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebPushError_h
#define WebPushError_h

#include "public/platform/WebString.h"

namespace blink {

struct WebPushError {
  enum ErrorType {
    kErrorTypeAbort = 0,
    kErrorTypeNetwork,
    kErrorTypeNone,
    kErrorTypeNotAllowed,
    kErrorTypeNotFound,
    kErrorTypeNotSupported,
    kErrorTypeInvalidState,
    kErrorTypeLast = kErrorTypeInvalidState
  };

  WebPushError(ErrorType error_type, const WebString& message)
      : error_type(error_type), message(message) {}

  ErrorType error_type;
  WebString message;
};

}  // namespace blink

#endif  // WebPushError_h
