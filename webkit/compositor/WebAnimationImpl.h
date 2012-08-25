// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebAnimationImpl_h
#define WebAnimationImpl_h

#include <public/WebAnimation.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>

namespace WebCore {
class CCActiveAnimation;
}

namespace WebKit {

class WebAnimationImpl : public WebAnimation {
public:
    WebAnimationImpl(const WebAnimationCurve&, TargetProperty, int animationId, int groupId);
    virtual ~WebAnimationImpl();

    // WebAnimation implementation
    virtual int id() OVERRIDE;
    virtual TargetProperty targetProperty() const OVERRIDE;
    virtual int iterations() const OVERRIDE;
    virtual void setIterations(int) OVERRIDE;
    virtual double startTime() const OVERRIDE;
    virtual void setStartTime(double monotonicTime) OVERRIDE;
    virtual double timeOffset() const OVERRIDE;
    virtual void setTimeOffset(double monotonicTime) OVERRIDE;
    virtual bool alternatesDirection() const OVERRIDE;
    virtual void setAlternatesDirection(bool) OVERRIDE;

    PassOwnPtr<WebCore::CCActiveAnimation> cloneToCCAnimation();
private:
    OwnPtr<WebCore::CCActiveAnimation> m_animation;
};

}

#endif // WebAnimationImpl_h

