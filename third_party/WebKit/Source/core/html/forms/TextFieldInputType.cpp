/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#include "core/html/forms/TextFieldInputType.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/HTMLNames.h"
#include "core/dom/ShadowRoot.h"
#include "core/editing/FrameSelection.h"
#include "core/events/BeforeTextInsertedEvent.h"
#include "core/events/KeyboardEvent.h"
#include "core/events/TextEvent.h"
#include "core/frame/LocalFrame.h"
#include "core/html/FormData.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/forms/TextControlInnerElements.h"
#include "core/html/shadow/ShadowElementNames.h"
#include "core/layout/LayoutDetailsMarker.h"
#include "core/layout/LayoutTextControlSingleLine.h"
#include "core/layout/LayoutTheme.h"
#include "core/page/ChromeClient.h"
#include "core/page/Page.h"
#include "core/paint/PaintLayer.h"
#include "platform/EventDispatchForbiddenScope.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

using namespace HTMLNames;

class DataListIndicatorElement final : public HTMLDivElement {
 private:
  inline DataListIndicatorElement(Document& document)
      : HTMLDivElement(document) {}
  inline HTMLInputElement* HostInput() const {
    return toHTMLInputElement(OwnerShadowHost());
  }

  LayoutObject* CreateLayoutObject(const ComputedStyle&) override {
    return new LayoutDetailsMarker(this);
  }

  EventDispatchHandlingState* PreDispatchEventHandler(Event* event) override {
    // Chromium opens autofill popup in a mousedown event listener
    // associated to the document. We don't want to open it in this case
    // because we opens a datalist chooser later.
    // FIXME: We should dispatch mousedown events even in such case.
    if (event->type() == EventTypeNames::mousedown)
      event->stopPropagation();
    return nullptr;
  }

  void DefaultEventHandler(Event* event) override {
    DCHECK(GetDocument().IsActive());
    if (event->type() != EventTypeNames::click)
      return;
    HTMLInputElement* host = HostInput();
    if (host && !host->IsDisabledOrReadOnly()) {
      GetDocument().GetPage()->GetChromeClient().OpenTextDataListChooser(*host);
      event->SetDefaultHandled();
    }
  }

  bool WillRespondToMouseClickEvents() override {
    return HostInput() && !HostInput()->IsDisabledOrReadOnly() &&
           GetDocument().IsActive();
  }

 public:
  static DataListIndicatorElement* Create(Document& document) {
    DataListIndicatorElement* element = new DataListIndicatorElement(document);
    element->SetShadowPseudoId(
        AtomicString("-webkit-calendar-picker-indicator"));
    element->setAttribute(idAttr, ShadowElementNames::PickerIndicator());
    return element;
  }
};

TextFieldInputType::TextFieldInputType(HTMLInputElement& element)
    : InputType(element), InputTypeView(element) {}

TextFieldInputType::~TextFieldInputType() {}

DEFINE_TRACE(TextFieldInputType) {
  InputTypeView::Trace(visitor);
  InputType::Trace(visitor);
}

InputTypeView* TextFieldInputType::CreateView() {
  return this;
}

InputType::ValueMode TextFieldInputType::GetValueMode() const {
  return ValueMode::kValue;
}

SpinButtonElement* TextFieldInputType::GetSpinButtonElement() const {
  return ToSpinButtonElementOrDie(
      GetElement().UserAgentShadowRoot()->getElementById(
          ShadowElementNames::SpinButton()));
}

bool TextFieldInputType::ShouldShowFocusRingOnMouseFocus() const {
  return true;
}

bool TextFieldInputType::IsTextField() const {
  return true;
}

bool TextFieldInputType::ValueMissing(const String& value) const {
  return GetElement().IsRequired() && value.IsEmpty();
}

bool TextFieldInputType::CanSetSuggestedValue() {
  return true;
}

