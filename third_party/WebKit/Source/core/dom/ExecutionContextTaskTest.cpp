// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/ExecutionContextTask.h"

#include "platform/heap/Handle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class GCObject : public GarbageCollectedFinalized<GCObject> {
 public:
  static int counter_;
  GCObject() { ++counter_; }
  ~GCObject() { --counter_; }
  DEFINE_INLINE_TRACE() {}
  void Run(GCObject*) {}
};

int GCObject::counter_ = 0;

static void FunctionWithGarbageCollected(GCObject*) {}

static void FunctionWithExecutionContext(GCObject*, ExecutionContext*) {}

class CrossThreadTaskTest : public testing::Test {
 protected:
  void SetUp() override { GCObject::counter_ = 0; }
  void TearDown() override {
    ThreadState::Current()->CollectGarbage(BlinkGC::kNoHeapPointersOnStack,
                                           BlinkGC::kGCWithSweep,
                                           BlinkGC::kForcedGC);
    ASSERT_EQ(0, GCObject::counter_);
  }
};

TEST_F(CrossThreadTaskTest, CreateForGarbageCollectedMethod) {
  std::unique_ptr<ExecutionContextTask> task1 = CreateCrossThreadTask(
      &GCObject::Run, WrapCrossThreadPersistent(new GCObject),
      WrapCrossThreadPersistent(new GCObject));
  std::unique_ptr<ExecutionContextTask> task2 = CreateCrossThreadTask(
      &GCObject::Run, WrapCrossThreadPersistent(new GCObject),
      WrapCrossThreadPersistent(new GCObject));
  ThreadState::Current()->CollectGarbage(BlinkGC::kNoHeapPointersOnStack,
                                         BlinkGC::kGCWithSweep,
                                         BlinkGC::kForcedGC);
  EXPECT_EQ(4, GCObject::counter_);
}

TEST_F(CrossThreadTaskTest, CreateForFunctionWithGarbageCollected) {
  std::unique_ptr<ExecutionContextTask> task1 = CreateCrossThreadTask(
      &FunctionWithGarbageCollected, WrapCrossThreadPersistent(new GCObject));
  std::unique_ptr<ExecutionContextTask> task2 = CreateCrossThreadTask(
      &FunctionWithGarbageCollected, WrapCrossThreadPersistent(new GCObject));
  ThreadState::Current()->CollectGarbage(BlinkGC::kNoHeapPointersOnStack,
                                         BlinkGC::kGCWithSweep,
                                         BlinkGC::kForcedGC);
  EXPECT_EQ(2, GCObject::counter_);
}

TEST_F(CrossThreadTaskTest, CreateForFunctionWithExecutionContext) {
  std::unique_ptr<ExecutionContextTask> task1 = CreateCrossThreadTask(
      &FunctionWithExecutionContext, WrapCrossThreadPersistent(new GCObject));
  std::unique_ptr<ExecutionContextTask> task2 = CreateCrossThreadTask(
      &FunctionWithExecutionContext, WrapCrossThreadPersistent(new GCObject));
  ThreadState::Current()->CollectGarbage(BlinkGC::kNoHeapPointersOnStack,
                                         BlinkGC::kGCWithSweep,
                                         BlinkGC::kForcedGC);
  EXPECT_EQ(2, GCObject::counter_);
}

}  // namespace blink
