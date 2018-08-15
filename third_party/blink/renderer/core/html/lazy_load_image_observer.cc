// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/lazy_load_image_observer.h"

#include "build/build_config.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html_element_type_helpers.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer_entry.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

void LazyLoadImageObserver::StartMonitoring(Element* element) {
  if (LocalFrame* frame = element->GetDocument().GetFrame()) {
    if (Document* document = frame->LocalFrameRoot().GetDocument()) {
      document->EnsureLazyLoadImageObserver()
          .lazy_load_intersection_observer_->observe(element);
    }
  }
}

void LazyLoadImageObserver::StopMonitoring(Element* element) {
  if (LocalFrame* frame = element->GetDocument().GetFrame()) {
    if (Document* document = frame->LocalFrameRoot().GetDocument()) {
      document->EnsureLazyLoadImageObserver()
          .lazy_load_intersection_observer_->unobserve(element);
    }
  }
}

LazyLoadImageObserver::LazyLoadImageObserver(Document& document) {
  DCHECK(RuntimeEnabledFeatures::LazyImageLoadingEnabled());
  document.AddConsoleMessage(ConsoleMessage::Create(
      kInterventionMessageSource, kInfoMessageLevel,
      "Images loaded lazily and replaced with placeholders. Load events are "
      "deferred. See https://crbug.com/846170"));
  lazy_load_intersection_observer_ = IntersectionObserver::Create(
      {Length(kLazyLoadRootMarginPx, kFixed)},
      {std::numeric_limits<float>::min()}, &document,
      WTF::BindRepeating(&LazyLoadImageObserver::LoadIfNearViewport,
                         WrapWeakPersistent(this)));
}

void LazyLoadImageObserver::LoadIfNearViewport(
    const HeapVector<Member<IntersectionObserverEntry>>& entries) {
  DCHECK(!entries.IsEmpty());

  for (auto entry : entries) {
    if (!entry->isIntersecting())
      continue;
    Element* element = entry->target();
    if (auto* image_element = ToHTMLImageElementOrNull(element))
      image_element->LoadDeferredImage();

    lazy_load_intersection_observer_->unobserve(element);
  }
}

void LazyLoadImageObserver::Trace(Visitor* visitor) {
  visitor->Trace(lazy_load_intersection_observer_);
}

}  // namespace blink
