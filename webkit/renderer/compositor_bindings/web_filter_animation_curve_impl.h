// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_RENDERER_COMPOSITOR_BINDINGS_WEB_FILTER_ANIMATION_CURVE_IMPL_H_
#define WEBKIT_RENDERER_COMPOSITOR_BINDINGS_WEB_FILTER_ANIMATION_CURVE_IMPL_H_

#include "base/memory/scoped_ptr.h"
#include "third_party/WebKit/public/platform/WebFilterAnimationCurve.h"
#include "webkit/renderer/compositor_bindings/webkit_compositor_bindings_export.h"

namespace cc {
class AnimationCurve;
class KeyframedFilterAnimationCurve;
}

namespace blink { class WebFilterKeyframe; }

namespace webkit {

class WebFilterAnimationCurveImpl
    : public blink::WebFilterAnimationCurve {
 public:
  WEBKIT_COMPOSITOR_BINDINGS_EXPORT WebFilterAnimationCurveImpl();
  virtual ~WebFilterAnimationCurveImpl();

  // blink::WebAnimationCurve implementation.
  virtual AnimationCurveType type() const;

  // blink::WebFilterAnimationCurve implementation.
  virtual void add(const blink::WebFilterKeyframe& keyframe,
                   TimingFunctionType type);
  virtual void add(const blink::WebFilterKeyframe& keyframe,
                   double x1,
                   double y1,
                   double x2,
                   double y2);

  scoped_ptr<cc::AnimationCurve> CloneToAnimationCurve() const;

 private:
  scoped_ptr<cc::KeyframedFilterAnimationCurve> curve_;

  DISALLOW_COPY_AND_ASSIGN(WebFilterAnimationCurveImpl);
};

}  // namespace webkit

#endif  // WEBKIT_RENDERER_COMPOSITOR_BINDINGS_WEB_FILTER_ANIMATION_CURVE_IMPL_H_
