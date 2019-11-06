// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/push_messaging/push_error.h"

#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {

DOMException* PushError::Take(ScriptPromiseResolver* resolver,
                              const WebPushError& web_error) {
  switch (web_error.error_type) {
    case WebPushError::kErrorTypeAbort:
      return MakeGarbageCollected<DOMException>(DOMExceptionCode::kAbortError,
                                                web_error.message);
    case WebPushError::kErrorTypeInvalidState:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kInvalidStateError, web_error.message);
    case WebPushError::kErrorTypeNetwork:
      return MakeGarbageCollected<DOMException>(DOMExceptionCode::kNetworkError,
                                                web_error.message);
    case WebPushError::kErrorTypeNone:
      NOTREACHED();
      return MakeGarbageCollected<DOMException>(DOMExceptionCode::kUnknownError,
                                                web_error.message);
    case WebPushError::kErrorTypeNotAllowed:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotAllowedError, web_error.message);
    case WebPushError::kErrorTypeNotFound:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotFoundError, web_error.message);
    case WebPushError::kErrorTypeNotSupported:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError, web_error.message);
  }
  NOTREACHED();
  return MakeGarbageCollected<DOMException>(DOMExceptionCode::kUnknownError);
}

}  // namespace blink
