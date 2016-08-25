// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RootScrollerController_h
#define RootScrollerController_h

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"

namespace blink {

class Document;
class Element;
class GraphicsLayer;
class ScrollStateCallback;
class ViewportScrollCallback;

// Manages the root scroller associated with a given document. The root scroller
// causes top controls movement, overscroll effects and prevents chaining
// scrolls up further in the DOM. It can be set from script using
// document.setRootScroller.
//
// There are two notions of a root scroller in this class: m_rootScroller and
// m_effectiveRootScroller. The former is the Element that was set as the root
// scroller using document.setRootScroller. If the page didn't set a root
// scroller this will be nullptr. The "effective" root scroller is the current
// element we're using internally to apply viewport scroll actions. i.e It's the
// element with the ViewportScrollCallback set as its apply-scroll callback.
// The effective root scroller will only be null during document initialization.
//
// If the root scroller element is a valid element to become the root scroller,
// it will be promoted to the effective root scroller. If it is not valid, the
// effective root scroller will fall back to a default Element (see
// defaultEffectiveRootScroller()). The rules for what makes an element a valid
// root scroller are set in isValidRootScroller(). The validity of the current
// root scroller is re-checked after each layout.
class CORE_EXPORT RootScrollerController
    : public GarbageCollected<RootScrollerController> {
public:
    // Creates a RootScrollerController for the given document. You should use
    // setViewportScrollCallback to provide this class with a scroll callback
    // that RootScrollerController will keep applied to the current RootScroller
    // so that special actions can occur on scrolling.
    static RootScrollerController* create(Document& document)
    {
        return new RootScrollerController(document);
    }

    DECLARE_TRACE();

    // Sets the element that will be used as the root scroller. This can be
    // nullptr, in which case we'll use the default element (documentElement) as
    // the effective root scroller.
    void set(Element*);

    // Returns the element currently set as the root scroller from script. This
    // differs from the effective root scroller since the set Element may not
    // currently be a valid root scroller. e.g. If the page sets an Element
    // with `display: none`, get() will return that element, even though the
    // effective root scroller will remain the element returned by
    // defaultEffectiveRootScroller().
    Element* get() const;

    // This returns the Element that's actually being used to control viewport
    // actions right now. This is different from get() if a root scroller hasn't
    // been set, or if the set root scroller isn't currently a valid scroller.
    Element* effectiveRootScroller() const;

    // This class needs to be informed of changes in layout so that it can
    // determine if the current root scroller is still valid or if it must be
    // replaced by the defualt root scroller.
    void didUpdateLayout();

    // This class needs to be informed of changes to compositing so that it can
    // update the compositor when the effective root scroller changes.
    void didUpdateCompositing();

    // This class needs to be informed when the document has been attached to a
    // FrameView so that we can initialize the viewport scroll callback.
    void didAttachDocument();

    GraphicsLayer* rootScrollerLayer();

    // TODO(bokan): Temporarily needed to allow ScrollCustomization to
    // differentiate between real custom callback and the built-in viewport
    // apply scroll.
    bool isViewportScrollCallback(const ScrollStateCallback*) const;

private:
    RootScrollerController(Document&);

    Element* defaultEffectiveRootScroller();

    // Ensures the effective root scroller is currently valid and replaces it
    // with the default if not.
    void updateEffectiveRootScroller();

    // Called only from the top Document's RootScrollerController. Ensures that
    // the element that should be used as the root scroller on the page has the
    // m_viewportApplyScroll callback set on it.
    void setViewportApplyScrollOnRootScroller();

    WeakMember<Document> m_document;
    Member<ViewportScrollCallback> m_viewportApplyScroll;

    WeakMember<Element> m_rootScroller;

    // The element currently being used as the root scroller. If
    // m_viewportApplyScroll has been set, this element is guaranteed to have it
    // set as its applyScroll callback. This can be nullptr during
    // initialization and will not be set until m_viewportApplyScroll is
    // provided.
    WeakMember<Element> m_effectiveRootScroller;

    // Tracks which element currently has the m_viewportApplyScroll set to it.
    // This will only ever be set on the top Document's RootScrollerController.
    WeakMember<Element> m_currentViewportApplyScrollHost;
};

} // namespace blink

#endif // RootScrollerController_h
