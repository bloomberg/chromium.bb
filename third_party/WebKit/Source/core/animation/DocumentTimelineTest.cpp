/*
 * Copyright (c) 2013, Google Inc. All rights reserved.
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

#include "core/animation/Animation.h"
#include "core/animation/KeyframeAnimationEffect.h"
#include "core/animation/TimedItem.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/QualifiedName.h"
#include "weborigin/KURL.h"
#include <gtest/gtest.h>

using namespace WebCore;

namespace {

class DocumentTimelineTest : public ::testing::Test {
protected:
    virtual void SetUp()
    {
        document = Document::create();
        element = Element::create(nullQName() , document.get());
        timeline = DocumentTimeline::create(document.get());
    }

    RefPtr<Document> document;
    RefPtr<Element> element;
    RefPtr<DocumentTimeline> timeline;
    Timing timing;
};

TEST_F(DocumentTimelineTest, EmptyKeyframeAnimation)
{
    RefPtr<KeyframeAnimationEffect> effect = KeyframeAnimationEffect::create(KeyframeAnimationEffect::KeyframeVector());
    RefPtr<Animation> anim = Animation::create(element.get(), effect, timing);

    EXPECT_TRUE(isNull(timeline->currentTime()));

    timeline->play(anim.get());
    EXPECT_TRUE(isNull(timeline->currentTime()));

    timeline->serviceAnimations(0);
    EXPECT_FLOAT_EQ(0, timeline->currentTime());
    EXPECT_TRUE(anim->compositableValues()->isEmpty());

    timeline->serviceAnimations(100);
    EXPECT_FLOAT_EQ(100, timeline->currentTime());
}

TEST_F(DocumentTimelineTest, PauseForTesting)
{
    float seekTime = 1;
    RefPtr<Animation> anim1 = Animation::create(element.get(), KeyframeAnimationEffect::create(KeyframeAnimationEffect::KeyframeVector()), timing);
    RefPtr<Animation> anim2  = Animation::create(element.get(), KeyframeAnimationEffect::create(KeyframeAnimationEffect::KeyframeVector()), timing);
    RefPtr<Player> player1 = timeline->play(anim1.get());
    RefPtr<Player> player2 = timeline->play(anim2.get());
    timeline->pauseAnimationsForTesting(seekTime);

    EXPECT_FLOAT_EQ(seekTime, player1->currentTime());
    EXPECT_FLOAT_EQ(seekTime, player2->currentTime());
}

}
