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
class StubLocalFrameClient final : public EmptyLocalFrameClient {
 public:
  Frame* parent() const override { return m_parent; }
  Frame* top() const override { return m_parent; }

  void setParent(Frame* frame) { m_parent = frame; }

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->trace(m_parent);
    EmptyLocalFrameClient::trace(visitor);
  }

 private:
  WeakMember<Frame> m_parent = nullptr;
};

class ElementVisibilityObserverTest : public ::testing::Test {
 protected:
  void SetUp() override {
    m_localFrameClient = new StubLocalFrameClient();
    m_dummyPageHolder = DummyPageHolder::create(
        IntSize(), nullptr, m_localFrameClient, nullptr, nullptr);
  }

  void TearDown() override {
    m_dummyPageHolder->frame().detach(FrameDetachType::Remove);
  }

  Document& document() { return m_dummyPageHolder->document(); }
  FrameHost& frameHost() { return m_dummyPageHolder->page().frameHost(); }
  StubLocalFrameClient* localFrameClient() const { return m_localFrameClient; }

 private:
  std::unique_ptr<DummyPageHolder> m_dummyPageHolder;
  Persistent<StubLocalFrameClient> m_localFrameClient;
};

TEST_F(ElementVisibilityObserverTest, ObserveElementWithoutDocumentFrame) {
  HTMLElement* element = HTMLDivElement::create(
      *DOMImplementation::create(document())->createHTMLDocument("test"));
  ElementVisibilityObserver* observer =
      new ElementVisibilityObserver(element, nullptr);
  observer->start();
  observer->stop();
  // It should not crash.
}

TEST_F(ElementVisibilityObserverTest, ObserveElementInRemoteFrame) {
  Persistent<RemoteFrame> remoteFrame =
      RemoteFrame::create(new EmptyRemoteFrameClient(), &frameHost(), nullptr);
  localFrameClient()->setParent(remoteFrame);

  Persistent<HTMLElement> element = HTMLDivElement::create(document());
  ElementVisibilityObserver* observer =
      new ElementVisibilityObserver(element, WTF::bind([](bool) {}));
  observer->start();
  observer->deliverObservationsForTesting();
  observer->stop();
  // It should not crash.
}

}  // anonymous namespace

}  // blink namespace
