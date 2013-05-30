// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_RENDERER_COMPOSITOR_BINDINGS_WEB_FLOAT_ANIMATION_CURVE_IMPL_H_
#define WEBKIT_RENDERER_COMPOSITOR_BINDINGS_WEB_FLOAT_ANIMATION_CURVE_IMPL_H_

#include "base/memory/scoped_ptr.h"
#include "third_party/WebKit/public/platform/WebFloatAnimationCurve.h"
#include "webkit/renderer/compositor_bindings/webkit_compositor_bindings_export.h"

namespace cc {
class AnimationCurve;
class KeyframedFloatAnimationCurve;
}

namespace WebKit { struct WebFloatKeyframe; }

namespace webkit {

class WebFloatAnimationCurveImpl : public WebKit::WebFloatAnimationCurve {
 public:
  WEBKIT_COMPOSITOR_BINDINGS_EXPORT WebFloatAnimationCurveImpl();
  virtual ~WebFloatAnimationCurveImpl();

  // WebAnimationCurve implementation.
  virtual AnimationCurveType type() const;

  // WebFloatAnimationCurve implementation.
  virtual void add(const WebKit::WebFloatKeyframe& keyframe);
  virtual void add(const WebKit::WebFloatKeyframe& keyframe,
                   TimingFunctionType type);
  virtual void add(const WebKit::WebFloatKeyframe& keyframe,
                   double x1,
                   double y1,
                   double x2,
                   double y2);

  virtual float getValue(double time) const;

  scoped_ptr<cc::AnimationCurve> CloneToAnimationCurve() const;

 private:
  scoped_ptr<cc::KeyframedFloatAnimationCurve> curve_;

  DISALLOW_COPY_AND_ASSIGN(WebFloatAnimationCurveImpl);
};

}  // namespace webkit

#endif  // WEBKIT_RENDERER_COMPOSITOR_BINDINGS_WEB_FLOAT_ANIMATION_CURVE_IMPL_H_
