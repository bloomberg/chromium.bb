// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/ElementVisibilityObserver.h"

#include "core/dom/DOMImplementation.h"
#include "core/dom/Document.h"
#include "core/frame/RemoteFrame.h"
#include "core/html/HTMLDivElement.h"
#include "core/html/HTMLDocument.h"
#include "core/loader/EmptyClients.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

// Stub implementation of LocalFrameClient for the purpose of testing. It will
// alow callers to set the parent/top frames by calling |setParent|. It is used
// in ElementVisibilityObserverTest in order to mock a RemoteFrame parent of a
// LocalFrame.
class DOMStubLocalFrameClient final : public EmptyLocalFrameClient {
 public:
  Frame* Parent() const override { return parent_; }
  Frame* Top() const override { return parent_; }

  void SetParent(Frame* frame) { parent_ = frame; }

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->Trace(parent_);
    EmptyLocalFrameClient::Trace(visitor);
  }

 private:
  WeakMember<Frame> parent_ = nullptr;
};

class ElementVisibilityObserverTest : public ::testing::Test {
 protected:
  void SetUp() override {
    local_frame_client_ = new DOMStubLocalFrameClient();
    dummy_page_holder_ = DummyPageHolder::Create(IntSize(), nullptr,
                                                 local_frame_client_, nullptr);
  }

  void TearDown() override {
    dummy_page_holder_->GetFrame().Detach(FrameDetachType::kRemove);
  }

  Document& GetDocument() { return dummy_page_holder_->GetDocument(); }
  Page& GetPage() { return dummy_page_holder_->GetPage(); }
  DOMStubLocalFrameClient* LocalFrameClient() const {
    return local_frame_client_;
  }

 private:
  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
  Persistent<DOMStubLocalFrameClient> local_frame_client_;
};

TEST_F(ElementVisibilityObserverTest, ObserveElementWithoutDocumentFrame) {
  HTMLElement* element = HTMLDivElement::Create(
      *DOMImplementation::Create(GetDocument())->createHTMLDocument("test"));
  ElementVisibilityObserver* observer = new ElementVisibilityObserver(
      element, ElementVisibilityObserver::VisibilityCallback());
  observer->Start();
  observer->Stop();
  // It should not crash.
}

TEST_F(ElementVisibilityObserverTest, ObserveElementInRemoteFrame) {
  Persistent<RemoteFrame> remote_frame =
      RemoteFrame::Create(new EmptyRemoteFrameClient(), GetPage(), nullptr);
  LocalFrameClient()->SetParent(remote_frame);

  Persistent<HTMLElement> element = HTMLDivElement::Create(GetDocument());
  ElementVisibilityObserver* observer =
      new ElementVisibilityObserver(element, WTF::Bind([](bool) {}));
  observer->Start();
  observer->DeliverObservationsForTesting();
  observer->Stop();
  // It should not crash.
}

}  // anonymous namespace

}  // blink namespace