void TextFieldInputType::SetValue(const String& sanitized_value,
                                  bool value_changed,
                                  TextFieldEventBehavior event_behavior,
                                  TextControlSetValueSelection selection) {
  // We don't use InputType::setValue.  TextFieldInputType dispatches events
  // different way from InputType::setValue.
  if (event_behavior == kDispatchNoEvent)
    GetElement().SetNonAttributeValue(sanitized_value);
  else
    GetElement().SetNonAttributeValueByUserEdit(sanitized_value);

  // The following early-return can't be moved to the beginning of this
  // function. We need to update non-attribute value even if the value is not
  // changed.  For example, <input type=number> has a badInput string, that is
  // to say, IDL value=="", and new value is "", which should clear the badInput
  // string and update validiity.
  if (!value_changed)
    return;
  GetElement().UpdateView();

  if (selection == TextControlSetValueSelection::kSetSelectionToEnd) {
    unsigned max = VisibleValue().length();
    GetElement().SetSelectionRange(max, max);
  }

  switch (event_behavior) {
    case kDispatchChangeEvent:
      // If the user is still editing this field, dispatch an input event rather
      // than a change event.  The change event will be dispatched when editing
      // finishes.
      if (GetElement().IsFocused())
        GetElement().DispatchInputEvent();
      else
        GetElement().DispatchFormControlChangeEvent();
      break;

    case kDispatchInputAndChangeEvent: {
      GetElement().DispatchInputEvent();
      GetElement().DispatchFormControlChangeEvent();
      break;
    }

    case kDispatchNoEvent:
      break;
  }
}

void TextFieldInputType::HandleKeydownEvent(KeyboardEvent* event) {
  if (!GetElement().IsFocused())
    return;
  if (ChromeClient* chrome_client = this->GetChromeClient()) {
    chrome_client->HandleKeyboardEventOnTextField(GetElement(), *event);
    return;
  }
  event->SetDefaultHandled();
}

void TextFieldInputType::HandleKeydownEventForSpinButton(KeyboardEvent* event) {
  if (GetElement().IsDisabledOrReadOnly())
    return;
  const String& key = event->key();
  if (key == "ArrowUp")
    SpinButtonStepUp();
  else if (key == "ArrowDown" && !event->altKey())
    SpinButtonStepDown();
  else
    return;
  GetElement().DispatchFormControlChangeEvent();
  event->SetDefaultHandled();
}

void TextFieldInputType::ForwardEvent(Event* event) {
  if (SpinButtonElement* spin_button = GetSpinButtonElement()) {
    spin_button->ForwardEvent(event);
    if (event->DefaultHandled())
      return;
  }

  if (GetElement().GetLayoutObject() &&
      (event->IsMouseEvent() || event->IsDragEvent() ||
       event->HasInterface(EventNames::WheelEvent) ||
       event->type() == EventTypeNames::blur ||
       event->type() == EventTypeNames::focus)) {
    LayoutTextControlSingleLine* layout_text_control =
        ToLayoutTextControlSingleLine(GetElement().GetLayoutObject());
    if (event->type() == EventTypeNames::blur) {
      if (LayoutBox* inner_editor_layout_object =
              GetElement().InnerEditorElement()->GetLayoutBox()) {
        // FIXME: This class has no need to know about PaintLayer!
        if (PaintLayer* inner_layer = inner_editor_layout_object->Layer()) {
          if (PaintLayerScrollableArea* inner_scrollable_area =
                  inner_layer->GetScrollableArea()) {
            inner_scrollable_area->SetScrollOffset(ScrollOffset(0, 0),
                                                   kProgrammaticScroll);
          }
        }
      }

      layout_text_control->CapsLockStateMayHaveChanged();
    } else if (event->type() == EventTypeNames::focus) {
      layout_text_control->CapsLockStateMayHaveChanged();
    }

    GetElement().ForwardEvent(event);
  }
}

void TextFieldInputType::HandleFocusEvent(Element* old_focused_node,
                                          WebFocusType focus_type) {
  InputTypeView::HandleFocusEvent(old_focused_node, focus_type);
  GetElement().BeginEditing();
}

void TextFieldInputType::HandleBlurEvent() {
  InputTypeView::HandleBlurEvent();
  GetElement().EndEditing();
  if (SpinButtonElement* spin_button = GetSpinButtonElement())
    spin_button->ReleaseCapture();
}

