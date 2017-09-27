// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/testing/EditingTestBase.h"

#include "core/dom/Document.h"
#include "core/dom/Range.h"
#include "core/dom/Text.h"
#include "core/editing/Position.h"
#include "core/editing/SelectionTemplate.h"
#include "core/editing/testing/SelectionSample.h"
#include "core/frame/LocalFrameView.h"
#include "core/html/HTMLCollection.h"
#include "core/html/HTMLElement.h"
#include "core/testing/DummyPageHolder.h"

namespace blink {

namespace {

Element* GetOrCreateElement(ContainerNode* parent,
                            const HTMLQualifiedName& tag_name) {
  HTMLCollection* elements = parent->getElementsByTagNameNS(
      tag_name.NamespaceURI(), tag_name.LocalName());
  if (!elements->IsEmpty())
    return elements->item(0);
  return parent->ownerDocument()->createElement(tag_name,
                                                kCreatedByCreateElement);
}

}  // namespace

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

void EditingTestBase::InsertStyleElement(const std::string& style_rules) {
  Element* const head = GetOrCreateElement(&GetDocument(), HTMLNames::headTag);
  DCHECK_EQ(head, GetOrCreateElement(&GetDocument(), HTMLNames::headTag));
  Element* const style =
      GetDocument().createElement(HTMLNames::styleTag, kCreatedByCreateElement);
  style->setTextContent(String(style_rules.data(), style_rules.size()));
  head->appendChild(style);
}

Position EditingTestBase::SetCaretTextToBody(
    const std::string& selection_text) {
  const SelectionInDOMTree selection = SetSelectionTextToBody(selection_text);
  DCHECK(selection.IsCaret())
      << "|selection_text| should contain a caret marker '|'";
  return selection.Base();
}

SelectionInDOMTree EditingTestBase::SetSelectionTextToBody(
    const std::string& selection_text) {
  return SetSelectionText(GetDocument().body(), selection_text);
}

SelectionInDOMTree EditingTestBase::SetSelectionText(
    HTMLElement* element,
    const std::string& selection_text) {
  const SelectionInDOMTree selection =
      SelectionSample::SetSelectionText(element, selection_text);
  UpdateAllLifecyclePhases();
  return selection;
}

std::string EditingTestBase::GetSelectionTextFromBody(
    const SelectionInDOMTree& selection) const {
  return SelectionSample::GetSelectionText(*GetDocument().body(), selection);
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
  ShadowRoot& shadow_root =
      scope.getElementById(AtomicString::FromUTF8(host_element_id))
          ->CreateShadowRootInternal();
  shadow_root.SetInnerHTMLFromString(String::FromUTF8(shadow_root_content),
                                     ASSERT_NO_EXCEPTION);
  scope.GetDocument().View()->UpdateAllLifecyclePhases();
  return &shadow_root;
}

void EditingTestBase::SetBodyContent(const std::string& body_content) {
  GetDocument().body()->SetInnerHTMLFromString(
      String::FromUTF8(body_content.c_str()), ASSERT_NO_EXCEPTION);
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
