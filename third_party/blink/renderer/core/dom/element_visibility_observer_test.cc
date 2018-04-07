// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/element_visibility_observer.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_implementation.h"
#include "third_party/blink/renderer/core/frame/remote_frame.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

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

  virtual void Trace(blink::Visitor* visitor) {
    visitor->Trace(parent_);
    EmptyLocalFrameClient::Trace(visitor);
  }

 private:
  WeakMember<Frame> parent_ = nullptr;
};

class ElementVisibilityObserverTest : public PageTestBase {
 protected:
  void SetUp() override {
    local_frame_client_ = new DOMStubLocalFrameClient();
    SetupPageWithClients(nullptr, local_frame_client_, nullptr);
  }

  void TearDown() override { GetFrame().Detach(FrameDetachType::kRemove); }

  DOMStubLocalFrameClient* LocalFrameClient() const {
    return local_frame_client_;
  }

 private:
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
      new ElementVisibilityObserver(element, WTF::BindRepeating([](bool) {}));
  observer->Start();
  observer->DeliverObservationsForTesting();
  observer->Stop();
  // It should not crash.
}

}  // anonymous namespace

}  // blink namespace
