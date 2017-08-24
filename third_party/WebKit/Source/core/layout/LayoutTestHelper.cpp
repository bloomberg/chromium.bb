// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/LayoutTestHelper.h"

#include "bindings/core/v8/StringOrArrayBufferOrArrayBufferView.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "core/css/FontFaceDescriptors.h"
#include "core/css/FontFaceSetDocument.h"
#include "core/html/HTMLIFrameElement.h"
#include "core/typed_arrays/DOMArrayBuffer.h"
#include "platform/loader/fetch/MemoryCache.h"
#include "platform/scroll/ScrollbarTheme.h"
#include "platform/testing/UnitTestHelpers.h"

namespace blink {

LocalFrame* SingleChildLocalFrameClient::CreateFrame(
    const AtomicString& name,
    HTMLFrameOwnerElement* owner_element) {
  DCHECK(!child_) << "This test helper only supports one child frame.";

  LocalFrame* parent_frame = owner_element->GetDocument().GetFrame();
  auto* child_client = LocalFrameClientWithParent::Create(parent_frame);
  child_ =
      LocalFrame::Create(child_client, *parent_frame->GetPage(), owner_element);
  child_->CreateView(IntSize(500, 500), Color::kTransparent);
  child_->Init();

  return child_.Get();
}

void LocalFrameClientWithParent::Detached(FrameDetachType) {
  static_cast<SingleChildLocalFrameClient*>(Parent()->Client())
      ->DidDetachChild();
}

ChromeClient& RenderingTest::GetChromeClient() const {
  DEFINE_STATIC_LOCAL(EmptyChromeClient, client, (EmptyChromeClient::Create()));
  return client;
}

RenderingTest::RenderingTest(LocalFrameClient* local_frame_client)
    : local_frame_client_(local_frame_client) {}

void RenderingTest::SetUp() {
  Page::PageClients page_clients;
  FillWithEmptyClients(page_clients);
  page_clients.chrome_client = &GetChromeClient();
  page_holder_ =
      DummyPageHolder::Create(IntSize(800, 600), &page_clients,
                              local_frame_client_, SettingOverrider());

  Settings::SetMockScrollbarsEnabled(true);
  RuntimeEnabledFeatures::SetOverlayScrollbarsEnabled(true);
  EXPECT_TRUE(ScrollbarTheme::GetTheme().UsesOverlayScrollbars());

  // This ensures that the minimal DOM tree gets attached
  // correctly for tests that don't call setBodyInnerHTML.
  GetDocument().View()->UpdateAllLifecyclePhases();

  // Allow ASSERT_DEATH and EXPECT_DEATH for multiple threads.
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";
}

void RenderingTest::TearDown() {
  // We need to destroy most of the Blink structure here because derived tests
  // may restore RuntimeEnabledFeatures setting during teardown, which happens
  // before our destructor getting invoked, breaking the assumption that REF
  // can't change during Blink lifetime.
  page_holder_ = nullptr;

  // Clear memory cache, otherwise we can leak pruned resources.
  GetMemoryCache()->EvictResources();
}

void RenderingTest::SetChildFrameHTML(const String& html) {
  ChildDocument().SetBaseURLOverride(KURL(kParsedURLString, "http://test.com"));
  ChildDocument().body()->setInnerHTML(html, ASSERT_NO_EXCEPTION);
}

void RenderingTest::LoadAhem() {
  LoadAhem(page_holder_->GetFrame());
}

void RenderingTest::LoadAhem(LocalFrame& frame) {
  Document& document = *frame.DomWindow()->document();
  RefPtr<SharedBuffer> shared_buffer =
      testing::ReadFromFile(testing::CoreTestDataPath("Ahem.ttf"));
  StringOrArrayBufferOrArrayBufferView buffer =
      StringOrArrayBufferOrArrayBufferView::fromArrayBuffer(
          DOMArrayBuffer::Create(shared_buffer));
  FontFace* ahem =
      FontFace::Create(&document, "Ahem", buffer, FontFaceDescriptors());

  ScriptState* script_state = ToScriptStateForMainWorld(&frame);
  DummyExceptionStateForTesting exception_state;
  FontFaceSetDocument::From(document)->addForBinding(script_state, ahem,
                                                     exception_state);
}

}  // namespace blink
