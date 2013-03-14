// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebLayerImpl_h
#define WebLayerImpl_h

#include "base/memory/ref_counted.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebAnimation.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebColor.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebFloatPoint.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebLayer.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebPoint.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebRect.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebSize.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebString.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebVector.h"
#include "third_party/skia/include/utils/SkMatrix44.h"
#include "webkit/compositor_bindings/webkit_compositor_bindings_export.h"

namespace cc { class Layer; }

// TODO(senorblanco):  Remove this once WebKit changes have landed.
class SkImageFilter;

namespace WebKit {
class WebAnimationDelegate;
class WebFilterOperations;
class WebLayerScrollClient;
struct WebFloatRect;

class WebLayerImpl : public WebLayer {
 public:
  WEBKIT_COMPOSITOR_BINDINGS_EXPORT WebLayerImpl();
  WEBKIT_COMPOSITOR_BINDINGS_EXPORT explicit WebLayerImpl(
      scoped_refptr<cc::Layer>);
  virtual ~WebLayerImpl();

  WEBKIT_COMPOSITOR_BINDINGS_EXPORT cc::Layer* layer() const;

  // WebLayer implementation.
  virtual int id() const;
  virtual void invalidateRect(const WebFloatRect&);
  virtual void invalidate();
  virtual void addChild(WebLayer*);
  virtual void insertChild(WebLayer*, size_t index);
  virtual void replaceChild(WebLayer* reference, WebLayer* new_layer);
  virtual void removeFromParent();
  virtual void removeAllChildren();
  virtual void setAnchorPoint(const WebFloatPoint&);
  virtual WebFloatPoint anchorPoint() const;
  virtual void setAnchorPointZ(float);
  virtual float anchorPointZ() const;
  virtual void setBounds(const WebSize&);
  virtual WebSize bounds() const;
  virtual void setMasksToBounds(bool);
  virtual bool masksToBounds() const;
  virtual void setMaskLayer(WebLayer*);
  virtual void setReplicaLayer(WebLayer*);
  virtual void setOpacity(float);
  virtual float opacity() const;
  virtual void setOpaque(bool);
  virtual bool opaque() const;
  virtual void setPosition(const WebFloatPoint&);
  virtual WebFloatPoint position() const;
  virtual void setSublayerTransform(const SkMatrix44&);
  virtual SkMatrix44 sublayerTransform() const;
  virtual void setTransform(const SkMatrix44&);
  virtual SkMatrix44 transform() const;
  virtual void setDrawsContent(bool);
  virtual bool drawsContent() const;
  virtual void setPreserves3D(bool);
  virtual void setUseParentBackfaceVisibility(bool);
  virtual void setBackgroundColor(WebColor);
  virtual WebColor backgroundColor() const;
  virtual void setFilter(SkImageFilter*);
  virtual void setFilters(const WebFilterOperations&);
  virtual void setBackgroundFilters(const WebFilterOperations&);
  virtual void setDebugName(WebString);
  virtual void setAnimationDelegate(WebAnimationDelegate*);
  virtual bool addAnimation(WebAnimation*);
  virtual void removeAnimation(int animation_id);
  virtual void removeAnimation(int animation_id, WebAnimation::TargetProperty);
  virtual void pauseAnimation(int animation_id, double time_offset);
  virtual void suspendAnimations(double monotonic_time);
  virtual void resumeAnimations(double monotonic_time);
  virtual bool hasActiveAnimation();
  virtual void transferAnimationsTo(WebLayer*);
  virtual void setForceRenderSurface(bool);
  virtual void setScrollPosition(WebPoint);
  virtual WebPoint scrollPosition() const;
  virtual void setMaxScrollPosition(WebSize);
  virtual WebSize maxScrollPosition() const;
  virtual void setScrollable(bool);
  virtual bool scrollable() const;
  virtual void setHaveWheelEventHandlers(bool);
  virtual bool haveWheelEventHandlers() const;
  virtual void setShouldScrollOnMainThread(bool);
  virtual bool shouldScrollOnMainThread() const;
  virtual void setNonFastScrollableRegion(const WebVector<WebRect>&);
  virtual WebVector<WebRect> nonFastScrollableRegion() const;
  virtual void setTouchEventHandlerRegion(const WebVector<WebRect>&);
  virtual WebVector<WebRect> touchEventHandlerRegion() const;
  virtual void setIsContainerForFixedPositionLayers(bool);
  virtual bool isContainerForFixedPositionLayers() const;
  virtual void setFixedToContainerLayer(bool);
  virtual bool fixedToContainerLayer() const;
  virtual void setScrollClient(WebLayerScrollClient*);

 protected:
  scoped_refptr<cc::Layer> layer_;
};

}  // namespace WebKit

#endif  // WebLayerImpl_h
