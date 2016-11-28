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
  explicit EmptyMutationCallback(Document& document) : m_document(document) {}
  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->trace(m_document);
    MutationCallback::trace(visitor);
  }

 private:
  void call(const HeapVector<Member<MutationRecord>>&,
            MutationObserver*) override {}
  ExecutionContext* getExecutionContext() const override { return m_document; }

  Member<Document> m_document;
};
}

TEST(MutationObserverTest, DisconnectCrash) {
  Persistent<Document> document = HTMLDocument::create();
  HTMLElement* root = toHTMLElement(document->createElement("html"));
  document->appendChild(root);
  root->setInnerHTML("<head><title>\n</title></head><body></body>");
  Node* head = root->firstChild()->firstChild();
  DCHECK(head);
  Persistent<MutationObserver> observer =
      MutationObserver::create(new EmptyMutationCallback(*document));
  MutationObserverInit init;
  init.setCharacterDataOldValue(false);
  observer->observe(head, init, ASSERT_NO_EXCEPTION);

  head->remove();
  Persistent<MutationObserverRegistration> registration =
      observer->m_registrations.begin()->get();
  // The following GC will collect |head|, but won't collect a
  // MutationObserverRegistration for |head|.
  ThreadState::current()->collectGarbage(BlinkGC::NoHeapPointersOnStack,
                                         BlinkGC::GCWithoutSweep,
                                         BlinkGC::ForcedGC);
  observer->disconnect();
  // The test passes if disconnect() didn't crash.  crbug.com/657613.
}

}  // namespace blink
