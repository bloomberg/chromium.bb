// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/api-arguments.h"

#include "src/api-arguments-inl.h"

namespace v8 {
namespace internal {

PropertyCallbackArguments::PropertyCallbackArguments(Isolate* isolate,
                                                     Object* data, Object* self,
                                                     JSObject* holder,
                                                     ShouldThrow should_throw)
    : Super(isolate) {
  slot_at(T::kThisIndex).store(self);
  slot_at(T::kHolderIndex).store(holder);
  slot_at(T::kDataIndex).store(data);
  slot_at(T::kIsolateIndex).store(reinterpret_cast<Object*>(isolate));
  slot_at(T::kShouldThrowOnErrorIndex)
      .store(Smi::FromInt(should_throw == kThrowOnError ? 1 : 0));

  // Here the hole is set as default value.
  // It cannot escape into js as it's removed in Call below.
  HeapObject* the_hole = ReadOnlyRoots(isolate).the_hole_value();
  slot_at(T::kReturnValueDefaultValueIndex).store(the_hole);
  slot_at(T::kReturnValueIndex).store(the_hole);
  DCHECK((*slot_at(T::kHolderIndex))->IsHeapObject());
  DCHECK((*slot_at(T::kIsolateIndex))->IsSmi());
}

FunctionCallbackArguments::FunctionCallbackArguments(
    internal::Isolate* isolate, internal::Object* data,
    internal::HeapObject* callee, internal::Object* holder,
    internal::HeapObject* new_target, internal::Address* argv, int argc)
    : Super(isolate), argv_(argv), argc_(argc) {
  slot_at(T::kDataIndex).store(data);
  slot_at(T::kHolderIndex).store(holder);
  slot_at(T::kNewTargetIndex).store(new_target);
  slot_at(T::kIsolateIndex).store(reinterpret_cast<internal::Object*>(isolate));
  // Here the hole is set as default value.
  // It cannot escape into js as it's remove in Call below.
  HeapObject* the_hole = ReadOnlyRoots(isolate).the_hole_value();
  slot_at(T::kReturnValueDefaultValueIndex).store(the_hole);
  slot_at(T::kReturnValueIndex).store(the_hole);
  DCHECK((*slot_at(T::kHolderIndex))->IsHeapObject());
  DCHECK((*slot_at(T::kIsolateIndex))->IsSmi());
}

}  // namespace internal
}  // namespace v8
