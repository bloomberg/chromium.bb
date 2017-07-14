// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/ElementVisibilityObserver.h"

#include "core/dom/Element.h"
#include "core/frame/LocalFrame.h"
#include "core/intersection_observer/IntersectionObserverEntry.h"
#include "platform/wtf/Functional.h"

namespace blink {

ElementVisibilityObserver::ElementVisibilityObserver(
    Element* element,
    std::unique_ptr<VisibilityCallback> callback)
    : element_(element), callback_(std::move(callback)) {}

ElementVisibilityObserver::~ElementVisibilityObserver() = default;

void ElementVisibilityObserver::Start(float threshold) {
  DCHECK(!intersection_observer_);

  ExecutionContext* context = element_->GetExecutionContext();
  DCHECK(context->IsDocument());
  Document& document = ToDocument(*context);

  intersection_observer_ = IntersectionObserver::Create(
      {} /* root_margin */, {threshold}, &document,
      WTF::Bind(&ElementVisibilityObserver::OnVisibilityChanged,
                WrapWeakPersistent(this)));
  DCHECK(intersection_observer_);

  intersection_observer_->observe(element_.Release());
}

void ElementVisibilityObserver::Stop() {
  DCHECK(intersection_observer_);

  intersection_observer_->disconnect();
  intersection_observer_ = nullptr;
}

void ElementVisibilityObserver::DeliverObservationsForTesting() {
  intersection_observer_->Deliver();
}

DEFINE_TRACE(ElementVisibilityObserver) {
  visitor->Trace(element_);
  visitor->Trace(intersection_observer_);
}

void ElementVisibilityObserver::OnVisibilityChanged(
    const HeapVector<Member<IntersectionObserverEntry>>& entries) {
  bool is_visible = entries.back()->intersectionRatio() >=
                    intersection_observer_->thresholds()[0];
  (*callback_.get())(is_visible);
}

}  // namespace blink
