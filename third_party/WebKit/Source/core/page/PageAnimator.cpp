// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/page/PageAnimator.h"

#include "core/animation/DocumentAnimations.h"
#include "core/dom/Document.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/page/Chrome.h"
#include "core/page/ChromeClient.h"
#include "core/page/Page.h"

namespace WebCore {

PageAnimator::PageAnimator(Page* page)
    : m_page(page)
    , m_animationFramePending(false)
    , m_servicingAnimations(false)
{
}

void PageAnimator::serviceScriptedAnimations(double monotonicAnimationStartTime)
{
    m_animationFramePending = false;
    TemporaryChange<bool> servicing(m_servicingAnimations, true);

    for (RefPtr<LocalFrame> frame = m_page->mainFrame(); frame; frame = frame->tree().traverseNext()) {
        frame->view()->serviceScrollAnimations();
        DocumentAnimations::updateAnimationTimingForAnimationFrame(*frame->document(), monotonicAnimationStartTime);
    }

    Vector<RefPtr<Document> > documents;
    for (LocalFrame* frame = m_page->mainFrame(); frame; frame = frame->tree().traverseNext())
        documents.append(frame->document());

    for (size_t i = 0; i < documents.size(); ++i)
        documents[i]->serviceScriptedAnimations(monotonicAnimationStartTime);
}

void PageAnimator::scheduleVisualUpdate()
{
    if (m_animationFramePending || m_servicingAnimations)
        return;
    m_page->chrome().scheduleAnimation();
    ASSERT(m_animationFramePending);
}

}
