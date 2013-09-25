/*
 * Copyright (C) 2013 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebAnimationProvider_h
#define WebAnimationProvider_h

#include "CSSPropertyNames.h"
#include "core/platform/animation/KeyframeValueList.h"
#include "wtf/HashMap.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/text/WTFString.h"

namespace WebKit {
class WebAnimation;
}

namespace WebCore {
class CSSAnimationData;
class IntSize;
class KeyframeList;
class RenderStyle;

struct WebAnimations {
    WebAnimations();
    ~WebAnimations();
    WebAnimations(const WebAnimations&);
    bool isEmpty() const;
    OwnPtr<WebKit::WebAnimation> m_transformAnimation;
    OwnPtr<WebKit::WebAnimation> m_opacityAnimation;
    OwnPtr<WebKit::WebAnimation> m_filterAnimation;
};

class WebAnimationProvider {
    WTF_MAKE_NONCOPYABLE(WebAnimationProvider); WTF_MAKE_FAST_ALLOCATED;
public:
    WebAnimationProvider();
    ~WebAnimationProvider();

    int getWebAnimationId(const String& animationName) const;
    int getWebAnimationId(CSSPropertyID) const;
    WebAnimations startAnimation(double timeOffset, const CSSAnimationData*, const KeyframeList&, bool hasTransform, const IntSize& boxSize);
    WebAnimations startTransition(double timeOffset, CSSPropertyID, const RenderStyle* fromStyle, const RenderStyle* toStyle, bool hasTransform, bool hasFilter, const IntSize& boxSize, float fromOpacity, float toOpacity);

private:
    PassOwnPtr<WebKit::WebAnimation> createWebAnimationAndStoreId(const KeyframeValueList&, const IntSize& boxSize, const CSSAnimationData*, const String& animationName, double timeOffset);

    typedef HashMap<String, int> AnimationIdMap;
    AnimationIdMap m_animationIdMap;
};

} // namespace WebCore

#endif // WebAnimationProvider_h
