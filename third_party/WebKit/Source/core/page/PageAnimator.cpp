// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/page/PageAnimator.h"

#include "core/animation/DocumentAnimations.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/page/ChromeClient.h"
#include "core/page/Page.h"
#include "core/svg/SVGDocumentExtensions.h"
#include "platform/Logging.h"

namespace blink {

PageAnimator::PageAnimator(Page& page)
    : m_page(page)
    , m_servicingAnimations(false)
    , m_updatingLayoutAndStyleForPainting(false)
{
}

PassRefPtrWillBeRawPtr<PageAnimator> PageAnimator::create(Page& page)
{
    return adoptRefWillBeNoop(new PageAnimator(page));
}

DEFINE_TRACE(PageAnimator)
{
    visitor->trace(m_page);
}

void PageAnimator::serviceScriptedAnimations(double monotonicAnimationStartTime)
{
    RefPtrWillBeRawPtr<PageAnimator> protector(this);
    TemporaryChange<bool> servicing(m_servicingAnimations, true);
    clock().updateTime(monotonicAnimationStartTime);

    WillBeHeapVector<RefPtrWillBeMember<Document>> documents;
    for (Frame* frame = m_page->mainFrame(); frame; frame = frame->tree().traverseNext()) {
        if (frame->isLocalFrame())
            documents.append(toLocalFrame(frame)->document());
    }

    for (auto& document : documents) {
        DocumentAnimations::updateAnimationTimingForAnimationFrame(*document);
        if (document->view()) {
            if (document->view()->shouldThrottleRendering())
                continue;
            document->view()->scrollableArea()->serviceScrollAnimations(monotonicAnimationStartTime);

            if (const FrameView::ScrollableAreaSet* animatingScrollableAreas = document->view()->animatingScrollableAreas()) {
                // Iterate over a copy, since ScrollableAreas may deregister
                // themselves during the iteration.
                WillBeHeapVector<RawPtrWillBeMember<ScrollableArea>> animatingScrollableAreasCopy;
                copyToVector(*animatingScrollableAreas, animatingScrollableAreasCopy);
                for (ScrollableArea* scrollableArea : animatingScrollableAreasCopy)
                    scrollableArea->serviceScrollAnimations(monotonicAnimationStartTime);
            }
        }
        // TODO(skyostil): These functions should not run for documents without views.
        SVGDocumentExtensions::serviceOnAnimationFrame(*document, monotonicAnimationStartTime);
        document->serviceScriptedAnimations(monotonicAnimationStartTime);
    }

#if ENABLE(OILPAN)
    // TODO(esprehn): Why is this here? It doesn't make sense to explicitly
    // clear a stack allocated vector.
    documents.clear();
#endif
}

void PageAnimator::scheduleVisualUpdate(LocalFrame* frame)
{
    if (m_servicingAnimations || m_updatingLayoutAndStyleForPainting)
        return;
    m_page->chromeClient().scheduleAnimation(frame->view());
}

void PageAnimator::updateAllLifecyclePhases(LocalFrame& rootFrame)
{
    RefPtrWillBeRawPtr<FrameView> view = rootFrame.view();
    TemporaryChange<bool> servicing(m_updatingLayoutAndStyleForPainting, true);
    view->updateAllLifecyclePhases();
}

} // namespace blink
