// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RootScroller_h
#define RootScroller_h

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"

namespace blink {

class Element;
class FrameHost;
class ViewportScrollCallback;

// This class manages the root scroller associated with the top-level document.
// The root scroller causes top controls movement, overscroll effects and
// prevents chaining up further in the DOM. It can be set using
// document.setRootScroller. By default, the rootScroller is the
// documentElement.
//
// Not all elements can be set as the root scroller. A valid element must be
// in the top-level document and must have a LayoutObject that is at least a
// LayoutBlockFlow or LayoutIFrame. In addition, only the top-level document can
// set a non-default root scroller.
class CORE_EXPORT RootScroller : public GarbageCollected<RootScroller> {
public:
    static RootScroller* create(FrameHost& frameHost)
    {
        return new RootScroller(frameHost);
    }

    // This method returns true if the element was set as the root scroller. If
    // the element isn't eligible to be the root scroller or we're in some bad
    // state, the method returns false without changing the current root
    // scroller.
    bool set(Element&);

    // Returns the element currently set as the root scroller.
    Element* get() const;

    // This class needs to be informed of changes in layout so that it can
    // determine if the current scroller is still valid or if it must be
    // replaced by the defualt root scroller.
    void didUpdateTopDocumentLayout();

    DECLARE_TRACE();

private:
    RootScroller(FrameHost&);

    bool isValid(Element&) const;
    void resetToDefault();
    void createApplyScrollIfNeeded();

    WeakMember<FrameHost> m_frameHost;
    Member<ViewportScrollCallback> m_viewportApplyScroll;

    WeakMember<Element> m_rootScroller;
};

} // namespace blink

#endif // RootScroller_h
