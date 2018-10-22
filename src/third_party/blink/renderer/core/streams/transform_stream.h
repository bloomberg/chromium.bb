// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_TRANSFORM_STREAM_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_TRANSFORM_STREAM_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_v8_reference.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "v8/include/v8.h"

namespace blink {

class ExceptionState;
class ScriptState;
class ScriptValue;
class TransformStreamTransformer;
class Visitor;

// Creates and wraps a JavaScript TransformStream object with a transformation
// defined in C++. Provides access to the readable and writable streams.
//
// On-heap references to this class must always be via a TraceWrapperMember, and
// must always have an ancestor in the V8 heap, or |stream_| will be lost.
//
// To ensure that the JS TransformStream is always referenced, this class uses
// two-stage construction. After calling the constructor, store the reference
// in a TraceWrapperMember before calling Init(). Init() must always be called
// before using the instance.
class CORE_EXPORT TransformStream final
    : public GarbageCollectedFinalized<TransformStream> {
 public:
  TransformStream();
  ~TransformStream();

  // If HadException is true on return, the object is invalid and should be
  // destroyed.
  void Init(TransformStreamTransformer*, ScriptState*, ExceptionState&);

  ScriptValue Readable(ScriptState*, ExceptionState&) const;
  ScriptValue Writable(ScriptState*, ExceptionState&) const;

  void Trace(Visitor*);

 private:
  // These are class-scoped to avoid name clashes in jumbo builds.
  class Algorithm;
  class FlushAlgorithm;
  class TransformAlgorithm;

  // Common implementation for Readable() and Writable() accessors.
  ScriptValue Accessor(const char* accessor_function_name,
                       ScriptState*,
                       ExceptionState&) const;

  TraceWrapperV8Reference<v8::Value> stream_;

  DISALLOW_COPY_AND_ASSIGN(TransformStream);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_TRANSFORM_STREAM_H_
