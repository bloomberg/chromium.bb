/*
 * Copyright 2017 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_HEAP_TEST_UTILITIES_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_HEAP_TEST_UTILITIES_H_

#include "base/callback.h"
#include "third_party/blink/renderer/platform/heap/blink_gc.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/trace_traits.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink {

void PreciselyCollectGarbage();
void ConservativelyCollectGarbage(
    BlinkGC::SweepingType sweeping_type = BlinkGC::kEagerSweeping);
void ClearOutOldGarbage();
void CompleteSweepingIfNeeded();

template <typename T>
class ObjectWithCallbackBeforeInitializer
    : public GarbageCollected<ObjectWithCallbackBeforeInitializer<T>> {
 public:
  ObjectWithCallbackBeforeInitializer(
      base::OnceCallback<void(ObjectWithCallbackBeforeInitializer<T>*)>&& cb,
      T* value)
      : bool_(ExecuteCallbackReturnTrue(this, std::move(cb))), value_(value) {}

  ObjectWithCallbackBeforeInitializer(
      base::OnceCallback<void(ObjectWithCallbackBeforeInitializer<T>*)>&& cb)
      : bool_(ExecuteCallbackReturnTrue(this, std::move(cb))) {}

  virtual void Trace(Visitor* visitor) { visitor->Trace(value_); }

  T* value() const { return value_.Get(); }

 private:
  static bool ExecuteCallbackReturnTrue(
      ObjectWithCallbackBeforeInitializer* thiz,
      base::OnceCallback<void(ObjectWithCallbackBeforeInitializer<T>*)>&& cb) {
    std::move(cb).Run(thiz);
    return true;
  }

  bool bool_;
  Member<T> value_;
};

template <typename T>
class MixinWithCallbackBeforeInitializer : public GarbageCollectedMixin {
 public:
  MixinWithCallbackBeforeInitializer(
      base::OnceCallback<void(MixinWithCallbackBeforeInitializer<T>*)>&& cb,
      T* value)
      : bool_(ExecuteCallbackReturnTrue(this, std::move(cb))), value_(value) {}

  MixinWithCallbackBeforeInitializer(
      base::OnceCallback<void(MixinWithCallbackBeforeInitializer<T>*)>&& cb)
      : bool_(ExecuteCallbackReturnTrue(this, std::move(cb))) {}

  void Trace(Visitor* visitor) override { visitor->Trace(value_); }

  T* value() const { return value_.Get(); }

 private:
  static bool ExecuteCallbackReturnTrue(
      MixinWithCallbackBeforeInitializer* thiz,
      base::OnceCallback<void(MixinWithCallbackBeforeInitializer<T>*)>&& cb) {
    std::move(cb).Run(thiz);
    return true;
  }

  bool bool_;
  Member<T> value_;
};

class BoolMixin {
 protected:
  bool bool_ = false;
};

template <typename T>
class ObjectWithMixinWithCallbackBeforeInitializer
    : public GarbageCollected<ObjectWithMixinWithCallbackBeforeInitializer<T>>,
      public BoolMixin,
      public MixinWithCallbackBeforeInitializer<T> {
  USING_GARBAGE_COLLECTED_MIXIN(ObjectWithMixinWithCallbackBeforeInitializer);

 public:
  using Mixin = MixinWithCallbackBeforeInitializer<T>;

  ObjectWithMixinWithCallbackBeforeInitializer(
      base::OnceCallback<void(Mixin*)>&& cb,
      T* value)
      : Mixin(std::move(cb), value) {}

  ObjectWithMixinWithCallbackBeforeInitializer(
      base::OnceCallback<void(Mixin*)>&& cb)
      : Mixin(std::move(cb)) {}

  void Trace(Visitor* visitor) override { Mixin::Trace(visitor); }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_HEAP_TEST_UTILITIES_H_
