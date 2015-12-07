// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebNFCError_h
#define WebNFCError_h

namespace blink {

// Errors that may occur during WebNFC execution.
enum class WebNFCError {
    // SecurityError
    Security,
    // NotSupportedError
    NotSupported,
    DeviceDisabled,
    // NotFoundError
    NotFound,
    // SyntaxError
    InvalidMessage,
    // AbortError
    OperationCancelled,
    // TimeoutError
    TimerExpired,
    // NoModificationAllowedError
    CannotCancel,
    // NetworkError
    IOError,

    ENUM_MAX_VALUE = IOError
};

} // namespace blink

#endif // WebNFCError_h
