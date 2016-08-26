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
// level Document on a page. In addition to the regular RootScroller duties
// such as keeping track of which Element is set as root scroller and which is
// the effective root scroller, this class is also responsible for setting the
// ViewportApplyScroll on the one Element on a page that should apply viewport
// scrolling actions.
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

    // Returns true if the given ScrollStateCallback is the ViewportScrollCallback managed
    // by this class.
    // TODO(bokan): Temporarily needed to allow ScrollCustomization to
    // differentiate between real custom callback and the built-in viewport
    // apply scroll. crbug.com/623079.
    bool isViewportScrollCallback(
        const ScrollStateCallback*) const override;

protected:
    TopDocumentRootScrollerController(Document&);

    // Ensures the effective root scroller is currently valid and replaces it
    // with the default if not.
    void updateEffectiveRootScroller() override;

private:
    // Ensures that the element that should be used as the root scroller on the
    // page has the m_viewportApplyScroll callback set on it.
    void setViewportApplyScrollOnRootScroller();

    // The apply-scroll callback that moves top controls and produces
    // overscroll effects. This class makes sure this callback is set on the
    // appropriate root scroller element.
    Member<ViewportScrollCallback> m_viewportApplyScroll;

    // Tracks which element currently has the m_viewportApplyScroll set to it.
    WeakMember<Element> m_currentViewportApplyScrollHost;
};

} // namespace blink

#endif // RootScrollerController_h
