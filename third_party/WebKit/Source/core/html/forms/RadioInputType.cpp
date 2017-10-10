/*
 * Copyright (C) 2005, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "core/html/forms/RadioInputType.h"

#include "core/dom/Document.h"
#include "core/dom/ElementTraversal.h"
#include "core/events/KeyboardEvent.h"
#include "core/events/MouseEvent.h"
#include "core/html/forms/HTMLFormElement.h"
#include "core/html/forms/HTMLInputElement.h"
#include "core/html_names.h"
#include "core/input_type_names.h"
#include "core/page/SpatialNavigation.h"
#include "platform/text/PlatformLocale.h"

namespace blink {

namespace {

HTMLInputElement* NextInputElement(const HTMLInputElement& element,
                                   const HTMLFormElement* stay_within,
                                   bool forward) {
  return forward ? Traversal<HTMLInputElement>::Next(element, stay_within)
                 : Traversal<HTMLInputElement>::Previous(element, stay_within);
}

}  // namespace

using namespace HTMLNames;

InputType* RadioInputType::Create(HTMLInputElement& element) {
  return new RadioInputType(element);
}

const AtomicString& RadioInputType::FormControlType() const {
  return InputTypeNames::radio;
}

bool RadioInputType::ValueMissing(const String&) const {
  return GetElement().IsInRequiredRadioButtonGroup() &&
         !GetElement().CheckedRadioButtonForGroup();
}

String RadioInputType::ValueMissingText() const {
  return GetLocale().QueryString(
      WebLocalizedString::kValidationValueMissingForRadio);
}

void RadioInputType::HandleClickEvent(MouseEvent* event) {
  event->SetDefaultHandled();
}

HTMLInputElement* RadioInputType::FindNextFocusableRadioButtonInGroup(
    HTMLInputElement* current_element,
    bool forward) {
  for (HTMLInputElement* input_element =
           NextRadioButtonInGroup(current_element, forward);
       input_element;
       input_element = NextRadioButtonInGroup(input_element, forward)) {
    if (input_element->IsFocusable())
      return input_element;
  }
  return nullptr;
}

void RadioInputType::HandleKeydownEvent(KeyboardEvent* event) {
  // TODO(tkent): We should return more earlier.
  if (!GetElement().GetLayoutObject())
    return;
  BaseCheckableInputType::HandleKeydownEvent(event);
  if (event->DefaultHandled())
    return;
  const String& key = event->key();
  if (key != "ArrowUp" && key != "ArrowDown" && key != "ArrowLeft" &&
      key != "ArrowRight")
    return;

  if (event->ctrlKey() || event->metaKey() || event->altKey())
    return;

  // Left and up mean "previous radio button".
  // Right and down mean "next radio button".
  // Tested in WinIE, and even for RTL, left still means previous radio button
  // (and so moves to the right). Seems strange, but we'll match it. However,
  // when using Spatial Navigation, we need to be able to navigate without
  // changing the selection.
  Document& document = GetElement().GetDocument();
  if (IsSpatialNavigationEnabled(document.GetFrame()))
    return;
  bool forward = ComputedTextDirection() == TextDirection::kRtl
                     ? (key == "ArrowDown" || key == "ArrowLeft")
                     : (key == "ArrowDown" || key == "ArrowRight");

  // Force layout for isFocusable() in findNextFocusableRadioButtonInGroup().
  document.UpdateStyleAndLayoutIgnorePendingStylesheets();

  // We can only stay within the form's children if the form hasn't been demoted
  // to a leaf because of malformed HTML.
  HTMLInputElement* input_element =
      FindNextFocusableRadioButtonInGroup(&GetElement(), forward);
  if (!input_element) {
    // Traverse in reverse direction till last or first radio button
    forward = !(forward);
    HTMLInputElement* next_input_element =
        FindNextFocusableRadioButtonInGroup(&GetElement(), forward);
    while (next_input_element) {
      input_element = next_input_element;
      next_input_element =
          FindNextFocusableRadioButtonInGroup(next_input_element, forward);
    }
  }
  if (input_element) {
    document.SetFocusedElement(input_element,
                               FocusParams(SelectionBehaviorOnFocus::kRestore,
                                           kWebFocusTypeNone, nullptr));
    input_element->DispatchSimulatedClick(event, kSendNoEvents);
    event->SetDefaultHandled();
    return;
  }
}

void RadioInputType::HandleKeyupEvent(KeyboardEvent* event) {
  const String& key = event->key();
  if (key != " ")
    return;
  // If an unselected radio is tabbed into (because the entire group has nothing
  // checked, or because of some explicit .focus() call), then allow space to
  // check it.
  if (GetElement().checked())
    return;
  DispatchSimulatedClickIfActive(event);
}

bool RadioInputType::IsKeyboardFocusable() const {
  if (!InputType::IsKeyboardFocusable())
    return false;

  // When using Spatial Navigation, every radio button should be focusable.
  if (IsSpatialNavigationEnabled(GetElement().GetDocument().GetFrame()))
    return true;

  // Never allow keyboard tabbing to leave you in the same radio group. Always
  // skip any other elements in the group.
  Element* current_focused_element =
      GetElement().GetDocument().FocusedElement();
  if (auto* focused_input = ToHTMLInputElementOrNull(current_focused_element)) {
    if (focused_input->type() == InputTypeNames::radio &&
        focused_input->Form() == GetElement().Form() &&
        focused_input->GetName() == GetElement().GetName())
      return false;
  }

  // Allow keyboard focus if we're checked or if nothing in the group is
  // checked.
  return GetElement().checked() || !GetElement().CheckedRadioButtonForGroup();
}

bool RadioInputType::ShouldSendChangeEventAfterCheckedChanged() {
  // Don't send a change event for a radio button that's getting unchecked.
  // This was done to match the behavior of other browsers.
  return GetElement().checked();
}

ClickHandlingState* RadioInputType::WillDispatchClick() {
  // An event handler can use preventDefault or "return false" to reverse the
  // selection we do here.  The ClickHandlingState object contains what we need
  // to undo what we did here in didDispatchClick.

  // We want radio groups to end up in sane states, i.e., to have something
  // checked.  Therefore if nothing is currently selected, we won't allow the
  // upcoming action to be "undone", since we want some object in the radio
  // group to actually get selected.

  ClickHandlingState* state = new ClickHandlingState;

  state->checked = GetElement().checked();
  state->checked_radio_button = GetElement().CheckedRadioButtonForGroup();
  GetElement().setChecked(true, kDispatchChangeEvent);
  is_in_click_handler_ = true;
  return state;
}

void RadioInputType::DidDispatchClick(Event* event,
                                      const ClickHandlingState& state) {
  if (event->defaultPrevented() || event->DefaultHandled()) {
    // Restore the original selected radio button if possible.
    // Make sure it is still a radio button and only do the restoration if it
    // still belongs to our group.
    HTMLInputElement* checked_radio_button = state.checked_radio_button.Get();
    if (!checked_radio_button)
      GetElement().setChecked(false);
    else if (checked_radio_button->type() == InputTypeNames::radio &&
             checked_radio_button->Form() == GetElement().Form() &&
             checked_radio_button->GetName() == GetElement().GetName())
      checked_radio_button->setChecked(true);
  } else if (state.checked != GetElement().checked()) {
    GetElement().DispatchChangeEventIfNeeded();
  }
  is_in_click_handler_ = false;
  // The work we did in willDispatchClick was default handling.
  event->SetDefaultHandled();
}

bool RadioInputType::ShouldAppearIndeterminate() const {
  return !GetElement().CheckedRadioButtonForGroup();
}

HTMLInputElement* RadioInputType::NextRadioButtonInGroup(
    HTMLInputElement* current,
    bool forward) {
  // TODO(tkent): Staying within form() is incorrect.  This code ignore input
  // elements associated by |form| content attribute.
  // TODO(tkent): Comparing name() with == is incorrect.  It should be
  // case-insensitive.
  for (HTMLInputElement* input_element =
           NextInputElement(*current, current->Form(), forward);
       input_element; input_element = NextInputElement(
                          *input_element, current->Form(), forward)) {
    if (current->Form() == input_element->Form() &&
        input_element->type() == InputTypeNames::radio &&
        input_element->GetName() == current->GetName())
      return input_element;
  }
  return nullptr;
}

}  // namespace blink
