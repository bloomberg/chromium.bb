// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_RENDERER_COMPOSITOR_BINDINGS_WEB_TRANSFORM_ANIMATION_CURVE_IMPL_H_
#define WEBKIT_RENDERER_COMPOSITOR_BINDINGS_WEB_TRANSFORM_ANIMATION_CURVE_IMPL_H_

#include "base/memory/scoped_ptr.h"
#include "third_party/WebKit/public/platform/WebTransformAnimationCurve.h"
#include "webkit/renderer/compositor_bindings/webkit_compositor_bindings_export.h"

namespace cc {
class AnimationCurve;
class KeyframedTransformAnimationCurve;
}

namespace WebKit { class WebTransformKeyframe; }

namespace webkit {

class WebTransformAnimationCurveImpl
    : public WebKit::WebTransformAnimationCurve {
 public:
  WEBKIT_COMPOSITOR_BINDINGS_EXPORT WebTransformAnimationCurveImpl();
  virtual ~WebTransformAnimationCurveImpl();

  // WebKit::WebAnimationCurve implementation.
  virtual AnimationCurveType type() const;

  // WebKit::WebTransformAnimationCurve implementation.
  virtual void add(const WebKit::WebTransformKeyframe& keyframe);
  virtual void add(const WebKit::WebTransformKeyframe& keyframe,
                   TimingFunctionType type);
  virtual void add(const WebKit::WebTransformKeyframe& keyframe,
                   double x1,
                   double y1,
                   double x2,
                   double y2);

  scoped_ptr<cc::AnimationCurve> CloneToAnimationCurve() const;

 private:
  scoped_ptr<cc::KeyframedTransformAnimationCurve> curve_;

  DISALLOW_COPY_AND_ASSIGN(WebTransformAnimationCurveImpl);
};

}  // namespace webkit

#endif  // WEBKIT_RENDERER_COMPOSITOR_BINDINGS_WEB_TRANSFORM_ANIMATION_CURVE_IMPL_H_
