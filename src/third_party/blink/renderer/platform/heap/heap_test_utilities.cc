/*
 * Copyright 2021 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "third_party/blink/renderer/platform/heap/heap_test_utilities.h"
#include <memory>

#include "third_party/blink/renderer/platform/heap/heap.h"
#include "v8/include/cppgc/platform.h"

namespace blink {

namespace {

bool IsGCInProgress() {
  return cppgc::subtle::HeapState::IsMarking(
             ThreadState::Current()->heap_handle()) ||
         cppgc::subtle::HeapState::IsSweeping(
             ThreadState::Current()->heap_handle());
}

cppgc::EmbedderStackState ConvertStackState(BlinkGC::StackState stack_state) {
  switch (stack_state) {
    case BlinkGC::StackState::kNoHeapPointersOnStack:
      return cppgc::EmbedderStackState::kNoHeapPointers;
    case BlinkGC::StackState::kHeapPointersOnStack:
      return cppgc::EmbedderStackState::kMayContainHeapPointers;
  }
}

}  // namespace

TestSupportingGC::~TestSupportingGC() {
  PreciselyCollectGarbage();
}

// static
void TestSupportingGC::PreciselyCollectGarbage() {
  ThreadState::Current()->CollectAllGarbageForTesting(
      BlinkGC::kNoHeapPointersOnStack);
}

// static
void TestSupportingGC::ConservativelyCollectGarbage() {
  ThreadState::Current()->CollectAllGarbageForTesting(
      BlinkGC::kHeapPointersOnStack);
}

// static
void TestSupportingGC::ClearOutOldGarbage() {
  PreciselyCollectGarbage();
  auto& cpp_heap = ThreadState::Current()->cpp_heap();
  size_t old_used = cpp_heap.CollectStatistics(cppgc::HeapStatistics::kDetailed)
                        .used_size_bytes;
  while (true) {
    PreciselyCollectGarbage();
    size_t used = cpp_heap.CollectStatistics(cppgc::HeapStatistics::kDetailed)
                      .used_size_bytes;
    if (used >= old_used)
      break;
    old_used = used;
  }
}

CompactionTestDriver::CompactionTestDriver(ThreadState* thread_state)
    : heap_(thread_state->heap_handle()) {}

void CompactionTestDriver::ForceCompactionForNextGC() {
  heap_.ForceCompactionForNextGarbageCollection();
}

IncrementalMarkingTestDriver::IncrementalMarkingTestDriver(
    ThreadState* thread_state)
    : heap_(thread_state->heap_handle()) {}

IncrementalMarkingTestDriver::~IncrementalMarkingTestDriver() {
  if (IsGCInProgress())
    heap_.FinalizeGarbageCollection(cppgc::EmbedderStackState::kNoHeapPointers);
}

void IncrementalMarkingTestDriver::StartGC() {
  heap_.StartGarbageCollection();
}

void IncrementalMarkingTestDriver::TriggerMarkingSteps(
    BlinkGC::StackState stack_state) {
  CHECK(ThreadState::Current()->IsIncrementalMarking());
  while (!heap_.PerformMarkingStep(ConvertStackState(stack_state))) {
  }
}

void IncrementalMarkingTestDriver::FinishGC() {
  CHECK(ThreadState::Current()->IsIncrementalMarking());
  heap_.FinalizeGarbageCollection(cppgc::EmbedderStackState::kNoHeapPointers);
  CHECK(!ThreadState::Current()->IsIncrementalMarking());
}

ConcurrentMarkingTestDriver::ConcurrentMarkingTestDriver(
    ThreadState* thread_state)
    : IncrementalMarkingTestDriver(thread_state) {}

void ConcurrentMarkingTestDriver::StartGC() {
  IncrementalMarkingTestDriver::StartGC();
  heap_.ToggleMainThreadMarking(false);
}

void ConcurrentMarkingTestDriver::TriggerMarkingSteps(
    BlinkGC::StackState stack_state) {
  CHECK(ThreadState::Current()->IsIncrementalMarking());
  heap_.PerformMarkingStep(ConvertStackState(stack_state));
}

void ConcurrentMarkingTestDriver::FinishGC() {
  heap_.ToggleMainThreadMarking(true);
  IncrementalMarkingTestDriver::FinishGC();
}

}  // namespace blink
