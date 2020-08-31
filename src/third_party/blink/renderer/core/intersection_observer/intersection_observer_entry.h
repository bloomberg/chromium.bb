// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_INTERSECTION_OBSERVER_INTERSECTION_OBSERVER_ENTRY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_INTERSECTION_OBSERVER_INTERSECTION_OBSERVER_ENTRY_H_

#include "third_party/blink/renderer/core/dom/dom_high_res_time_stamp.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_geometry.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class DOMRectReadOnly;
class Element;

class CORE_EXPORT IntersectionObserverEntry final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  IntersectionObserverEntry(const IntersectionGeometry& geometry,
                            DOMHighResTimeStamp timestamp,
                            Element* target);

  // IDL interface
  double time() const { return time_; }
  double intersectionRatio() const { return geometry_.IntersectionRatio(); }
  DOMRectReadOnly* boundingClientRect() const;
  DOMRectReadOnly* rootBounds() const;
  DOMRectReadOnly* intersectionRect() const;
  bool isIntersecting() const { return geometry_.IsIntersecting(); }
  bool isVisible() const { return geometry_.IsVisible(); }
  Element* target() const { return target_.Get(); }

  // blink-internal interface
  const IntersectionGeometry& GetGeometry() const { return geometry_; }
  void Trace(Visitor*) override;

 private:
  IntersectionGeometry geometry_;
  DOMHighResTimeStamp time_;
  Member<Element> target_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_INTERSECTION_OBSERVER_INTERSECTION_OBSERVER_ENTRY_H_
