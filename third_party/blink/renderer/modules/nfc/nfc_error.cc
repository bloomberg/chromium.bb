// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/nfc/nfc_error.h"

#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

using device::mojom::blink::NFCErrorType;

namespace blink {

DOMException* NFCError::Take(ScriptPromiseResolver*,
                             const NFCErrorType& error_type) {
  switch (error_type) {
    case NFCErrorType::SECURITY:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kSecurityError, "NFC operation not allowed.");
    case NFCErrorType::NOT_SUPPORTED:
    case NFCErrorType::DEVICE_DISABLED:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError, "NFC operation not supported.");
    case NFCErrorType::NOT_FOUND:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotFoundError,
          "Invalid NFC watch Id was provided.");
    case NFCErrorType::INVALID_MESSAGE:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kSyntaxError, "Invalid NFC message was provided.");
    case NFCErrorType::OPERATION_CANCELLED:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kAbortError, "The NFC operation was cancelled.");
    case NFCErrorType::TIMER_EXPIRED:
      return MakeGarbageCollected<DOMException>(DOMExceptionCode::kTimeoutError,
                                                "NFC operation has timed-out.");
    case NFCErrorType::CANNOT_CANCEL:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNoModificationAllowedError,
          "NFC operation cannot be canceled.");
    case NFCErrorType::IO_ERROR:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNetworkError,
          "NFC data transfer error has occurred.");
  }
  NOTREACHED();
  return MakeGarbageCollected<DOMException>(
      DOMExceptionCode::kUnknownError, "An unknown NFC error has occurred.");
}

}  // namespace blink