bool TextFieldInputType::ShouldSubmitImplicitly(Event* event) {
  return (event->type() == EventTypeNames::textInput &&
          event->HasInterface(EventNames::TextEvent) &&
          ToTextEvent(event)->data() == "\n") ||
         InputTypeView::ShouldSubmitImplicitly(event);
}

LayoutObject* TextFieldInputType::CreateLayoutObject(
    const ComputedStyle&) const {
  return new LayoutTextControlSingleLine(&GetElement());
}

bool TextFieldInputType::ShouldHaveSpinButton() const {
  return LayoutTheme::GetTheme().ShouldHaveSpinButton(&GetElement());
}

void TextFieldInputType::CreateShadowSubtree() {
  DCHECK(GetElement().Shadow());
  ShadowRoot* shadow_root = GetElement().UserAgentShadowRoot();
  DCHECK(!shadow_root->HasChildren());

  Document& document = GetElement().GetDocument();
  bool should_have_spin_button = this->ShouldHaveSpinButton();
  bool should_have_data_list_indicator = GetElement().HasValidDataListOptions();
  bool creates_container = should_have_spin_button ||
                           should_have_data_list_indicator || NeedsContainer();

  TextControlInnerEditorElement* inner_editor =
      TextControlInnerEditorElement::Create(document);
  if (!creates_container) {
    shadow_root->AppendChild(inner_editor);
    return;
  }

  TextControlInnerContainer* container =
      TextControlInnerContainer::Create(document);
  container->SetShadowPseudoId(
      AtomicString("-webkit-textfield-decoration-container"));
  shadow_root->AppendChild(container);

  EditingViewPortElement* editing_view_port =
      EditingViewPortElement::Create(document);
  editing_view_port->AppendChild(inner_editor);
  container->AppendChild(editing_view_port);

  if (should_have_data_list_indicator)
    container->AppendChild(DataListIndicatorElement::Create(document));
  // FIXME: Because of a special handling for a spin button in
  // LayoutTextControlSingleLine, we need to put it to the last position. It's
  // inconsistent with multiple-fields date/time types.
  if (should_have_spin_button)
    container->AppendChild(SpinButtonElement::Create(document, *this));

  // See listAttributeTargetChanged too.
}

Element* TextFieldInputType::ContainerElement() const {
  return GetElement().UserAgentShadowRoot()->getElementById(
      ShadowElementNames::TextFieldContainer());
}

void TextFieldInputType::DestroyShadowSubtree() {
  InputTypeView::DestroyShadowSubtree();
  if (SpinButtonElement* spin_button = GetSpinButtonElement())
    spin_button->RemoveSpinButtonOwner();
}

void TextFieldInputType::ListAttributeTargetChanged() {
  if (ChromeClient* chrome_client = this->GetChromeClient())
    chrome_client->TextFieldDataListChanged(GetElement());
  Element* picker = GetElement().UserAgentShadowRoot()->getElementById(
      ShadowElementNames::PickerIndicator());
  bool did_have_picker_indicator = picker;
  bool will_have_picker_indicator = GetElement().HasValidDataListOptions();
  if (did_have_picker_indicator == will_have_picker_indicator)
    return;
  EventDispatchForbiddenScope::AllowUserAgentEvents allow_events;
  if (will_have_picker_indicator) {
    Document& document = GetElement().GetDocument();
    if (Element* container = ContainerElement()) {
      container->InsertBefore(DataListIndicatorElement::Create(document),
                              GetSpinButtonElement());
    } else {
      // FIXME: The following code is similar to createShadowSubtree(),
      // but they are different. We should simplify the code by making
      // containerElement mandatory.
      Element* rp_container = TextControlInnerContainer::Create(document);
      rp_container->SetShadowPseudoId(
          AtomicString("-webkit-textfield-decoration-container"));
      Element* inner_editor = GetElement().InnerEditorElement();
      inner_editor->parentNode()->ReplaceChild(rp_container, inner_editor);
      Element* editing_view_port = EditingViewPortElement::Create(document);
      editing_view_port->AppendChild(inner_editor);
      rp_container->AppendChild(editing_view_port);
      rp_container->AppendChild(DataListIndicatorElement::Create(document));
      if (GetElement().GetDocument().FocusedElement() == GetElement())
        GetElement().UpdateFocusAppearance(SelectionBehaviorOnFocus::kRestore);
    }
  } else {
    picker->remove(ASSERT_NO_EXCEPTION);
  }
}

