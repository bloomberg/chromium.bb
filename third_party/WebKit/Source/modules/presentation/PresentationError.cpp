// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/presentation/PresentationError.h"

#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "public/platform/modules/presentation/WebPresentationError.h"

namespace blink {

// static
DOMException* PresentationError::Take(const WebPresentationError& error) {
  ExceptionCode code = kUnknownError;
  switch (error.error_type) {
    case WebPresentationError::kErrorTypeNoAvailableScreens:
    case WebPresentationError::kErrorTypeNoPresentationFound:
      code = kNotFoundError;
      break;
    case WebPresentationError::kErrorTypePresentationRequestCancelled:
      code = kNotAllowedError;
      break;
    case WebPresentationError::kErrorTypeAvailabilityNotSupported:
      code = kNotSupportedError;
      break;
    case WebPresentationError::kErrorTypePreviousStartInProgress:
      code = kOperationError;
      break;
    case WebPresentationError::kErrorTypeUnknown:
      code = kUnknownError;
      break;
  }

  return DOMException::Create(code, error.message);
}

}  // namespace blink
