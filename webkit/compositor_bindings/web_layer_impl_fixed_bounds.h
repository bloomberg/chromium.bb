// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebLayerImplFixedBounds_h
#define WebLayerImplFixedBounds_h

#include "ui/gfx/size.h"
#include "ui/gfx/transform.h"
#include "webkit/compositor_bindings/web_layer_impl.h"

namespace WebKit {

// A special implementation of WebLayerImpl for layers that its contents
// need to be automatically scaled when the bounds changes. The compositor
// can efficiently handle the bounds change of such layers if the bounds
// is fixed to a given value and the change of bounds are converted to
// transformation scales.
class WebLayerImplFixedBounds : public WebLayerImpl {
 public:
  WEBKIT_COMPOSITOR_BINDINGS_EXPORT WebLayerImplFixedBounds();
  WEBKIT_COMPOSITOR_BINDINGS_EXPORT explicit WebLayerImplFixedBounds(
      scoped_refptr<cc::Layer>);
  virtual ~WebLayerImplFixedBounds();

  // WebLayerImpl overrides.
  virtual void invalidateRect(const WebFloatRect&) OVERRIDE;
  virtual void setAnchorPoint(const WebFloatPoint&) OVERRIDE;
  virtual void setBounds(const WebSize&) OVERRIDE;
  virtual WebSize bounds() const OVERRIDE;
  virtual void setSublayerTransform(const SkMatrix44&) OVERRIDE;
  virtual void setSublayerTransform(const WebTransformationMatrix&) OVERRIDE;
  virtual SkMatrix44 sublayerTransform() const OVERRIDE;
  virtual void setTransform(const SkMatrix44&) OVERRIDE;
  virtual void setTransform(const WebTransformationMatrix&) OVERRIDE;
  virtual SkMatrix44 transform() const OVERRIDE;

  WEBKIT_COMPOSITOR_BINDINGS_EXPORT void SetFixedBounds(const gfx::Size&);

 protected:
  void SetTransformInternal(const gfx::Transform&);
  void SetSublayerTransformInternal(const gfx::Transform&);
  void UpdateLayerBoundsAndTransform();

  gfx::Transform original_sublayer_transform_;
  gfx::Transform original_transform_;
  gfx::Size original_bounds_;
  gfx::Size fixed_bounds_;
};

} // namespace WebKit

#endif // WebLayerImplFixedBounds_h
