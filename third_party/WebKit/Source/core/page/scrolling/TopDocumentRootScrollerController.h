// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TopDocumentRootScrollerController_h
#define TopDocumentRootScrollerController_h

#include "core/CoreExport.h"
#include "core/page/scrolling/RootScrollerController.h"
#include "platform/heap/Handle.h"

namespace blink {

class Document;
class Element;
class GraphicsLayer;
class ScrollStateCallback;
class ViewportScrollCallback;

// The RootScrollerController used to manage the root scroller for the top
// level Document on a page. In addition to the regular RootScroller duties,
// such as keeping track of which Element is set as root scroller and which is
// the effective root scroller, this class is also manages the "global" root
// scroller. That is, given all the iframes on a page and their individual root
// scrollers, this class will determine which ultimate Element should be used
// as the root scroller and ensures that Element is used to scroll top controls
// and provide overscroll effects.
// TODO(bokan): This class is currently OOPIF unaware. It should be broken into
// a standalone class and placed on a Page level object. crbug.com/642378.
class CORE_EXPORT TopDocumentRootScrollerController
    : public RootScrollerController {
public:
    static TopDocumentRootScrollerController* create(Document&);

    DECLARE_VIRTUAL_TRACE();

    // This class needs to be informed of changes to compositing so that it can
    // update the compositor when the effective root scroller changes.
    void didUpdateCompositing() override;

    // This class needs to be informed when the document has been attached to a
    // FrameView so that we can initialize the viewport scroll callback.
    void didAttachDocument() override;

    // Returns true if the given ScrollStateCallback is the
    // ViewportScrollCallback managed by this class.
    // TODO(bokan): Temporarily needed to allow ScrollCustomization to
    // differentiate between real custom callback and the built-in viewport
    // apply scroll. crbug.com/623079.
    bool isViewportScrollCallback(
        const ScrollStateCallback*) const override;

    // Returns the GraphicsLayer for the global root scroller.
    GraphicsLayer* rootScrollerLayer() override;

    // Returns the Element that's the global root scroller.
    Element* globalRootScroller() const;

protected:
    TopDocumentRootScrollerController(Document&);

    // Called when the root scroller of descendant frames changes.
    void globalRootScrollerMayHaveChanged() override;

private:
    // Calculates the Element that should be the globalRootScroller. On a
    // simple page, this will simply the root frame's effectiveRootScroller but
    // if the root scroller is set to an iframe, this will then descend into
    // the iframe to find its effective root scroller.
    Element* findGlobalRootScrollerElement();

    // Should be called to recalculate the global root scroller and ensure all
    // appropriate state changes are made if it changes.
    void updateGlobalRootScroller();

    void setNeedsCompositingInputsUpdateOnGlobalRootScroller();

    // The apply-scroll callback that moves top controls and produces
    // overscroll effects. This class makes sure this callback is set on the
    // appropriate root scroller element.
    Member<ViewportScrollCallback> m_viewportApplyScroll;

    // The page level root scroller. i.e. The actual element for which scrolling
    // should move top controls and produce overscroll glow.
    WeakMember<Element> m_globalRootScroller;
};

} // namespace blink

#endif // RootScrollerController_h
