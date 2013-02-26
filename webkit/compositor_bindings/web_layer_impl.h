// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebLayerImpl_h
#define WebLayerImpl_h

#include "base/memory/ref_counted.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebLayer.h"
#include "webkit/compositor_bindings/webkit_compositor_bindings_export.h"

namespace cc { class Layer; }

// TODO(senorblanco):  Remove this once WebKit changes have landed.
class SkImageFilter;

namespace WebKit {

class WebLayerImpl : public WebLayer {
 public:
  WEBKIT_COMPOSITOR_BINDINGS_EXPORT WebLayerImpl();
  WEBKIT_COMPOSITOR_BINDINGS_EXPORT explicit WebLayerImpl(
      scoped_refptr<cc::Layer>);
  virtual ~WebLayerImpl();

  WEBKIT_COMPOSITOR_BINDINGS_EXPORT cc::Layer* layer() const;

  // WebLayer implementation.
  virtual int id() const OVERRIDE;
  virtual void invalidateRect(const WebFloatRect&) OVERRIDE;
  virtual void invalidate() OVERRIDE;
  virtual void addChild(WebLayer*) OVERRIDE;
  virtual void insertChild(WebLayer*, size_t index) OVERRIDE;
  virtual void replaceChild(WebLayer* reference, WebLayer* new_layer) OVERRIDE;
  virtual void removeFromParent() OVERRIDE;
  virtual void removeAllChildren() OVERRIDE;
  virtual void setAnchorPoint(const WebFloatPoint&) OVERRIDE;
  virtual WebFloatPoint anchorPoint() const OVERRIDE;
  virtual void setAnchorPointZ(float) OVERRIDE;
  virtual float anchorPointZ() const OVERRIDE;
  virtual void setBounds(const WebSize&) OVERRIDE;
  virtual WebSize bounds() const OVERRIDE;
  virtual void setMasksToBounds(bool) OVERRIDE;
  virtual bool masksToBounds() const OVERRIDE;
  virtual void setMaskLayer(WebLayer*) OVERRIDE;
  virtual void setReplicaLayer(WebLayer*) OVERRIDE;
  virtual void setOpacity(float) OVERRIDE;
  virtual float opacity() const OVERRIDE;
  virtual void setOpaque(bool) OVERRIDE;
  virtual bool opaque() const OVERRIDE;
  virtual void setPosition(const WebFloatPoint&) OVERRIDE;
  virtual WebFloatPoint position() const OVERRIDE;
  virtual void setSublayerTransform(const SkMatrix44&) OVERRIDE;
  virtual void setSublayerTransform(const WebTransformationMatrix&) OVERRIDE;
  virtual SkMatrix44 sublayerTransform() const OVERRIDE;
  virtual void setTransform(const SkMatrix44&) OVERRIDE;
  virtual void setTransform(const WebTransformationMatrix&) OVERRIDE;
  virtual SkMatrix44 transform() const OVERRIDE;
  virtual void setDrawsContent(bool) OVERRIDE;
  virtual bool drawsContent() const OVERRIDE;
  virtual void setPreserves3D(bool) OVERRIDE;
  virtual void setUseParentBackfaceVisibility(bool) OVERRIDE;
  virtual void setBackgroundColor(WebColor) OVERRIDE;
  virtual WebColor backgroundColor() const;
  virtual void setFilter(SkImageFilter*);
  virtual void setFilters(const WebFilterOperations&) OVERRIDE;
  virtual void setBackgroundFilters(const WebFilterOperations&) OVERRIDE;
  virtual void setDebugName(WebString) OVERRIDE;
  virtual void setAnimationDelegate(WebAnimationDelegate*) OVERRIDE;
  virtual bool addAnimation(WebAnimation*) OVERRIDE;
  virtual void removeAnimation(int animation_id) OVERRIDE;
  virtual void removeAnimation(int animation_id, WebAnimation::TargetProperty)
      OVERRIDE;
  virtual void pauseAnimation(int animation_id, double time_offset) OVERRIDE;
  virtual void suspendAnimations(double monotonic_time) OVERRIDE;
  virtual void resumeAnimations(double monotonic_time) OVERRIDE;
  virtual bool hasActiveAnimation() OVERRIDE;
  virtual void transferAnimationsTo(WebLayer*) OVERRIDE;
  virtual void setForceRenderSurface(bool) OVERRIDE;
  virtual void setScrollPosition(WebPoint) OVERRIDE;
  virtual WebPoint scrollPosition() const OVERRIDE;
  virtual void setMaxScrollPosition(WebSize) OVERRIDE;
  virtual WebSize maxScrollPosition() const;
  virtual void setScrollable(bool) OVERRIDE;
  virtual bool scrollable() const;
  virtual void setHaveWheelEventHandlers(bool) OVERRIDE;
  virtual bool haveWheelEventHandlers() const;
  virtual void setShouldScrollOnMainThread(bool) OVERRIDE;
  virtual bool shouldScrollOnMainThread() const;
  virtual void setNonFastScrollableRegion(const WebVector<WebRect>&) OVERRIDE;
  virtual WebVector<WebRect> nonFastScrollableRegion() const;
  virtual void setTouchEventHandlerRegion(const WebVector<WebRect>&);
  virtual WebVector<WebRect> touchEventHandlerRegion() const;
  virtual void setIsContainerForFixedPositionLayers(bool) OVERRIDE;
  virtual bool isContainerForFixedPositionLayers() const;
  virtual void setFixedToContainerLayer(bool) OVERRIDE;
  virtual bool fixedToContainerLayer() const;
  virtual void setScrollClient(WebLayerScrollClient*) OVERRIDE;

 protected:
  scoped_refptr<cc::Layer> layer_;
};

}  // namespace WebKit

#endif  // WebLayerImpl_h
