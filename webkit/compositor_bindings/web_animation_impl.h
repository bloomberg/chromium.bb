// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebAnimationImpl_h
#define WebAnimationImpl_h

#include "base/memory/scoped_ptr.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebAnimation.h"
#include "webkit/compositor_bindings/webkit_compositor_bindings_export.h"

namespace cc {
class Animation;
}

namespace WebKit {

class WebAnimationImpl : public WebAnimation {
public:
    WEBKIT_COMPOSITOR_BINDINGS_EXPORT WebAnimationImpl(
        const WebAnimationCurve&, TargetProperty, int animationId, int groupId = 0);
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

    scoped_ptr<cc::Animation> cloneToAnimation();

private:
    scoped_ptr<cc::Animation> m_animation;
};

}

#endif // WebAnimationImpl_h
