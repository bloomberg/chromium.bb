// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_LAZY_LOAD_IMAGE_OBSERVER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_LAZY_LOAD_IMAGE_OBSERVER_H_

#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class Document;
class Element;
class IntersectionObserver;
class IntersectionObserverEntry;
class Visitor;

class LazyLoadImageObserver final
    : public GarbageCollected<LazyLoadImageObserver> {
 public:
  explicit LazyLoadImageObserver(Document&);

  static void StartMonitoring(Element*);
  static void StopMonitoring(Element*);

  void Trace(Visitor*);

 private:
  // TODO(rajendrant): Make the root margins configurable via field trial params
  // instead of just hardcoding the value here.
  static constexpr int kLazyLoadRootMarginPx = 800;

  void LoadIfNearViewport(const HeapVector<Member<IntersectionObserverEntry>>&);

  // The intersection observer responsible for loading the image once it's near
  // the viewport.
  Member<IntersectionObserver> lazy_load_intersection_observer_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_LAZY_LOAD_IMAGE_OBSERVER_H_
