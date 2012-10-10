// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebTransformAnimationCurveImpl_h
#define WebTransformAnimationCurveImpl_h

#include <public/WebTransformAnimationCurve.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>

namespace cc {
class CCAnimationCurve;
class CCKeyframedTransformAnimationCurve;
}

namespace WebKit {

class WebTransformAnimationCurveImpl : public WebTransformAnimationCurve {
public:
    WebTransformAnimationCurveImpl();
    virtual ~WebTransformAnimationCurveImpl();

    // WebAnimationCurve implementation.
    virtual AnimationCurveType type() const OVERRIDE;

    // WebTransformAnimationCurve implementation.
    virtual void add(const WebTransformKeyframe&) OVERRIDE;
    virtual void add(const WebTransformKeyframe&, TimingFunctionType) OVERRIDE;
    virtual void add(const WebTransformKeyframe&, double x1, double y1, double x2, double y2) OVERRIDE;

    virtual WebTransformationMatrix getValue(double time) const OVERRIDE;

    PassOwnPtr<cc::CCAnimationCurve> cloneToCCAnimationCurve() const;

private:
    OwnPtr<cc::CCKeyframedTransformAnimationCurve> m_curve;
};

}

#endif // WebTransformAnimationCurveImpl_h
