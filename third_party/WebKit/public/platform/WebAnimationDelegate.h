/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebAnimationDelegate_h
#define WebAnimationDelegate_h

#include "WebAnimation.h"
#include "WebCommon.h"

#define WEB_ANIMATION_DELEGATE_TAKES_MONOTONIC_TIME 1

namespace blink {

class BLINK_PLATFORM_EXPORT WebAnimationDelegate {
public:
    // FIXME: Remove wallClockTime API after the following file is updated;
    // webkit/renderer/compositor_bindings/web_to_cc_animation_delegate_adapter.cc
    void notifyAnimationStarted(double wallClockTime, double monotonicTime, WebAnimation::TargetProperty prop)
    {
        notifyAnimationStarted(monotonicTime, prop);
    }
    void notifyAnimationFinished(double wallClockTime, double monotonicTime, WebAnimation::TargetProperty prop)
    {
        notifyAnimationFinished(monotonicTime, prop);
    }
    virtual void notifyAnimationStarted(double monotonicTime, WebAnimation::TargetProperty) = 0;
    virtual void notifyAnimationFinished(double monotonicTime, WebAnimation::TargetProperty) = 0;
};

} // namespace blink

#endif // WebAnimationDelegate_h
