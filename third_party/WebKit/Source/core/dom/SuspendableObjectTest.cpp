/*
 * Copyright (c) 2014, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/dom/SuspendableObject.h"

#include "core/dom/Document.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>

namespace blink {

class MockSuspendableObject final
    : public GarbageCollectedFinalized<MockSuspendableObject>,
      public SuspendableObject {
  USING_GARBAGE_COLLECTED_MIXIN(MockSuspendableObject);

 public:
  explicit MockSuspendableObject(ExecutionContext* context)
      : SuspendableObject(context) {}

  virtual void Trace(blink::Visitor* visitor) {
    SuspendableObject::Trace(visitor);
  }

  MOCK_METHOD0(Suspend, void());
  MOCK_METHOD0(Resume, void());
  MOCK_METHOD1(ContextDestroyed, void(ExecutionContext*));
};

class SuspendableObjectTest : public ::testing::Test {
 protected:
  SuspendableObjectTest();

  Document& SrcDocument() const { return src_page_holder_->GetDocument(); }
  Document& DestDocument() const { return dest_page_holder_->GetDocument(); }
  MockSuspendableObject& SuspendableObject() { return *suspendable_object_; }

 private:
  std::unique_ptr<DummyPageHolder> src_page_holder_;
  std::unique_ptr<DummyPageHolder> dest_page_holder_;
  Persistent<MockSuspendableObject> suspendable_object_;
};

SuspendableObjectTest::SuspendableObjectTest()
    : src_page_holder_(DummyPageHolder::Create(IntSize(800, 600))),
      dest_page_holder_(DummyPageHolder::Create(IntSize(800, 600))),
      suspendable_object_(
          new MockSuspendableObject(&src_page_holder_->GetDocument())) {
  suspendable_object_->SuspendIfNeeded();
}

TEST_F(SuspendableObjectTest, NewContextObserved) {
  unsigned initial_src_count = SrcDocument().SuspendableObjectCount();
  unsigned initial_dest_count = DestDocument().SuspendableObjectCount();

  EXPECT_CALL(SuspendableObject(), Resume());
  SuspendableObject().DidMoveToNewExecutionContext(&DestDocument());

  EXPECT_EQ(initial_src_count - 1, SrcDocument().SuspendableObjectCount());
  EXPECT_EQ(initial_dest_count + 1, DestDocument().SuspendableObjectCount());
}

TEST_F(SuspendableObjectTest, MoveToActiveDocument) {
  EXPECT_CALL(SuspendableObject(), Resume());
  SuspendableObject().DidMoveToNewExecutionContext(&DestDocument());
}

TEST_F(SuspendableObjectTest, MoveToSuspendedDocument) {
  DestDocument().SuspendScheduledTasks();

  EXPECT_CALL(SuspendableObject(), Suspend());
  SuspendableObject().DidMoveToNewExecutionContext(&DestDocument());
}

TEST_F(SuspendableObjectTest, MoveToStoppedDocument) {
  DestDocument().Shutdown();

  EXPECT_CALL(SuspendableObject(), ContextDestroyed(&DestDocument()));
  SuspendableObject().DidMoveToNewExecutionContext(&DestDocument());
}

}  // namespace blink
