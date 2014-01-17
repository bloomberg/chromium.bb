// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_RENDERER_COMPOSITOR_BINDINGS_WEB_SCROLL_OFFSET_ANIMATION_CURVE_IMPL_H_
#define WEBKIT_RENDERER_COMPOSITOR_BINDINGS_WEB_SCROLL_OFFSET_ANIMATION_CURVE_IMPL_H_

#include "third_party/WebKit/public/platform/WebAnimationCurve.h"

#if WEB_SCROLL_OFFSET_ANIMATION_CURVE_IS_DEFINED

#include "base/memory/scoped_ptr.h"
#include "third_party/WebKit/public/platform/WebScrollOffsetAnimationCurve.h"
#include "webkit/renderer/compositor_bindings/webkit_compositor_bindings_export.h"

namespace cc {
class AnimationCurve;
class ScrollOffsetAnimationCurve;
}

namespace webkit {

class WebScrollOffsetAnimationCurveImpl
    : public blink::WebScrollOffsetAnimationCurve {
 public:
  WEBKIT_COMPOSITOR_BINDINGS_EXPORT WebScrollOffsetAnimationCurveImpl(
      blink::WebFloatPoint target_value,
      TimingFunctionType timing_function);
  virtual ~WebScrollOffsetAnimationCurveImpl();

  // blink::WebAnimationCurve implementation.
  virtual AnimationCurveType type() const;

  // blink::WebScrollOffsetAnimationCurve implementation.
  virtual void setInitialValue(blink::WebFloatPoint initial_value);
  virtual blink::WebFloatPoint getValue(double time) const;
  virtual double duration() const;

  scoped_ptr<cc::AnimationCurve> CloneToAnimationCurve() const;

 private:
  scoped_ptr<cc::ScrollOffsetAnimationCurve> curve_;

  DISALLOW_COPY_AND_ASSIGN(WebScrollOffsetAnimationCurveImpl);
};

}  // namespace webkit

#endif  // WEB_SCROLL_OFFSET_ANIMATION_CURVE_IS_DEFINED

#endif  // WEBKIT_RENDERER_COMPOSITOR_BINDINGS_WEB_SCROLL_OFFSET_ANIMATION_CURVE_IMPL_H_
