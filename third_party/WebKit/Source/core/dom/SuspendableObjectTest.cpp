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

  DEFINE_INLINE_VIRTUAL_TRACE() { SuspendableObject::trace(visitor); }

  MOCK_METHOD0(suspend, void());
  MOCK_METHOD0(resume, void());
  MOCK_METHOD1(contextDestroyed, void(ExecutionContext*));
};

class SuspendableObjectTest : public ::testing::Test {
 protected:
  SuspendableObjectTest();

  Document& srcDocument() const { return m_srcPageHolder->document(); }
  Document& destDocument() const { return m_destPageHolder->document(); }
  MockSuspendableObject& suspendableObject() { return *m_suspendableObject; }

 private:
  std::unique_ptr<DummyPageHolder> m_srcPageHolder;
  std::unique_ptr<DummyPageHolder> m_destPageHolder;
  Persistent<MockSuspendableObject> m_suspendableObject;
};

SuspendableObjectTest::SuspendableObjectTest()
    : m_srcPageHolder(DummyPageHolder::create(IntSize(800, 600))),
      m_destPageHolder(DummyPageHolder::create(IntSize(800, 600))),
      m_suspendableObject(
          new MockSuspendableObject(&m_srcPageHolder->document())) {
  m_suspendableObject->suspendIfNeeded();
}

TEST_F(SuspendableObjectTest, NewContextObserved) {
  unsigned initialSrcCount = srcDocument().suspendableObjectCount();
  unsigned initialDestCount = destDocument().suspendableObjectCount();

  EXPECT_CALL(suspendableObject(), resume());
  suspendableObject().didMoveToNewExecutionContext(&destDocument());

  EXPECT_EQ(initialSrcCount - 1, srcDocument().suspendableObjectCount());
  EXPECT_EQ(initialDestCount + 1, destDocument().suspendableObjectCount());
}

TEST_F(SuspendableObjectTest, MoveToActiveDocument) {
  EXPECT_CALL(suspendableObject(), resume());
  suspendableObject().didMoveToNewExecutionContext(&destDocument());
}

TEST_F(SuspendableObjectTest, MoveToSuspendedDocument) {
  destDocument().suspendScheduledTasks();

  EXPECT_CALL(suspendableObject(), suspend());
  suspendableObject().didMoveToNewExecutionContext(&destDocument());
}

TEST_F(SuspendableObjectTest, MoveToStoppedDocument) {
  destDocument().shutdown();

  EXPECT_CALL(suspendableObject(), contextDestroyed(&destDocument()));
  suspendableObject().didMoveToNewExecutionContext(&destDocument());
}

}  // namespace blink
