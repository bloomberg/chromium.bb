// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebFloatAnimationCurveImpl_h
#define WebFloatAnimationCurveImpl_h

#include "base/memory/scoped_ptr.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebFloatAnimationCurve.h"
#include "webkit/compositor_bindings/webkit_compositor_bindings_export.h"

namespace cc {
class AnimationCurve;
class KeyframedFloatAnimationCurve;
}

namespace WebKit {

class WebFloatAnimationCurveImpl : public WebFloatAnimationCurve {
 public:
  WEBKIT_COMPOSITOR_BINDINGS_EXPORT WebFloatAnimationCurveImpl();
  virtual ~WebFloatAnimationCurveImpl();

  // WebAnimationCurve implementation.
  virtual AnimationCurveType type() const OVERRIDE;

  // WebFloatAnimationCurve implementation.
  virtual void add(const WebFloatKeyframe&) OVERRIDE;
  virtual void add(const WebFloatKeyframe&, TimingFunctionType) OVERRIDE;
  virtual void add(const WebFloatKeyframe&,
                   double x1,
                   double y1,
                   double x2,
                   double y2) OVERRIDE;

  virtual float getValue(double time) const OVERRIDE;

  scoped_ptr<cc::AnimationCurve> cloneToAnimationCurve() const;

 private:
  scoped_ptr<cc::KeyframedFloatAnimationCurve> curve_;
};

}

#endif  // WebFloatAnimationCurveImpl_h
