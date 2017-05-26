// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/EditingTestBase.h"

#include "core/dom/Document.h"
#include "core/dom/Range.h"
#include "core/dom/Text.h"
#include "core/frame/LocalFrameView.h"
#include "core/html/HTMLElement.h"
#include "core/testing/DummyPageHolder.h"

namespace blink {

EditingTestBase::EditingTestBase() {}

EditingTestBase::~EditingTestBase() {}

Document& EditingTestBase::GetDocument() const {
  return dummy_page_holder_->GetDocument();
}

LocalFrame& EditingTestBase::GetFrame() const {
  return GetDummyPageHolder().GetFrame();
}

FrameSelection& EditingTestBase::Selection() const {
  return GetFrame().Selection();
}

void EditingTestBase::SetUp() {
  dummy_page_holder_ = DummyPageHolder::Create(IntSize(800, 600));
}

void EditingTestBase::SetupPageWithClients(Page::PageClients* clients) {
  DCHECK(!dummy_page_holder_) << "Page should be set up only once";
  dummy_page_holder_ = DummyPageHolder::Create(IntSize(800, 600), clients);
}

ShadowRoot* EditingTestBase::CreateShadowRootForElementWithIDAndSetInnerHTML(
    TreeScope& scope,
    const char* host_element_id,
    const char* shadow_root_content) {
  ShadowRoot* shadow_root =
      scope.getElementById(AtomicString::FromUTF8(host_element_id))
          ->CreateShadowRootInternal(ShadowRootType::V0, ASSERT_NO_EXCEPTION);
  shadow_root->setInnerHTML(String::FromUTF8(shadow_root_content),
                            ASSERT_NO_EXCEPTION);
  scope.GetDocument().View()->UpdateAllLifecyclePhases();
  return shadow_root;
}

void EditingTestBase::SetBodyContent(const std::string& body_content) {
  GetDocument().body()->setInnerHTML(String::FromUTF8(body_content.c_str()),
                                     ASSERT_NO_EXCEPTION);
  UpdateAllLifecyclePhases();
}

ShadowRoot* EditingTestBase::SetShadowContent(const char* shadow_content,
                                              const char* host) {
  ShadowRoot* shadow_root = CreateShadowRootForElementWithIDAndSetInnerHTML(
      GetDocument(), host, shadow_content);
  return shadow_root;
}

void EditingTestBase::UpdateAllLifecyclePhases() {
  GetDocument().View()->UpdateAllLifecyclePhases();
}

}  // namespace blink
