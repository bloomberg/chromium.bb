/*
 * Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies)
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

#include "core/html/HTMLSummaryElement.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/FlatTreeTraversal.h"
#include "core/dom/ShadowRoot.h"
#include "core/events/KeyboardEvent.h"
#include "core/html/HTMLContentElement.h"
#include "core/html/HTMLDetailsElement.h"
#include "core/html/shadow/DetailsMarkerControl.h"
#include "core/html/shadow/ShadowElementNames.h"
#include "core/html_names.h"
#include "core/layout/LayoutBlockFlow.h"

namespace blink {

using namespace HTMLNames;

HTMLSummaryElement* HTMLSummaryElement::Create(Document& document) {
  HTMLSummaryElement* summary = new HTMLSummaryElement(document);
  summary->EnsureUserAgentShadowRoot();
  return summary;
}

HTMLSummaryElement::HTMLSummaryElement(Document& document)
    : HTMLElement(summaryTag, document) {}

LayoutObject* HTMLSummaryElement::CreateLayoutObject(
    const ComputedStyle& style) {
  EDisplay display = style.Display();
  if (display == EDisplay::kFlex || display == EDisplay::kInlineFlex ||
      display == EDisplay::kGrid || display == EDisplay::kInlineGrid)
    return LayoutObject::CreateObject(this, style);
  return new LayoutBlockFlow(this);
}

void HTMLSummaryElement::DidAddUserAgentShadowRoot(ShadowRoot& root) {
  DetailsMarkerControl* marker_control =
      DetailsMarkerControl::Create(GetDocument());
  marker_control->SetIdAttribute(ShadowElementNames::DetailsMarker());
  root.AppendChild(marker_control);
  root.AppendChild(HTMLContentElement::Create(GetDocument()));
}

HTMLDetailsElement* HTMLSummaryElement::DetailsElement() const {
  if (auto* details = ToHTMLDetailsElementOrNull(parentNode()))
    return details;
  if (auto* details = ToHTMLDetailsElementOrNull(OwnerShadowHost()))
    return details;
  return nullptr;
}

Element* HTMLSummaryElement::MarkerControl() {
  return EnsureUserAgentShadowRoot().getElementById(
      ShadowElementNames::DetailsMarker());
}

bool HTMLSummaryElement::IsMainSummary() const {
  if (HTMLDetailsElement* details = DetailsElement())
    return details->FindMainSummary() == this;

  return false;
}

static bool IsClickableControl(Node* node) {
  if (!node->IsElementNode())
    return false;
  Element* element = ToElement(node);
  if (element->IsFormControlElement())
    return true;
  Element* host = element->OwnerShadowHost();
  return host && host->IsFormControlElement();
}

bool HTMLSummaryElement::SupportsFocus() const {
  return IsMainSummary() || HTMLElement::SupportsFocus();
}

void HTMLSummaryElement::DefaultEventHandler(Event* event) {
  if (IsMainSummary()) {
    if (event->type() == EventTypeNames::DOMActivate &&
        !IsClickableControl(event->target()->ToNode())) {
      if (HTMLDetailsElement* details = DetailsElement())
        details->ToggleOpen();
      event->SetDefaultHandled();
      return;
    }

    if (event->IsKeyboardEvent()) {
      if (event->type() == EventTypeNames::keydown &&
          ToKeyboardEvent(event)->key() == " ") {
        SetActive(true);
        // No setDefaultHandled() - IE dispatches a keypress in this case.
        return;
      }
      if (event->type() == EventTypeNames::keypress) {
        switch (ToKeyboardEvent(event)->charCode()) {
          case '\r':
            DispatchSimulatedClick(event);
            event->SetDefaultHandled();
            return;
          case ' ':
            // Prevent scrolling down the page.
            event->SetDefaultHandled();
            return;
        }
      }
      if (event->type() == EventTypeNames::keyup &&
          ToKeyboardEvent(event)->key() == " ") {
        if (IsActive())
          DispatchSimulatedClick(event);
        event->SetDefaultHandled();
        return;
      }
    }
  }

  HTMLElement::DefaultEventHandler(event);
}

bool HTMLSummaryElement::HasActivationBehavior() const {
  return true;
}

bool HTMLSummaryElement::WillRespondToMouseClickEvents() {
  return IsMainSummary() || HTMLElement::WillRespondToMouseClickEvents();
}

}  // namespace blink
