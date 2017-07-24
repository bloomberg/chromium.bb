// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaintWorkletPendingGeneratorRegistry_h
#define PaintWorkletPendingGeneratorRegistry_h

#include "modules/csspaint/CSSPaintImageGeneratorImpl.h"
#include "platform/heap/Heap.h"
#include "platform/heap/HeapAllocator.h"

namespace blink {

class CSSPaintDefinition;

// Keeps pending CSSPaintImageGeneratorImpls until corresponding
// CSSPaintDefinitions are registered. This is primarily owned by the
// PaintWorklet instance.
class PaintWorkletPendingGeneratorRegistry
    : public GarbageCollected<PaintWorkletPendingGeneratorRegistry> {
  WTF_MAKE_NONCOPYABLE(PaintWorkletPendingGeneratorRegistry);

 public:
  PaintWorkletPendingGeneratorRegistry() = default;

  void NotifyGeneratorReady(const String& name);
  void AddPendingGenerator(const String& name, CSSPaintImageGeneratorImpl*);

  DECLARE_TRACE();

 private:
  // The map of CSSPaintImageGeneratorImpl which are waiting for a
  // CSSPaintDefinition to be registered. Owners of this registry is expected to
  // outlive the generators hence are held onto with a WeakMember.
  using GeneratorHashSet = HeapHashSet<WeakMember<CSSPaintImageGeneratorImpl>>;
  using PendingGeneratorMap = HeapHashMap<String, Member<GeneratorHashSet>>;
  PendingGeneratorMap pending_generators_;
};

}  // namespace blink

#endif  // PaintWorkletPendingGeneratorRegistry_h
