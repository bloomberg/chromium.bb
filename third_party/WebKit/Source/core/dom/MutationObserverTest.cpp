// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/MutationObserver.h"

#include "core/dom/MutationCallback.h"
#include "core/dom/MutationObserverInit.h"
#include "core/dom/MutationObserverRegistration.h"
#include "core/html/HTMLDocument.h"
#include "core/html/HTMLElement.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

class EmptyMutationCallback : public MutationCallback {
 public:
  explicit EmptyMutationCallback(Document& document) : document_(document) {}
  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->Trace(document_);
    MutationCallback::Trace(visitor);
  }

 private:
  void Call(const HeapVector<Member<MutationRecord>>&,
            MutationObserver*) override {}
  ExecutionContext* GetExecutionContext() const override { return document_; }

  Member<Document> document_;
};
}

TEST(MutationObserverTest, DisconnectCrash) {
  Persistent<Document> document = HTMLDocument::CreateForTest();
  HTMLElement* root = ToHTMLElement(document->createElement("html"));
  document->AppendChild(root);
  root->setInnerHTML("<head><title>\n</title></head><body></body>");
  Node* head = root->firstChild()->firstChild();
  DCHECK(head);
  Persistent<MutationObserver> observer =
      MutationObserver::Create(new EmptyMutationCallback(*document));
  MutationObserverInit init;
  init.setCharacterDataOldValue(false);
  observer->observe(head, init, ASSERT_NO_EXCEPTION);

  head->remove();
  Persistent<MutationObserverRegistration> registration =
      observer->registrations_.begin()->Get();
  // The following GC will collect |head|, but won't collect a
  // MutationObserverRegistration for |head|.
  ThreadState::Current()->CollectGarbage(BlinkGC::kNoHeapPointersOnStack,
                                         BlinkGC::kGCWithoutSweep,
                                         BlinkGC::kForcedGC);
  observer->disconnect();
  // The test passes if disconnect() didn't crash.  crbug.com/657613.
}

}  // namespace blink
