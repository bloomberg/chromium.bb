// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/page/PageAnimator.h"

#include "core/animation/DocumentAnimations.h"
#include "core/dom/Document.h"
#include "core/frame/Frame.h"
#include "core/frame/FrameView.h"
#include "core/page/Page.h"

namespace WebCore {

PageAnimator::PageAnimator(Page* page)
    : m_page(page)
    , m_servicingAnimations(false)
{
}

void PageAnimator::serviceScriptedAnimations(double monotonicAnimationStartTime)
{
    TemporaryChange<bool> servicing(m_servicingAnimations, true);

    for (RefPtr<Frame> frame = m_page->mainFrame(); frame; frame = frame->tree().traverseNext()) {
        frame->view()->serviceScrollAnimations();
        DocumentAnimations::serviceOnAnimationFrame(*frame->document(), monotonicAnimationStartTime);
    }

    Vector<RefPtr<Document> > documents;
    for (Frame* frame = m_page->mainFrame(); frame; frame = frame->tree().traverseNext())
        documents.append(frame->document());

    for (size_t i = 0; i < documents.size(); ++i)
        documents[i]->serviceScriptedAnimations(monotonicAnimationStartTime);
}

}
