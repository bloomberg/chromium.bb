// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebFloatAnimationCurveImpl_h
#define WebFloatAnimationCurveImpl_h

#include <public/WebFloatAnimationCurve.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>

namespace cc {
class CCAnimationCurve;
class CCKeyframedFloatAnimationCurve;
}

namespace WebKit {

class WebFloatAnimationCurveImpl : public WebFloatAnimationCurve {
public:
    WebFloatAnimationCurveImpl();
    virtual ~WebFloatAnimationCurveImpl();

    // WebAnimationCurve implementation.
    virtual AnimationCurveType type() const OVERRIDE;

    // WebFloatAnimationCurve implementation.
    virtual void add(const WebFloatKeyframe&) OVERRIDE;
    virtual void add(const WebFloatKeyframe&, TimingFunctionType) OVERRIDE;
    virtual void add(const WebFloatKeyframe&, double x1, double y1, double x2, double y2) OVERRIDE;

    virtual float getValue(double time) const OVERRIDE;

    PassOwnPtr<cc::CCAnimationCurve> cloneToCCAnimationCurve() const;

private:
    OwnPtr<cc::CCKeyframedFloatAnimationCurve> m_curve;
};

}

#endif // WebFloatAnimationCurveImpl_h
