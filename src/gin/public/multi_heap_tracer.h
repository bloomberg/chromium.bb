// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_PUBLIC_MULTI_HEAP_TRACER_H_
#define GIN_PUBLIC_MULTI_HEAP_TRACER_H_

#include <unordered_map>
#include <utility>

#include "base/macros.h"
#include "gin/public/gin_embedders.h"
#include "gin/gin_export.h"
#include "v8/include/v8.h"

namespace gin {

class GIN_EXPORT MultiHeapTracer : public v8::EmbedderHeapTracer {
  // 'MultiHeapTracer' provides a concrete implementation of the
  // 'v8::EmbedderHeapTracer' protocol, designed to support the tracing of
  // multiple embedder heaps.
  //
  // When V8 informs tracers of wrappers that need to be traced, it gives the
  // tracer the first two embedder fields for each wrapper.  To integrate with
  // this MultiHeapTracer, embedders must set the first embedder field to an
  // object which is layout-compatible with 'gin::WrapperInfo'.  The 'embedder'
  // field for wrappers from a given embedder heap must be set to the id
  // returned from the 'AddHeapTracer' call to add the tracer for that embedder
  // heap.
  //
  // For known embedders, 'AddHeapTracer' allows the embedder to be specified,
  // in which case the returned value will match the value passed in.  If the
  // embedder is not specified (kEmbedderUnknown), this heap tracer will
  // generate a unique id.
  //
  // For 'RegisterV8References', each registered heap tracer will be given all
  // wrappers, but is expected to ignore wrappers with an embedder field which
  // it does not recognize.
  //
  // For 'AdvanceTracing', each registered heap tracer will be asked to advance
  // tracing.  Tracers are expected to ensure that they do nothing if the
  // deadline has already passed.

 public:
  // TYPES
  using WrapperFieldPair = std::pair<void *, void *>;
    // Alias for the first and second embedder field values of a wrapper.

  using WrapperFieldPairs = std::vector<WrapperFieldPair>;
    // Alias for a collection of WrapperFieldPair objects.

  // CLASS METHODS
  static MultiHeapTracer* From(v8::Isolate *isolate);
    // Return the MultiHeapTracer object associated with the specified
    // 'isolate', or 'nullptr' if there is no associated object.

  // CREATORS
  MultiHeapTracer();
    // Create a new MultiHeapTracer.

  ~MultiHeapTracer() final;
    // Destroy this object.

  // MANIPULATORS
  int AddHeapTracer(v8::EmbedderHeapTracer *tracer,
                    GinEmbedder             embedder = kEmbedderUnknown);
    // Register the specified 'tracer' and return an id which must be set as
    // the 'embedder' field of the 'WrapperInfo'-compatible struct.  Optionally
    // specify the 'embedder' id to be returned.  If 'embedder' is
    // 'kEmbedderUnknown', a new id will be generated.  The behavior is
    // undefined unless heap tracing is not currently in progress.

  void RemoveHeapTracer(int embedder_id);
    // Unregister the tracer for the specified 'embedder_id'.  The behavior is
    // undefined unless 'embedder_id' was obtained by a call to
    // 'AddHeapTracer'.  The behavior is undefined unless heap tracing is not
    // currently in progress.

  void RegisterV8References(
                        const WrapperFieldPairs& wrapper_field_pairs) override;
    // Notify all registered tracers of the wrapper field pairs that may need
    // tracing.  Each tracer is expected to inspect the 'embedder' field of the
    // 'WrapperInfo'-compatible struct in the first field of each pair, and
    // ignore wrapper field pairs which don't match.  Tracers are expected to
    // store the field pairs they care about for later tracing when
    // 'AdvanceTracing' is called.

  void TracePrologue() override;
    // Notify all registered tracers that tracing will begin.

  bool AdvanceTracing(double deadline_in_ms) override;
    // Notify all registered tracers that they should trace the wrapper field
    // pairs they previously stored from 'RegisterV8References'.  The
    // registered tracers will be called in a different random order each time.
    // Each tracer should ensure that once the specified 'deadline_in_ms' is
    // past, it does no work.  If the 'deadline_in_ms' is 'Infinity', the
    // tracers should complete tracing.

  void TraceEpilogue() override;
    // Notify all registered tracers that tracing has completed.

  void EnterFinalPause(EmbedderStackState stack_state) override;
    // Notify all registered tracers that we're entering the final pause.

  bool IsTracingDone() override;
    // Return 'true' if all the registered tracers have done tracing, and
    // 'false' otherwise.

 private:
  using Tracers = std::unordered_map<int, v8::EmbedderHeapTracer *>;

  bool    is_tracing_;  // Whether we're currently tracing
  int     next_id_;     // The next id for unknown embedders
  Tracers tracers_;     // The collection of registered tracers

  DISALLOW_COPY_AND_ASSIGN(MultiHeapTracer);
};

}  // namespace gin

#endif  // GIN_PUBLIC_MULTI_HEAP_TRACER_H_
