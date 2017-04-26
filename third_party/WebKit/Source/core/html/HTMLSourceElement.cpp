/*
 * Copyright (C) 2007, 2008, 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/html/HTMLSourceElement.h"

#include "core/HTMLNames.h"
#include "core/css/MediaList.h"
#include "core/css/MediaQueryList.h"
#include "core/css/MediaQueryMatcher.h"
#include "core/dom/Document.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/events/Event.h"
#include "core/html/HTMLMediaElement.h"
#include "core/html/HTMLPictureElement.h"

#define SOURCE_LOG_LEVEL 3

namespace blink {

using namespace HTMLNames;

class HTMLSourceElement::Listener final : public MediaQueryListListener {
 public:
  explicit Listener(HTMLSourceElement* element) : element_(element) {}
  void NotifyMediaQueryChanged() override {
    if (element_)
      element_->NotifyMediaQueryChanged();
  }

  void ClearElement() { element_ = nullptr; }
  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->Trace(element_);
    MediaQueryListListener::Trace(visitor);
  }

 private:
  Member<HTMLSourceElement> element_;
};

inline HTMLSourceElement::HTMLSourceElement(Document& document)
    : HTMLElement(sourceTag, document), listener_(new Listener(this)) {
  DVLOG(SOURCE_LOG_LEVEL) << "HTMLSourceElement - " << (void*)this;
}

DEFINE_NODE_FACTORY(HTMLSourceElement)

HTMLSourceElement::~HTMLSourceElement() {}

void HTMLSourceElement::CreateMediaQueryList(const AtomicString& media) {
  RemoveMediaQueryListListener();
  if (media.IsEmpty()) {
    media_query_list_ = nullptr;
    return;
  }

  RefPtr<MediaQuerySet> set = MediaQuerySet::Create(media);
  media_query_list_ = MediaQueryList::Create(
      &GetDocument(), &GetDocument().GetMediaQueryMatcher(), set);
  AddMediaQueryListListener();
}

void HTMLSourceElement::DidMoveToNewDocument(Document& old_document) {
  CreateMediaQueryList(FastGetAttribute(mediaAttr));
  HTMLElement::DidMoveToNewDocument(old_document);
}

Node::InsertionNotificationRequest HTMLSourceElement::InsertedInto(
    ContainerNode* insertion_point) {
  HTMLElement::InsertedInto(insertion_point);
  Element* parent = parentElement();
  if (IsHTMLMediaElement(parent))
    ToHTMLMediaElement(parent)->SourceWasAdded(this);
  if (isHTMLPictureElement(parent))
    toHTMLPictureElement(parent)->SourceOrMediaChanged();
  return kInsertionDone;
}

void HTMLSourceElement::RemovedFrom(ContainerNode* removal_root) {
  Element* parent = parentElement();
  if (!parent && removal_root->IsElementNode())
    parent = ToElement(removal_root);
  if (IsHTMLMediaElement(parent))
    ToHTMLMediaElement(parent)->SourceWasRemoved(this);
  if (isHTMLPictureElement(parent)) {
    RemoveMediaQueryListListener();
    toHTMLPictureElement(parent)->SourceOrMediaChanged();
  }
  HTMLElement::RemovedFrom(removal_root);
}

void HTMLSourceElement::RemoveMediaQueryListListener() {
  if (media_query_list_)
    media_query_list_->RemoveListener(listener_);
}

void HTMLSourceElement::AddMediaQueryListListener() {
  if (media_query_list_)
    media_query_list_->AddListener(listener_);
}

void HTMLSourceElement::SetSrc(const String& url) {
  setAttribute(srcAttr, AtomicString(url));
}

const AtomicString& HTMLSourceElement::type() const {
  return getAttribute(typeAttr);
}

void HTMLSourceElement::setType(const AtomicString& type) {
  setAttribute(typeAttr, type);
}

void HTMLSourceElement::ScheduleErrorEvent() {
  DVLOG(SOURCE_LOG_LEVEL) << "scheduleErrorEvent - " << (void*)this;

  pending_error_event_ =
      TaskRunnerHelper::Get(TaskType::kDOMManipulation, &GetDocument())
          ->PostCancellableTask(
              BLINK_FROM_HERE,
              WTF::Bind(&HTMLSourceElement::DispatchPendingEvent,
                        WrapPersistent(this)));
}

void HTMLSourceElement::CancelPendingErrorEvent() {
  DVLOG(SOURCE_LOG_LEVEL) << "cancelPendingErrorEvent - " << (void*)this;
  pending_error_event_.Cancel();
}

void HTMLSourceElement::DispatchPendingEvent() {
  DVLOG(SOURCE_LOG_LEVEL) << "dispatchPendingEvent - " << (void*)this;
  DispatchEvent(Event::CreateCancelable(EventTypeNames::error));
}

bool HTMLSourceElement::MediaQueryMatches() const {
  if (!media_query_list_)
    return true;

  return media_query_list_->matches();
}

bool HTMLSourceElement::IsURLAttribute(const Attribute& attribute) const {
  return attribute.GetName() == srcAttr ||
         HTMLElement::IsURLAttribute(attribute);
}

void HTMLSourceElement::ParseAttribute(
    const AttributeModificationParams& params) {
  HTMLElement::ParseAttribute(params);
  const QualifiedName& name = params.name;
  if (name == mediaAttr)
    CreateMediaQueryList(params.new_value);
  if (name == srcsetAttr || name == sizesAttr || name == mediaAttr ||
      name == typeAttr) {
    Element* parent = parentElement();
    if (isHTMLPictureElement(parent))
      toHTMLPictureElement(parent)->SourceOrMediaChanged();
  }
}

void HTMLSourceElement::NotifyMediaQueryChanged() {
  Element* parent = parentElement();
  if (isHTMLPictureElement(parent))
    toHTMLPictureElement(parent)->SourceOrMediaChanged();
}

DEFINE_TRACE(HTMLSourceElement) {
  visitor->Trace(media_query_list_);
  visitor->Trace(listener_);
  HTMLElement::Trace(visitor);
}

}  // namespace blink
