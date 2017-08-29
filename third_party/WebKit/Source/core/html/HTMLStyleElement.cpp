/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2010 Apple Inc. All rights reserved.
 *           (C) 2007 Rob Buis (buis@kde.org)
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
 */

#include "core/html/HTMLStyleElement.h"

#include "core/HTMLNames.h"
#include "core/css/MediaList.h"
#include "core/dom/Document.h"
#include "core/dom/ShadowRoot.h"
#include "core/dom/StyleEngine.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/dom/events/Event.h"

namespace blink {

using namespace HTMLNames;

inline HTMLStyleElement::HTMLStyleElement(Document& document,
                                          bool created_by_parser)
    : HTMLElement(styleTag, document),
      StyleElement(&document, created_by_parser),
      fired_load_(false),
      loaded_sheet_(false) {}

HTMLStyleElement::~HTMLStyleElement() {}

HTMLStyleElement* HTMLStyleElement::Create(Document& document,
                                           bool created_by_parser) {
  return new HTMLStyleElement(document, created_by_parser);
}

void HTMLStyleElement::ParseAttribute(
    const AttributeModificationParams& params) {
  if (params.name == titleAttr && sheet_ && IsInDocumentTree()) {
    sheet_->SetTitle(params.new_value);
  } else if (params.name == mediaAttr && isConnected() &&
             GetDocument().IsActive() && sheet_) {
    sheet_->SetMediaQueries(MediaQuerySet::Create(params.new_value));
    GetDocument().GetStyleEngine().MediaQueriesChangedInScope(GetTreeScope());
  } else {
    HTMLElement::ParseAttribute(params);
  }
}

void HTMLStyleElement::FinishParsingChildren() {
  StyleElement::ProcessingResult result =
      StyleElement::FinishParsingChildren(*this);
  HTMLElement::FinishParsingChildren();
  if (result == StyleElement::kProcessingFatalError)
    NotifyLoadedSheetAndAllCriticalSubresources(
        kErrorOccurredLoadingSubresource);
}

Node::InsertionNotificationRequest HTMLStyleElement::InsertedInto(
    ContainerNode* insertion_point) {
  HTMLElement::InsertedInto(insertion_point);
  return kInsertionShouldCallDidNotifySubtreeInsertions;
}

void HTMLStyleElement::RemovedFrom(ContainerNode* insertion_point) {
  HTMLElement::RemovedFrom(insertion_point);
  StyleElement::RemovedFrom(*this, insertion_point);
}

void HTMLStyleElement::DidNotifySubtreeInsertionsToDocument() {
  if (StyleElement::ProcessStyleSheet(GetDocument(), *this) ==
      StyleElement::kProcessingFatalError)
    NotifyLoadedSheetAndAllCriticalSubresources(
        kErrorOccurredLoadingSubresource);
}

void HTMLStyleElement::ChildrenChanged(const ChildrenChange& change) {
  HTMLElement::ChildrenChanged(change);
  if (StyleElement::ChildrenChanged(*this) ==
      StyleElement::kProcessingFatalError)
    NotifyLoadedSheetAndAllCriticalSubresources(
        kErrorOccurredLoadingSubresource);
}

const AtomicString& HTMLStyleElement::media() const {
  return getAttribute(mediaAttr);
}

const AtomicString& HTMLStyleElement::type() const {
  return getAttribute(typeAttr);
}

void HTMLStyleElement::DispatchPendingEvent(
    std::unique_ptr<IncrementLoadEventDelayCount> count) {
  if (loaded_sheet_) {
    if (GetDocument().HasListenerType(
            Document::kLoadListenerAtCapturePhaseOrAtStyleElement))
      DispatchEvent(Event::Create(EventTypeNames::load));
  } else {
    DispatchEvent(Event::Create(EventTypeNames::error));
  }
  // Checks Document's load event synchronously here for performance.
  // This is safe because dispatchPendingEvent() is called asynchronously.
  count->ClearAndCheckLoadEvent();
}

void HTMLStyleElement::NotifyLoadedSheetAndAllCriticalSubresources(
    LoadedSheetErrorStatus error_status) {
  bool is_load_event = error_status == kNoErrorLoadingSubresource;
  if (fired_load_ && is_load_event)
    return;
  loaded_sheet_ = is_load_event;
  TaskRunnerHelper::Get(TaskType::kDOMManipulation, &GetDocument())
      ->PostTask(BLINK_FROM_HERE,
                 WTF::Bind(&HTMLStyleElement::DispatchPendingEvent,
                           WrapPersistent(this),
                           WTF::Passed(IncrementLoadEventDelayCount::Create(
                               GetDocument()))));
  fired_load_ = true;
}

bool HTMLStyleElement::disabled() const {
  if (!sheet_)
    return false;

  return sheet_->disabled();
}

void HTMLStyleElement::setDisabled(bool set_disabled) {
  if (CSSStyleSheet* style_sheet = sheet())
    style_sheet->setDisabled(set_disabled);
}

DEFINE_TRACE(HTMLStyleElement) {
  StyleElement::Trace(visitor);
  HTMLElement::Trace(visitor);
}

}  // namespace blink
