// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebToCCAnimationDelegateAdapter_h
#define WebToCCAnimationDelegateAdapter_h

#include "cc/animation/animation_delegate.h"
#include "platform/PlatformExport.h"
#include "wtf/Noncopyable.h"

namespace blink {

class WebCompositorAnimationDelegate;

class PLATFORM_EXPORT WebToCCAnimationDelegateAdapter : public cc::AnimationDelegate {
    WTF_MAKE_NONCOPYABLE(WebToCCAnimationDelegateAdapter);
public:
    explicit WebToCCAnimationDelegateAdapter(WebCompositorAnimationDelegate*);

private:
    void NotifyAnimationStarted(base::TimeTicks monotonicTime, cc::Animation::TargetProperty, int group) override;
    void NotifyAnimationFinished(base::TimeTicks monotonicTime, cc::Animation::TargetProperty, int group) override;
    void NotifyAnimationAborted(base::TimeTicks monotonicTime, cc::Animation::TargetProperty, int group) override;

    WebCompositorAnimationDelegate* m_delegate;
};

} // namespace blink

#endif // WebToCCAnimationDelegateAdapter_h
