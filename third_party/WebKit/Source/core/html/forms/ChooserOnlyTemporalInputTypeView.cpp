/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "core/html/forms/ChooserOnlyTemporalInputTypeView.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/Document.h"
#include "core/dom/ShadowRoot.h"
#include "core/dom/UserGestureIndicator.h"
#include "core/html/HTMLDivElement.h"
#include "core/html/forms/HTMLInputElement.h"
#include "core/page/ChromeClient.h"
#include "core/page/Page.h"

namespace blink {

ChooserOnlyTemporalInputTypeView::ChooserOnlyTemporalInputTypeView(
    HTMLInputElement& element,
    BaseTemporalInputType& input_type)
    : KeyboardClickableInputTypeView(element), input_type_(input_type) {}

ChooserOnlyTemporalInputTypeView* ChooserOnlyTemporalInputTypeView::Create(
    HTMLInputElement& element,
    BaseTemporalInputType& input_type) {
  return new ChooserOnlyTemporalInputTypeView(element, input_type);
}

ChooserOnlyTemporalInputTypeView::~ChooserOnlyTemporalInputTypeView() {
  DCHECK(!date_time_chooser_);
}

void ChooserOnlyTemporalInputTypeView::Trace(blink::Visitor* visitor) {
  visitor->Trace(input_type_);
  visitor->Trace(date_time_chooser_);
  InputTypeView::Trace(visitor);
  DateTimeChooserClient::Trace(visitor);
}

void ChooserOnlyTemporalInputTypeView::HandleDOMActivateEvent(Event*) {
  if (GetElement().IsDisabledOrReadOnly() || !GetElement().GetLayoutObject() ||
      !UserGestureIndicator::ProcessingUserGesture() ||
      GetElement().OpenShadowRoot())
    return;

  if (date_time_chooser_)
    return;
  if (!GetElement().GetDocument().IsActive())
    return;
  DateTimeChooserParameters parameters;
  if (!GetElement().SetupDateTimeChooserParameters(parameters))
    return;
  date_time_chooser_ = GetElement()
                           .GetDocument()
                           .GetPage()
                           ->GetChromeClient()
                           .OpenDateTimeChooser(this, parameters);
}

void ChooserOnlyTemporalInputTypeView::CreateShadowSubtree() {
  DEFINE_STATIC_LOCAL(AtomicString, value_container_pseudo,
                      ("-webkit-date-and-time-value"));

  HTMLDivElement* value_container =
      HTMLDivElement::Create(GetElement().GetDocument());
  value_container->SetShadowPseudoId(value_container_pseudo);
  GetElement().UserAgentShadowRoot()->AppendChild(value_container);
  UpdateView();
}

void ChooserOnlyTemporalInputTypeView::UpdateView() {
  Node* node = GetElement().UserAgentShadowRoot()->firstChild();
  if (!node || !node->IsHTMLElement())
    return;
  String display_value;
  if (!GetElement().SuggestedValue().IsNull())
    display_value = GetElement().SuggestedValue();
  else
    display_value = input_type_->VisibleValue();
  if (display_value.IsEmpty()) {
    // Need to put something to keep text baseline.
    display_value = " ";
  }
  ToHTMLElement(node)->setTextContent(display_value);
}

void ChooserOnlyTemporalInputTypeView::DidSetValue(const String& value,
                                                   bool value_changed) {
  if (value_changed)
    UpdateView();
}

void ChooserOnlyTemporalInputTypeView::ClosePopupView() {
  CloseDateTimeChooser();
}

Element& ChooserOnlyTemporalInputTypeView::OwnerElement() const {
  return GetElement();
}

void ChooserOnlyTemporalInputTypeView::DidChooseValue(const String& value) {
  GetElement().setValue(value, kDispatchInputAndChangeEvent);
}

void ChooserOnlyTemporalInputTypeView::DidChooseValue(double value) {
  DCHECK(std::isfinite(value) || std::isnan(value));
  if (std::isnan(value))
    GetElement().setValue(g_empty_string, kDispatchInputAndChangeEvent);
  else
    GetElement().setValueAsNumber(value, ASSERT_NO_EXCEPTION,
                                  kDispatchInputAndChangeEvent);
}

void ChooserOnlyTemporalInputTypeView::DidEndChooser() {
  date_time_chooser_.Clear();
}

void ChooserOnlyTemporalInputTypeView::CloseDateTimeChooser() {
  if (date_time_chooser_)
    date_time_chooser_->EndChooser();
}

}  // namespace blink
