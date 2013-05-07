/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/animation/DocumentTimeline.h"

#include "RuntimeEnabledFeatures.h"
#include "core/dom/Document.h"
#include "core/page/FrameView.h"

namespace WebCore {

PassRefPtr<DocumentTimeline> DocumentTimeline::create(Document* document)
{
    return adoptRef(new DocumentTimeline(document));
}

DocumentTimeline::DocumentTimeline(Document* document)
{
    ASSERT(document);
    m_document = document;
}

void DocumentTimeline::play(PassRefPtr<TimedItem> child)
{
    m_children.append(child);

    if (m_document->view())
        m_document->view()->scheduleAnimation();
}

TimedItem::ChildAnimationState DocumentTimeline::serviceAnimations(double monotonicAnimationStartTime)
{
    Vector<size_t> expiredIndices;

    for (size_t i = 0; i < m_children.size(); ++i)
        if (m_children[i]->serviceAnimations(monotonicAnimationStartTime) ==
            TimedItem::AnimationCompleted)
            expiredIndices.append(i);

    for (int i = expiredIndices.size() - 1; i >= 0; i--)
        m_children.remove(expiredIndices[i]);

    if (m_document->view())
        m_document->view()->scheduleAnimation();

    return AnimationInProgress;
}

} // namespace
