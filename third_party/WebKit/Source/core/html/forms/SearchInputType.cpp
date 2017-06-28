/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "core/html/forms/SearchInputType.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/HTMLNames.h"
#include "core/InputTypeNames.h"
#include "core/dom/ShadowRoot.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/events/KeyboardEvent.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/forms/TextControlInnerElements.h"
#include "core/html/shadow/ShadowElementNames.h"
#include "core/layout/LayoutSearchField.h"

namespace blink {

using namespace HTMLNames;

inline SearchInputType::SearchInputType(HTMLInputElement& element)
    : BaseTextInputType(element),
      search_event_timer_(TaskRunnerHelper::Get(TaskType::kUserInteraction,
                                                &element.GetDocument()),
                          this,
                          &SearchInputType::SearchEventTimerFired) {}

InputType* SearchInputType::Create(HTMLInputElement& element) {
  return new SearchInputType(element);
}

void SearchInputType::CountUsage() {
  CountUsageIfVisible(WebFeature::kInputTypeSearch);
}

LayoutObject* SearchInputType::CreateLayoutObject(const ComputedStyle&) const {
  return new LayoutSearchField(&GetElement());
}

const AtomicString& SearchInputType::FormControlType() const {
  return InputTypeNames::search;
}

bool SearchInputType::NeedsContainer() const {
  return true;
}

void SearchInputType::CreateShadowSubtree() {
  TextFieldInputType::CreateShadowSubtree();
  Element* container = ContainerElement();
  Element* view_port = GetElement().UserAgentShadowRoot()->getElementById(
      ShadowElementNames::EditingViewPort());
  DCHECK(container);
  DCHECK(view_port);
  container->InsertBefore(
      SearchFieldCancelButtonElement::Create(GetElement().GetDocument()),
      view_port->nextSibling());
}

void SearchInputType::HandleKeydownEvent(KeyboardEvent* event) {
  if (GetElement().IsDisabledOrReadOnly()) {
    TextFieldInputType::HandleKeydownEvent(event);
    return;
  }

  const String& key = event->key();
  if (key == "Escape") {
    GetElement().SetValueForUser("");
    GetElement().OnSearch();
    event->SetDefaultHandled();
    return;
  }
  TextFieldInputType::HandleKeydownEvent(event);
}

void SearchInputType::StartSearchEventTimer() {
  DCHECK(GetElement().GetLayoutObject());
  unsigned length = GetElement().InnerEditorValue().length();

  if (!length) {
    search_event_timer_.Stop();
    TaskRunnerHelper::Get(TaskType::kUserInteraction,
                          &GetElement().GetDocument())
        ->PostTask(BLINK_FROM_HERE, WTF::Bind(&HTMLInputElement::OnSearch,
                                              WrapPersistent(&GetElement())));
    return;
  }

  // After typing the first key, we wait 0.5 seconds.
  // After the second key, 0.4 seconds, then 0.3, then 0.2 from then on.
  search_event_timer_.StartOneShot(max(0.2, 0.6 - 0.1 * length),
                                   BLINK_FROM_HERE);
}

void SearchInputType::DispatchSearchEvent() {
  search_event_timer_.Stop();
  GetElement().DispatchEvent(Event::CreateBubble(EventTypeNames::search));
}

void SearchInputType::SearchEventTimerFired(TimerBase*) {
  GetElement().OnSearch();
}

bool SearchInputType::SearchEventsShouldBeDispatched() const {
  return GetElement().hasAttribute(incrementalAttr);
}

void SearchInputType::DidSetValueByUserEdit() {
  UpdateCancelButtonVisibility();

  // If the incremental attribute is set, then dispatch the search event
  if (SearchEventsShouldBeDispatched())
    StartSearchEventTimer();

  TextFieldInputType::DidSetValueByUserEdit();
}

void SearchInputType::UpdateView() {
  BaseTextInputType::UpdateView();
  UpdateCancelButtonVisibility();
}

const AtomicString& SearchInputType::DefaultAutocapitalize() const {
  DEFINE_STATIC_LOCAL(const AtomicString, sentences, ("sentences"));
  return sentences;
}

void SearchInputType::UpdateCancelButtonVisibility() {
  Element* button = GetElement().UserAgentShadowRoot()->getElementById(
      ShadowElementNames::SearchClearButton());
  if (!button)
    return;
  if (GetElement().value().IsEmpty()) {
    button->SetInlineStyleProperty(CSSPropertyOpacity, 0.0,
                                   CSSPrimitiveValue::UnitType::kNumber);
    button->SetInlineStyleProperty(CSSPropertyPointerEvents, CSSValueNone);
  } else {
    button->RemoveInlineStyleProperty(CSSPropertyOpacity);
    button->RemoveInlineStyleProperty(CSSPropertyPointerEvents);
  }
}

bool SearchInputType::SupportsInputModeAttribute() const {
  return true;
}

}  // namespace blink
