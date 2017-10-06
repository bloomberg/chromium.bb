/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
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

#include "core/html/HTMLFormControlElementWithState.h"

#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameClient.h"
#include "core/html/forms/FormController.h"
#include "core/html/forms/HTMLFormElement.h"
#include "core/page/ChromeClient.h"

namespace blink {

HTMLFormControlElementWithState::HTMLFormControlElementWithState(
    const QualifiedName& tag_name,
    Document& doc)
    : HTMLFormControlElement(tag_name, doc) {}

HTMLFormControlElementWithState::~HTMLFormControlElementWithState() {}

Node::InsertionNotificationRequest
HTMLFormControlElementWithState::InsertedInto(ContainerNode* insertion_point) {
  if (insertion_point->isConnected() && !ContainingShadowRoot())
    GetDocument().GetFormController().RegisterStatefulFormControl(*this);
  return HTMLFormControlElement::InsertedInto(insertion_point);
}

void HTMLFormControlElementWithState::RemovedFrom(
    ContainerNode* insertion_point) {
  if (insertion_point->isConnected() && !ContainingShadowRoot() &&
      !insertion_point->ContainingShadowRoot())
    GetDocument().GetFormController().UnregisterStatefulFormControl(*this);
  HTMLFormControlElement::RemovedFrom(insertion_point);
}

bool HTMLFormControlElementWithState::ShouldAutocomplete() const {
  if (!Form())
    return true;
  return Form()->ShouldAutocomplete();
}

void HTMLFormControlElementWithState::NotifyFormStateChanged() {
  // This can be called during fragment parsing as a result of option
  // selection before the document is active (or even in a frame).
  if (!GetDocument().IsActive())
    return;
  GetDocument().GetFrame()->Client()->DidUpdateCurrentHistoryItem();
}

bool HTMLFormControlElementWithState::ShouldSaveAndRestoreFormControlState()
    const {
  // We don't save/restore control state in a form with autocomplete=off.
  return isConnected() && ShouldAutocomplete();
}

FormControlState HTMLFormControlElementWithState::SaveFormControlState() const {
  return FormControlState();
}

void HTMLFormControlElementWithState::FinishParsingChildren() {
  HTMLFormControlElement::FinishParsingChildren();
  GetDocument().GetFormController().RestoreControlStateFor(*this);
}

bool HTMLFormControlElementWithState::IsFormControlElementWithState() const {
  return true;
}

}  // namespace blink
