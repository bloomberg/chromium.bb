// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_EXCEPTION_CODE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_EXCEPTION_CODE_H_

namespace blink {

// DOMException uses |unsigned short| for exception codes.
// https://heycam.github.io/webidl/#idl-DOMException
// In our DOM implementation we use |int| instead, and use different numerical
// ranges for different types of exceptions (not limited to DOMException), so
// that an exception of any type can be expressed with a single integer.
//
// Zero value in ExceptionCode means no exception being thrown.
using ExceptionCode = int;

// Exception codes that correspond to ECMAScript Error objects.
// https://tc39.github.io/ecma262/#sec-error-objects
//
// Since each enum value needs to be convertible to ExceptionCode, scoped
// enumeration is not applicable.  Uses a class as a scope instead.
class ESErrorType final {
 public:
  enum : ExceptionCode {
    kError = 1000,    // v8::Exception::Error
    kRangeError,      // v8::Exception::RangeError
    kReferenceError,  // v8::Exception::ReferenceError
    kSyntaxError,     // v8::Exception::SyntaxError
    kTypeError,       // v8::Exception::TypeError
  };
};

// Exception codes used only inside ExceptionState implementation.
//
// Since each enum value needs to be convertible to ExceptionCode, scoped
// enumeration is not applicable.  Uses a class as a scope instead.
class InternalExceptionType final {
 public:
  enum : ExceptionCode {
    // An exception code that is used when rethrowing a v8::Value as an
    // exception where there is no way to determine an exception code.
    kRethrownException = 2000,
  };
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_EXCEPTION_CODE_H_
