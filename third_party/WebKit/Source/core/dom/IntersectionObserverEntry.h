// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IntersectionObserverEntry_h
#define IntersectionObserverEntry_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/dom/ClientRect.h"
#include "core/dom/DOMHighResTimeStamp.h"
#include "platform/geometry/IntRect.h"
#include "platform/heap/Handle.h"

namespace blink {

class Element;

class IntersectionObserverEntry final : public GarbageCollectedFinalized<IntersectionObserverEntry>, public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:
    IntersectionObserverEntry(DOMHighResTimeStamp timestamp, const IntRect& boundingClientRect, const IntRect* rootBounds, const IntRect& intersectionRect, Element*);
    ~IntersectionObserverEntry();

    double time() const { return m_time; }
    ClientRect* boundingClientRect() const { return m_boundingClientRect; }
    ClientRect* rootBounds() const { return m_rootBounds; }
    ClientRect* intersectionRect() const { return m_intersectionRect; }
    Element* target() const { return m_target.get(); }

    DECLARE_VIRTUAL_TRACE();

private:
    DOMHighResTimeStamp m_time;
    Member<ClientRect> m_boundingClientRect;
    Member<ClientRect> m_rootBounds;
    Member<ClientRect> m_intersectionRect;
    RefPtrWillBeMember<Element> m_target;
};

} // namespace blink

#endif // IntersectionObserverEntry_h
