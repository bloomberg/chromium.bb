// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ResizeObserverEntry_h
#define ResizeObserverEntry_h

#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"

namespace blink {

class Element;
class DOMRectReadOnly;
class LayoutRect;

class ResizeObserverEntry final : public GarbageCollected<ResizeObserverEntry>,
                                  public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  ResizeObserverEntry(Element* target, const LayoutRect& content_rect);

  Element* target() const { return target_; }
  DOMRectReadOnly* contentRect() const { return content_rect_; }

  virtual void Trace(blink::Visitor*);

 private:
  Member<Element> target_;
  Member<DOMRectReadOnly> content_rect_;
};

}  // namespace blink

#endif  // ResizeObserverEntry_h
