// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_MISCELLANEOUS_OPERATIONS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_MISCELLANEOUS_OPERATIONS_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "v8/include/v8.h"

namespace blink {

// Operations from the "Miscellaneous Operations" section of the standard:
// https://streams.spec.whatwg.org/#misc-abstract-ops

class ExceptionState;
class ScriptState;
class StrategySizeAlgorithm;
class StreamAlgorithm;
class StreamStartAlgorithm;

// This is slightly different than the version in the standard
// https://streams.spec.whatwg.org/#create-algorithm-from-underlying-method as
// it doesn't require the number of expected arguments, algoArgCount, to be
// specified. Also, it only supports 0 or 1 extraArgs, instead of an arbitrary
// number. Use an empty value for |extra_arg| to mean 0 extraArgs.
// |method_name_for_error| supplies a more helpful name for the method to be
// used in exception messages. For example, if |method_name| is "write",
// |method_name_for_error| might be "underlyingSink.write".
CORE_EXPORT StreamAlgorithm* CreateAlgorithmFromUnderlyingMethod(
    ScriptState*,
    v8::Local<v8::Object> underlying_object,
    const char* method_name,
    const char* method_name_for_error,
    v8::MaybeLocal<v8::Value> extra_arg,
    ExceptionState&);

// Create a StreamStartAlgorithm from the "start" method on |underlying_object|.
// Unlike other algorithms, the lookup of the method on the object is done at
// execution time rather than algorithm creation time. |method_name_for_error|
// is used in exception messages. It is not copied so must remain valid until
// the algorithm is run.
CORE_EXPORT StreamStartAlgorithm* CreateStartAlgorithm(
    ScriptState*,
    v8::Local<v8::Object> underlying_object,
    const char* method_name_for_error,
    v8::Local<v8::Value> controller);

// Used in place of InvokeOrNoop in spec. Always takes 1 argument.
// https://streams.spec.whatwg.org/#invoke-or-noop
CORE_EXPORT v8::MaybeLocal<v8::Value> CallOrNoop1(ScriptState*,
                                                  v8::Local<v8::Object> object,
                                                  const char* method_name,
                                                  const char* name_for_error,
                                                  v8::Local<v8::Value> arg0,
                                                  ExceptionState&);

// https://streams.spec.whatwg.org/#promise-call
// "PromiseCall(F, V, args)"
// "F" is called |method| here
// "V" is called |recv| here
// "args" becomes |argc| and |argv| here.
CORE_EXPORT v8::Local<v8::Promise> PromiseCall(ScriptState*,
                                               v8::Local<v8::Function> method,
                                               v8::Local<v8::Object> recv,
                                               int argc,
                                               v8::Local<v8::Value> argv[]);

// Unlike in the standard, the caller needs to handle the conversion of the
// value to a Number.
// https://streams.spec.whatwg.org/#validate-and-normalize-high-water-mark
CORE_EXPORT double ValidateAndNormalizeHighWaterMark(double high_water_mark,
                                                     ExceptionState&);

// https://streams.spec.whatwg.org/#make-size-algorithm-from-size-function
CORE_EXPORT StrategySizeAlgorithm* MakeSizeAlgorithmFromSizeFunction(
    ScriptState*,
    v8::Local<v8::Value> size,
    ExceptionState&);

// Implements "a promise rejected with" from the INFRA standard.
// https://www.w3.org/2001/tag/doc/promises-guide/#a-promise-rejected-with
CORE_EXPORT v8::Local<v8::Promise> PromiseReject(ScriptState*,
                                                 v8::Local<v8::Value>);

// Implements "a promise resolved with" from the INFRA standard.
// https://www.w3.org/2001/tag/doc/promises-guide/#a-promise-resolved-with
CORE_EXPORT v8::Local<v8::Promise> PromiseResolve(ScriptState*,
                                                  v8::Local<v8::Value>);

// Implements "a promise resolved with *undefined*".
CORE_EXPORT v8::Local<v8::Promise> PromiseResolveWithUndefined(ScriptState*);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_MISCELLANEOUS_OPERATIONS_H_
