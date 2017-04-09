// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebPresentationError_h
#define WebPresentationError_h

#include "public/platform/WebString.h"

namespace blink {

struct WebPresentationError {
  enum ErrorType {
    kErrorTypeNoAvailableScreens = 0,
    kErrorTypePresentationRequestCancelled,
    kErrorTypeNoPresentationFound,
    kErrorTypeAvailabilityNotSupported,
    kErrorTypePreviousStartInProgress,
    kErrorTypeUnknown,
    kErrorTypeLast = kErrorTypeUnknown
  };

  WebPresentationError(ErrorType error_type, const WebString& message)
      : error_type(error_type), message(message) {}

  ErrorType error_type;
  WebString message;
};

}  // namespace blink

#endif  // WebPresentationError_h
