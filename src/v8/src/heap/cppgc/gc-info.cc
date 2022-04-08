// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/cppgc/internal/gc-info.h"

#include "include/cppgc/internal/name-trait.h"
#include "include/v8config.h"
#include "src/heap/cppgc/gc-info-table.h"

namespace cppgc {
namespace internal {

namespace {

HeapObjectName GetHiddenName(const void*) {
  return {NameProvider::kHiddenName, true};
}

}  // namespace

// static
GCInfoIndex EnsureGCInfoIndexTrait::EnsureGCInfoIndexPolymorphic(
    AtomicGCInfoIndex& registered_index, TraceCallback trace_callback,
    FinalizationCallback finalization_callback, NameCallback name_callback) {
  return GlobalGCInfoTable::GetMutable().RegisterNewGCInfo(
      registered_index,
      {finalization_callback, trace_callback, name_callback, true});
}

// static
GCInfoIndex EnsureGCInfoIndexTrait::EnsureGCInfoIndexPolymorphic(
    AtomicGCInfoIndex& registered_index, TraceCallback trace_callback,
    FinalizationCallback finalization_callback) {
  return GlobalGCInfoTable::GetMutable().RegisterNewGCInfo(
      registered_index,
      {finalization_callback, trace_callback, GetHiddenName, true});
}

// static
GCInfoIndex EnsureGCInfoIndexTrait::EnsureGCInfoIndexPolymorphic(
    AtomicGCInfoIndex& registered_index, TraceCallback trace_callback,
    NameCallback name_callback) {
  return GlobalGCInfoTable::GetMutable().RegisterNewGCInfo(
      registered_index, {nullptr, trace_callback, name_callback, true});
}

// static
GCInfoIndex EnsureGCInfoIndexTrait::EnsureGCInfoIndexPolymorphic(
    AtomicGCInfoIndex& registered_index, TraceCallback trace_callback) {
  return GlobalGCInfoTable::GetMutable().RegisterNewGCInfo(
      registered_index, {nullptr, trace_callback, GetHiddenName, true});
}

// static
GCInfoIndex EnsureGCInfoIndexTrait::EnsureGCInfoIndexNonPolymorphic(
    AtomicGCInfoIndex& registered_index, TraceCallback trace_callback,
    FinalizationCallback finalization_callback, NameCallback name_callback) {
  return GlobalGCInfoTable::GetMutable().RegisterNewGCInfo(
      registered_index,
      {finalization_callback, trace_callback, name_callback, false});
}

// static
GCInfoIndex EnsureGCInfoIndexTrait::EnsureGCInfoIndexNonPolymorphic(
    AtomicGCInfoIndex& registered_index, TraceCallback trace_callback,
    FinalizationCallback finalization_callback) {
  return GlobalGCInfoTable::GetMutable().RegisterNewGCInfo(
      registered_index,
      {finalization_callback, trace_callback, GetHiddenName, false});
}

// static
GCInfoIndex EnsureGCInfoIndexTrait::EnsureGCInfoIndexNonPolymorphic(
    AtomicGCInfoIndex& registered_index, TraceCallback trace_callback,
    NameCallback name_callback) {
  return GlobalGCInfoTable::GetMutable().RegisterNewGCInfo(
      registered_index, {nullptr, trace_callback, name_callback, false});
}

// static
GCInfoIndex EnsureGCInfoIndexTrait::EnsureGCInfoIndexNonPolymorphic(
    AtomicGCInfoIndex& registered_index, TraceCallback trace_callback) {
  return GlobalGCInfoTable::GetMutable().RegisterNewGCInfo(
      registered_index, {nullptr, trace_callback, GetHiddenName, false});
}

AtomicGCInfoIndex::AtomicGCInfoIndex() {
}

GCInfoIndex
AtomicGCInfoIndex::load_acquire() const noexcept {
  return detail_.load(std::memory_order_acquire);
}

GCInfoIndex
AtomicGCInfoIndex::load_acquire() const volatile noexcept {
  return detail_.load(std::memory_order_acquire);
}

GCInfoIndex
AtomicGCInfoIndex::load_relaxed() const noexcept {
  return detail_.load(std::memory_order_relaxed);
}

GCInfoIndex
AtomicGCInfoIndex::load_relaxed() const volatile noexcept {
  return detail_.load(std::memory_order_relaxed);
}

void
AtomicGCInfoIndex::store_release(GCInfoIndex desired) noexcept {
  detail_.store(desired, std::memory_order_release);
}

void
AtomicGCInfoIndex::store_release(GCInfoIndex desired) volatile noexcept {
  detail_.store(desired, std::memory_order_release);
}

}  // namespace internal
}  // namespace cppgc
