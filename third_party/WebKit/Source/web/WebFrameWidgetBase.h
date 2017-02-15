// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebFrameWidgetBase_h
#define WebFrameWidgetBase_h

#include "core/clipboard/DataObject.h"
#include "platform/UserGestureIndicator.h"
#include "public/platform/WebDragData.h"
#include "public/web/WebFrameWidget.h"
#include "wtf/Assertions.h"

namespace blink {

class AnimationWorkletProxyClient;
class CompositorAnimationHost;
class CompositorWorkerProxyClient;
class GraphicsLayer;
class WebImage;
class WebLayer;
class WebLayerTreeView;
class WebViewImpl;
class HitTestResult;
struct WebPoint;

class WebFrameWidgetBase : public WebFrameWidget {
 public:
  virtual bool forSubframe() const = 0;
  virtual void scheduleAnimation() = 0;
  virtual CompositorWorkerProxyClient* createCompositorWorkerProxyClient() = 0;
  virtual AnimationWorkletProxyClient* createAnimationWorkletProxyClient() = 0;
  virtual WebWidgetClient* client() const = 0;

  // Sets the root graphics layer. |GraphicsLayer| can be null when detaching
  // the root layer.
  virtual void setRootGraphicsLayer(GraphicsLayer*) = 0;

  // Sets the root layer. |WebLayer| can be null when detaching the root layer.
  virtual void setRootLayer(WebLayer*) = 0;

  virtual WebLayerTreeView* getLayerTreeView() const = 0;
  virtual CompositorAnimationHost* animationHost() const = 0;

  virtual HitTestResult coreHitTestResultAt(const WebPoint&) = 0;

  // WebFrameWidget implementation.
  WebDragOperation dragTargetDragEnter(const WebDragData&,
                                       const WebPoint& pointInViewport,
                                       const WebPoint& screenPoint,
                                       WebDragOperationsMask operationsAllowed,
                                       int modifiers) override;
  WebDragOperation dragTargetDragOver(const WebPoint& pointInViewport,
                                      const WebPoint& screenPoint,
                                      WebDragOperationsMask operationsAllowed,
                                      int modifiers) override;
  void dragTargetDragLeave(const WebPoint& pointInViewport,
                           const WebPoint& screenPoint) override;
  void dragTargetDrop(const WebDragData&,
                      const WebPoint& pointInViewport,
                      const WebPoint& screenPoint,
                      int modifiers) override;
  void dragSourceEndedAt(const WebPoint& pointInViewport,
                         const WebPoint& screenPoint,
                         WebDragOperation) override;
  void dragSourceSystemDragEnded() override;

  // Called when a drag-n-drop operation should begin.
  void startDragging(WebReferrerPolicy,
                     const WebDragData&,
                     WebDragOperationsMask,
                     const WebImage& dragImage,
                     const WebPoint& dragImageOffset);

  bool doingDragAndDrop() { return m_doingDragAndDrop; }
  static void setIgnoreInputEvents(bool value) { s_ignoreInputEvents = value; }
  static bool ignoreInputEvents() { return s_ignoreInputEvents; }

  // WebWidget methods.
  void didAcquirePointerLock() override;
  void didNotAcquirePointerLock() override;
  void didLosePointerLock() override;

 protected:
  enum DragAction { DragEnter, DragOver };

  // Consolidate some common code between starting a drag over a target and
  // updating a drag over a target. If we're starting a drag, |isEntering|
  // should be true.
  WebDragOperation dragTargetDragEnterOrOver(const WebPoint& pointInViewport,
                                             const WebPoint& screenPoint,
                                             DragAction,
                                             int modifiers);

  // Helper function to call VisualViewport::viewportToRootFrame().
  WebPoint viewportToRootFrame(const WebPoint& pointInViewport) const;

  WebViewImpl* view() const;

  // Returns the page object associated with this widget. This may be null when
  // the page is shutting down, but will be valid at all other times.
  Page* page() const;

  // A copy of the web drop data object we received from the browser.
  Persistent<DataObject> m_currentDragData;

  bool m_doingDragAndDrop = false;

  // The available drag operations (copy, move link...) allowed by the source.
  WebDragOperation m_operationsAllowed = WebDragOperationNone;

  // The current drag operation as negotiated by the source and destination.
  // When not equal to DragOperationNone, the drag data can be dropped onto the
  // current drop target in this WebView (the drop target can accept the drop).
  WebDragOperation m_dragOperation = WebDragOperationNone;

  // Helper function to process events while pointer locked.
  void pointerLockMouseEvent(const WebInputEvent&);

 private:
  void cancelDrag();

  static bool s_ignoreInputEvents;
  RefPtr<UserGestureToken> m_pointerLockGestureToken;

  friend class WebViewImpl;
};

DEFINE_TYPE_CASTS(WebFrameWidgetBase, WebFrameWidget, widget, true, true);

}  // namespace blink

#endif
