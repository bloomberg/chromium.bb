// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebTransformAnimationCurveImpl_h
#define WebTransformAnimationCurveImpl_h

#include "base/memory/scoped_ptr.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebTransformAnimationCurve.h"
#include "webkit/compositor_bindings/webkit_compositor_bindings_export.h"

namespace cc {
class AnimationCurve;
class KeyframedTransformAnimationCurve;
}

namespace WebKit {

class WebTransformAnimationCurveImpl : public WebTransformAnimationCurve {
 public:
  WEBKIT_COMPOSITOR_BINDINGS_EXPORT WebTransformAnimationCurveImpl();
  virtual ~WebTransformAnimationCurveImpl();

  // WebAnimationCurve implementation.
  virtual AnimationCurveType type() const OVERRIDE;

  // WebTransformAnimationCurve implementation.
  virtual void add(const WebTransformKeyframe&) OVERRIDE;
  virtual void add(const WebTransformKeyframe&, TimingFunctionType) OVERRIDE;
  virtual void add(const WebTransformKeyframe&,
                   double x1,
                   double y1,
                   double x2,
                   double y2) OVERRIDE;

  virtual WebTransformationMatrix getValue(double time) const OVERRIDE;

  scoped_ptr<cc::AnimationCurve> cloneToAnimationCurve() const;

 private:
  scoped_ptr<cc::KeyframedTransformAnimationCurve> curve_;
};

}

#endif  // WebTransformAnimationCurveImpl_h
