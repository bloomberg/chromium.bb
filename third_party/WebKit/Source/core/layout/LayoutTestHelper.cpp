// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/LayoutTestHelper.h"

#include "bindings/core/v8/StringOrArrayBufferOrArrayBufferView.h"
#include "core/css/FontFaceDescriptors.h"
#include "core/css/FontFaceSet.h"
#include "core/dom/DOMArrayBuffer.h"
#include "core/frame/FrameHost.h"
#include "core/html/HTMLIFrameElement.h"
#include "platform/loader/fetch/MemoryCache.h"
#include "platform/scroll/ScrollbarTheme.h"
#include "platform/testing/UnitTestHelpers.h"

namespace blink {

LocalFrame* SingleChildLocalFrameClient::createFrame(
    const FrameLoadRequest&,
    const AtomicString& name,
    HTMLFrameOwnerElement* ownerElement) {
  DCHECK(!m_child) << "This test helper only supports one child frame.";

  LocalFrame* parentFrame = ownerElement->document().frame();
  auto* childClient = LocalFrameClientWithParent::create(parentFrame);
  m_child = LocalFrame::create(childClient, parentFrame->host(), ownerElement);
  m_child->createView(IntSize(500, 500), Color(), true /* transparent */);
  m_child->init();

  return m_child.get();
}

void LocalFrameClientWithParent::detached(FrameDetachType) {
  static_cast<SingleChildLocalFrameClient*>(parent()->client())
      ->didDetachChild();
}

ChromeClient& RenderingTest::chromeClient() const {
  DEFINE_STATIC_LOCAL(EmptyChromeClient, client, (EmptyChromeClient::create()));
  return client;
}

RenderingTest::RenderingTest(LocalFrameClient* localFrameClient)
    : m_localFrameClient(localFrameClient) {}

void RenderingTest::SetUp() {
  Page::PageClients pageClients;
  fillWithEmptyClients(pageClients);
  pageClients.chromeClient = &chromeClient();
  m_pageHolder = DummyPageHolder::create(
      IntSize(800, 600), &pageClients, m_localFrameClient, settingOverrider());

  Settings::setMockScrollbarsEnabled(true);
  RuntimeEnabledFeatures::setOverlayScrollbarsEnabled(true);
  EXPECT_TRUE(ScrollbarTheme::theme().usesOverlayScrollbars());

  // This ensures that the minimal DOM tree gets attached
  // correctly for tests that don't call setBodyInnerHTML.
  document().view()->updateAllLifecyclePhases();
}

void RenderingTest::TearDown() {
  // We need to destroy most of the Blink structure here because derived tests
  // may restore RuntimeEnabledFeatures setting during teardown, which happens
  // before our destructor getting invoked, breaking the assumption that REF
  // can't change during Blink lifetime.
  m_pageHolder = nullptr;

  // Clear memory cache, otherwise we can leak pruned resources.
  memoryCache()->evictResources();
}

void RenderingTest::setChildFrameHTML(const String& html) {
  childDocument().setBaseURLOverride(KURL(ParsedURLString, "http://test.com"));
  childDocument().body()->setInnerHTML(html, ASSERT_NO_EXCEPTION);
}

void RenderingTest::loadAhem() {
  RefPtr<SharedBuffer> sharedBuffer =
      testing::readFromFile(testing::webTestDataPath("Ahem.ttf"));
  StringOrArrayBufferOrArrayBufferView buffer =
      StringOrArrayBufferOrArrayBufferView::fromArrayBuffer(
          DOMArrayBuffer::create(sharedBuffer->data(), sharedBuffer->size()));
  FontFace* ahem =
      FontFace::create(&document(), "Ahem", buffer, FontFaceDescriptors());

  ScriptState* scriptState = ScriptState::forMainWorld(&m_pageHolder->frame());
  DummyExceptionStateForTesting exceptionState;
  FontFaceSet::from(document())
      ->addForBinding(scriptState, ahem, exceptionState);
}

}  // namespace blink
