// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IntersectionObserverEntry_h
#define IntersectionObserverEntry_h

#include "core/dom/ClientRect.h"
#include "core/dom/DOMHighResTimeStamp.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/geometry/IntRect.h"
#include "platform/heap/Handle.h"

namespace blink {

class Element;

class IntersectionObserverEntry final
    : public GarbageCollected<IntersectionObserverEntry>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  IntersectionObserverEntry(DOMHighResTimeStamp timestamp,
                            double intersection_ratio,
                            const IntRect& bounding_client_rect,
                            const IntRect* root_bounds,
                            const IntRect& intersection_rect,
                            bool is_intersecting,
                            Element*);

  double time() const { return time_; }
  double intersectionRatio() const { return intersection_ratio_; }
  ClientRect* boundingClientRect() const { return bounding_client_rect_; }
  ClientRect* rootBounds() const { return root_bounds_; }
  ClientRect* intersectionRect() const { return intersection_rect_; }
  bool isIntersecting() const { return is_intersecting_; }
  Element* target() const { return target_.Get(); }

  DECLARE_VIRTUAL_TRACE();

 private:
  DOMHighResTimeStamp time_;
  double intersection_ratio_;
  Member<ClientRect> bounding_client_rect_;
  Member<ClientRect> root_bounds_;
  Member<ClientRect> intersection_rect_;
  Member<Element> target_;
  bool is_intersecting_;
};

}  // namespace blink

#endif  // IntersectionObserverEntry_h
