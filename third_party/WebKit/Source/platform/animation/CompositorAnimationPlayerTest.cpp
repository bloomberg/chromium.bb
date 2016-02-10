// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/animation/CompositorAnimationPlayer.h"

#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "public/platform/WebCompositorAnimationDelegate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::TimeTicks;
using blink::WebCompositorAnimationDelegate;
using cc::Animation;
using testing::_;

namespace blink {

class MockWebCompositorAnimationDelegate : public WebCompositorAnimationDelegate {
public:
    MockWebCompositorAnimationDelegate() {}

    MOCK_METHOD2(notifyAnimationStarted, void(double, int));
    MOCK_METHOD2(notifyAnimationFinished, void(double, int));
    MOCK_METHOD2(notifyAnimationAborted, void(double, int));
};

// Test that when the animation delegate is null, the animation player
// doesn't forward the finish notification.
TEST(WebCompositorAnimationPlayerTest, NullDelegate)
{
    scoped_ptr<WebCompositorAnimationDelegate> delegate(
        new MockWebCompositorAnimationDelegate);
    EXPECT_CALL(*static_cast<MockWebCompositorAnimationDelegate*>(delegate.get()),
        notifyAnimationFinished(_, _))
        .Times(1);

    scoped_ptr<CompositorAnimationPlayer> webPlayer(new CompositorAnimationPlayer);
    cc::AnimationPlayer* player = webPlayer->animationPlayer();

    webPlayer->setAnimationDelegate(delegate.get());
    player->NotifyAnimationFinished(TimeTicks(), Animation::SCROLL_OFFSET, 0);

    webPlayer->setAnimationDelegate(nullptr);
    player->NotifyAnimationFinished(TimeTicks(), Animation::SCROLL_OFFSET, 0);
}

} // namespace blink