void TextFieldInputType::AttributeChanged() {
  // FIXME: Updating on any attribute update should be unnecessary. We should
  // figure out what attributes affect.
  UpdateView();
}

void TextFieldInputType::DisabledAttributeChanged() {
  if (SpinButtonElement* spin_button = GetSpinButtonElement())
    spin_button->ReleaseCapture();
}

void TextFieldInputType::ReadonlyAttributeChanged() {
  if (SpinButtonElement* spin_button = GetSpinButtonElement())
    spin_button->ReleaseCapture();
}

bool TextFieldInputType::SupportsReadOnly() const {
  return true;
}

static bool IsASCIILineBreak(UChar c) {
  return c == '\r' || c == '\n';
}

static String LimitLength(const String& string, unsigned max_length) {
  unsigned new_length = std::min(max_length, string.length());
  if (new_length == string.length())
    return string;
  if (new_length > 0 && U16_IS_LEAD(string[new_length - 1]))
    --new_length;
  return string.Left(new_length);
}

String TextFieldInputType::SanitizeValue(const String& proposed_value) const {
  return LimitLength(proposed_value.RemoveCharacters(IsASCIILineBreak),
                     std::numeric_limits<int>::max());
}

void TextFieldInputType::HandleBeforeTextInsertedEvent(
    BeforeTextInsertedEvent* event) {
  // Make sure that the text to be inserted will not violate the maxLength.

  // We use HTMLInputElement::innerEditorValue() instead of
  // HTMLInputElement::value() because they can be mismatched by
  // sanitizeValue() in HTMLInputElement::subtreeHasChanged() in some cases.
  unsigned old_length = GetElement().InnerEditorValue().length();

  // selectionLength represents the selection length of this text field to be
  // removed by this insertion.
  // If the text field has no focus, we don't need to take account of the
  // selection length. The selection is the source of text drag-and-drop in
  // that case, and nothing in the text field will be removed.
  unsigned selection_length = 0;
  if (GetElement().IsFocused()) {
    // TODO(editing-dev): Use of updateStyleAndLayoutIgnorePendingStylesheets
    // needs to be audited.  See http://crbug.com/590369 for more details.
    GetElement().GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();

    selection_length = GetElement()
                           .GetDocument()
                           .GetFrame()
                           ->Selection()
                           .SelectedText()
                           .length();
  }
  DCHECK_GE(old_length, selection_length);

  // Selected characters will be removed by the next text event.
  unsigned base_length = old_length - selection_length;
  unsigned max_length;
  if (this->MaxLength() < 0)
    max_length = std::numeric_limits<int>::max();
  else
    max_length = static_cast<unsigned>(this->MaxLength());
  unsigned appendable_length =
      max_length > base_length ? max_length - base_length : 0;

  // Truncate the inserted text to avoid violating the maxLength and other
  // constraints.
  String event_text = event->GetText();
  unsigned text_length = event_text.length();
  while (text_length > 0 && IsASCIILineBreak(event_text[text_length - 1]))
    text_length--;
  event_text.Truncate(text_length);
  event_text.Replace("\r\n", " ");
  event_text.Replace('\r', ' ');
  event_text.Replace('\n', ' ');

  event->SetText(LimitLength(event_text, appendable_length));
}

bool TextFieldInputType::ShouldRespectListAttribute() {
  return true;
}

