// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_error_util.h"

#include "third_party/blink/renderer/core/dom/dom_exception.h"

namespace blink {

DOMException* CreateDOMExceptionFromWebRTCError(const WebRTCError& error) {
  switch (error.GetType()) {
    case WebRTCErrorType::kNone:
      // This should never happen.
      NOTREACHED();
      break;
    case WebRTCErrorType::kSyntaxError:
      return DOMException::Create(kSyntaxError, error.message());
      break;
    case WebRTCErrorType::kInvalidModification:
      return DOMException::Create(kInvalidModificationError, error.message());
      break;
    case WebRTCErrorType::kNetworkError:
      return DOMException::Create(kNetworkError, error.message());
      break;
    case WebRTCErrorType::kOperationError:
      return DOMException::Create(kOperationError, error.message());
      break;
    case WebRTCErrorType::kInvalidState:
      return DOMException::Create(kInvalidStateError, error.message());
    case WebRTCErrorType::kInvalidParameter:
      // One use of this value is to signal invalid SDP syntax.
      // According to spec, this should return an RTCError with name
      // "RTCError" and detail "sdp-syntax-error", with
      // "sdpLineNumber" set to indicate the line where the error
      // occured.
      // TODO(https://crbug.com/821806): Implement the RTCError object.
      return DOMException::Create(kInvalidAccessError, error.message());
    case WebRTCErrorType::kInternalError:
      // Not a straightforward mapping, but used as a fallback at lower layers.
      return DOMException::Create(kOperationError, error.message());
      break;
    case WebRTCErrorType::kUnsupportedParameter:
    case WebRTCErrorType::kInvalidRange:
      LOG(ERROR) << "Got unhandled WebRTC error "
                 << static_cast<int>(error.GetType());
      // No DOM equivalent. Needs per-error evaluation.
      NOTREACHED();
      break;
  }
  NOTREACHED();
  return nullptr;
}
}  // namespace blink
