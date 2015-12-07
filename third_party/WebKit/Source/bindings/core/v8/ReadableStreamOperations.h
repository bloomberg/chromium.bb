// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ReadableStreamOperations_h
#define ReadableStreamOperations_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptValue.h"
#include <v8.h>

namespace blink {

class ExceptionState;
class ScriptState;

// This class has various methods for ReadableStream[Reader] implemented with
// V8 Extras.
// All methods should be called in an appropriate V8 context. All v8 handle
// arguments must not be empty.
class CORE_EXPORT ReadableStreamOperations {
    STATIC_ONLY(ReadableStreamOperations);
public:
    // AcquireReadableStreamReader
    // This function assumes |isReadableStream(stream)|.
    // Returns an empty value and throws an error via the ExceptionState when
    // errored.
    static ScriptValue getReader(ScriptState*, v8::Local<v8::Value> stream, ExceptionState&);

    // IsReadableStream
    static bool isReadableStream(ScriptState*, v8::Local<v8::Value>);

    // IsReadableStreamDisturbed
    // This function assumes |isReadableStream(stream)|.
    static bool isDisturbed(ScriptState*, v8::Local<v8::Value> stream);

    // IsReadableStreamLocked
    // This function assumes |isReadableStream(stream)|.
    static bool isLocked(ScriptState*, v8::Local<v8::Value> stream);

    // IsReadableStreamReader
    static bool isReadableStreamReader(ScriptState*, v8::Local<v8::Value>);

    // ReadFromReadableStreamReader
    // This function assumes |isReadableStreamReader(reader)|.
    static ScriptPromise read(ScriptState*, v8::Local<v8::Value> reader);
};

} // namespace blink

#endif // ReadableStreamOperations_h
