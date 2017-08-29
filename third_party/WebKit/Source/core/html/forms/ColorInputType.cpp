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

#include "core/html/forms/ColorInputType.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptController.h"
#include "core/CSSPropertyNames.h"
#include "core/InputTypeNames.h"
#include "core/dom/ShadowRoot.h"
#include "core/dom/UserGestureIndicator.h"
#include "core/dom/events/ScopedEventQueue.h"
#include "core/events/MouseEvent.h"
#include "core/frame/LocalFrameView.h"
#include "core/html/HTMLDataListElement.h"
#include "core/html/HTMLDataListOptionsCollection.h"
#include "core/html/HTMLDivElement.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLOptionElement.h"
#include "core/html/forms/ColorChooser.h"
#include "core/layout/LayoutTheme.h"
#include "core/page/ChromeClient.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/Color.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

using namespace HTMLNames;

// Upper limit of number of datalist suggestions shown.
static const unsigned kMaxSuggestions = 1000;
// Upper limit for the length of the labels for datalist suggestions.
static const unsigned kMaxSuggestionLabelLength = 1000;

static bool IsValidColorString(const String& value) {
  if (value.IsEmpty())
    return false;
  if (value[0] != '#')
    return false;

  // We don't accept #rgb and #aarrggbb formats.
  if (value.length() != 7)
    return false;
  Color color;
  return color.SetFromString(value) && !color.HasAlpha();
}

ColorInputType::ColorInputType(HTMLInputElement& element)
    : InputType(element), KeyboardClickableInputTypeView(element) {}

InputType* ColorInputType::Create(HTMLInputElement& element) {
  return new ColorInputType(element);
}

ColorInputType::~ColorInputType() {}

DEFINE_TRACE(ColorInputType) {
  visitor->Trace(chooser_);
  KeyboardClickableInputTypeView::Trace(visitor);
  ColorChooserClient::Trace(visitor);
  InputType::Trace(visitor);
}

InputTypeView* ColorInputType::CreateView() {
  return this;
}

InputType::ValueMode ColorInputType::GetValueMode() const {
  return ValueMode::kValue;
}

void ColorInputType::CountUsage() {
  CountUsageIfVisible(WebFeature::kInputTypeColor);
}

const AtomicString& ColorInputType::FormControlType() const {
  return InputTypeNames::color;
}

bool ColorInputType::SupportsRequired() const {
  return false;
}

String ColorInputType::SanitizeValue(const String& proposed_value) const {
  if (!IsValidColorString(proposed_value))
    return "#000000";
  return proposed_value.DeprecatedLower();
}

Color ColorInputType::ValueAsColor() const {
  Color color;
  bool success = color.SetFromString(GetElement().value());
  DCHECK(success);
  return color;
}

void ColorInputType::CreateShadowSubtree() {
  DCHECK(GetElement().Shadow());

  Document& document = GetElement().GetDocument();
  HTMLDivElement* wrapper_element = HTMLDivElement::Create(document);
  wrapper_element->SetShadowPseudoId(
      AtomicString("-webkit-color-swatch-wrapper"));
  HTMLDivElement* color_swatch = HTMLDivElement::Create(document);
  color_swatch->SetShadowPseudoId(AtomicString("-webkit-color-swatch"));
  wrapper_element->AppendChild(color_swatch);
  GetElement().UserAgentShadowRoot()->AppendChild(wrapper_element);

  GetElement().UpdateView();
}

void ColorInputType::DidSetValue(const String&, bool value_changed) {
  if (!value_changed)
    return;
  GetElement().UpdateView();
  if (chooser_)
    chooser_->SetSelectedColor(ValueAsColor());
}

void ColorInputType::HandleDOMActivateEvent(Event* event) {
  if (GetElement().IsDisabledFormControl())
    return;

  if (!UserGestureIndicator::ProcessingUserGesture())
    return;

  ChromeClient* chrome_client = this->GetChromeClient();
  if (chrome_client && !chooser_)
    chooser_ = chrome_client->OpenColorChooser(
        GetElement().GetDocument().GetFrame(), this, ValueAsColor());

  event->SetDefaultHandled();
}

void ColorInputType::ClosePopupView() {
  EndColorChooser();
}

bool ColorInputType::ShouldRespectListAttribute() {
  return true;
}

bool ColorInputType::TypeMismatchFor(const String& value) const {
  return !IsValidColorString(value);
}

void ColorInputType::WarnIfValueIsInvalid(const String& value) const {
  if (!DeprecatedEqualIgnoringCase(value, GetElement().SanitizeValue(value)))
    AddWarningToConsole(
        "The specified value %s does not conform to the required format.  The "
        "format is \"#rrggbb\" where rr, gg, bb are two-digit hexadecimal "
        "numbers.",
        value);
}

void ColorInputType::ValueAttributeChanged() {
  if (!GetElement().HasDirtyValue())
    GetElement().UpdateView();
}

void ColorInputType::DidChooseColor(const Color& color) {
  if (GetElement().IsDisabledFormControl() || color == ValueAsColor())
    return;
  EventQueueScope scope;
  GetElement().SetValueFromRenderer(color.Serialized());
  GetElement().UpdateView();
  if (!LayoutTheme::GetTheme().IsModalColorChooser())
    GetElement().DispatchFormControlChangeEvent();
}

void ColorInputType::DidEndChooser() {
  if (LayoutTheme::GetTheme().IsModalColorChooser())
    GetElement().EnqueueChangeEvent();
  chooser_.Clear();
}

void ColorInputType::EndColorChooser() {
  if (chooser_)
    chooser_->EndChooser();
}

void ColorInputType::UpdateView() {
  HTMLElement* color_swatch = ShadowColorSwatch();
  if (!color_swatch)
    return;

  color_swatch->SetInlineStyleProperty(CSSPropertyBackgroundColor,
                                       GetElement().value());
}

HTMLElement* ColorInputType::ShadowColorSwatch() const {
  ShadowRoot* shadow = GetElement().UserAgentShadowRoot();
  return shadow ? ToHTMLElementOrDie(shadow->firstChild()->firstChild())
                : nullptr;
}

Element& ColorInputType::OwnerElement() const {
  return GetElement();
}

IntRect ColorInputType::ElementRectRelativeToViewport() const {
  return GetElement().GetDocument().View()->ContentsToViewport(
      GetElement().PixelSnappedBoundingBox());
}

Color ColorInputType::CurrentColor() {
  return ValueAsColor();
}

bool ColorInputType::ShouldShowSuggestions() const {
  return GetElement().FastHasAttribute(listAttr);
}

Vector<ColorSuggestion> ColorInputType::Suggestions() const {
  Vector<ColorSuggestion> suggestions;
  HTMLDataListElement* data_list = GetElement().DataList();
  if (data_list) {
    HTMLDataListOptionsCollection* options = data_list->options();
    for (unsigned i = 0; HTMLOptionElement* option = options->Item(i); i++) {
      if (option->IsDisabledFormControl() || option->value().IsEmpty())
        continue;
      if (!GetElement().IsValidValue(option->value()))
        continue;
      Color color;
      if (!color.SetFromString(option->value()))
        continue;
      ColorSuggestion suggestion(
          color, option->label().Left(kMaxSuggestionLabelLength));
      suggestions.push_back(suggestion);
      if (suggestions.size() >= kMaxSuggestions)
        break;
    }
  }
  return suggestions;
}

AXObject* ColorInputType::PopupRootAXObject() {
  return chooser_ ? chooser_->RootAXObject() : nullptr;
}

ColorChooserClient* ColorInputType::GetColorChooserClient() {
  return this;
}

}  // namespace blink