void TextFieldInputType::UpdatePlaceholderText() {
  if (!SupportsPlaceholder())
    return;
  HTMLElement* placeholder = GetElement().PlaceholderElement();
  String placeholder_text = GetElement().StrippedPlaceholder();
  if (placeholder_text.IsEmpty()) {
    if (placeholder)
      placeholder->remove(ASSERT_NO_EXCEPTION);
    return;
  }
  if (!placeholder) {
    HTMLElement* new_element =
        HTMLDivElement::Create(GetElement().GetDocument());
    placeholder = new_element;
    placeholder->SetShadowPseudoId(AtomicString("-webkit-input-placeholder"));
    placeholder->SetInlineStyleProperty(
        CSSPropertyDisplay,
        GetElement().IsPlaceholderVisible() ? CSSValueBlock : CSSValueNone,
        true);
    placeholder->setAttribute(idAttr, ShadowElementNames::Placeholder());
    Element* container = ContainerElement();
    Node* previous = container ? container : GetElement().InnerEditorElement();
    previous->parentNode()->InsertBefore(placeholder, previous);
    SECURITY_DCHECK(placeholder->parentNode() == previous->parentNode());
  }
  placeholder->setTextContent(placeholder_text);
}

void TextFieldInputType::AppendToFormData(FormData& form_data) const {
  InputType::AppendToFormData(form_data);
  const AtomicString& dirname_attr_value =
      GetElement().FastGetAttribute(dirnameAttr);
  if (!dirname_attr_value.IsNull())
    form_data.append(dirname_attr_value, GetElement().DirectionForFormData());
}

String TextFieldInputType::ConvertFromVisibleValue(
    const String& visible_value) const {
  return visible_value;
}

void TextFieldInputType::SubtreeHasChanged() {
  GetElement().SetValueFromRenderer(SanitizeUserInputValue(
      ConvertFromVisibleValue(GetElement().InnerEditorValue())));
  GetElement().UpdatePlaceholderVisibility();
  GetElement().PseudoStateChanged(CSSSelector::kPseudoValid);
  GetElement().PseudoStateChanged(CSSSelector::kPseudoInvalid);
  GetElement().PseudoStateChanged(CSSSelector::kPseudoInRange);
  GetElement().PseudoStateChanged(CSSSelector::kPseudoOutOfRange);

  DidSetValueByUserEdit();
}

void TextFieldInputType::DidSetValueByUserEdit() {
  if (!GetElement().IsFocused())
    return;
  if (ChromeClient* chrome_client = this->GetChromeClient())
    chrome_client->DidChangeValueInTextField(GetElement());
}

void TextFieldInputType::SpinButtonStepDown() {
  StepUpFromLayoutObject(-1);
}

void TextFieldInputType::SpinButtonStepUp() {
  StepUpFromLayoutObject(1);
}

void TextFieldInputType::UpdateView() {
  if (!GetElement().SuggestedValue().IsNull()) {
    GetElement().SetInnerEditorValue(GetElement().SuggestedValue());
    GetElement().UpdatePlaceholderVisibility();
  } else if (GetElement().NeedsToUpdateViewValue()) {
    // Update the view only if needsToUpdateViewValue is true. It protects
    // an unacceptable view value from being overwritten with the DOM value.
    //
    // e.g. <input type=number> has a view value "abc", and input.max is
    // updated. In this case, updateView() is called but we should not
    // update the view value.
    GetElement().SetInnerEditorValue(VisibleValue());
    GetElement().UpdatePlaceholderVisibility();
  }
}

void TextFieldInputType::FocusAndSelectSpinButtonOwner() {
  GetElement().focus();
  GetElement().SetSelectionRange(0, std::numeric_limits<int>::max());
}

bool TextFieldInputType::ShouldSpinButtonRespondToMouseEvents() {
  return !GetElement().IsDisabledOrReadOnly();
}

bool TextFieldInputType::ShouldSpinButtonRespondToWheelEvents() {
  return ShouldSpinButtonRespondToMouseEvents() && GetElement().IsFocused();
}

void TextFieldInputType::SpinButtonDidReleaseMouseCapture(
    SpinButtonElement::EventDispatch event_dispatch) {
  if (event_dispatch == SpinButtonElement::kEventDispatchAllowed)
    GetElement().DispatchFormControlChangeEvent();
}

}  // namespace blink
