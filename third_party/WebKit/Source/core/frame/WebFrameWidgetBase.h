// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebFrameWidgetBase_h
#define WebFrameWidgetBase_h

#include "core/CoreExport.h"
#include "core/clipboard/DataObject.h"
#include "core/dom/UserGestureIndicator.h"
#include "platform/graphics/paint/PaintImage.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Assertions.h"
#include "public/platform/WebCoalescedInputEvent.h"
#include "public/platform/WebDragData.h"
#include "public/web/WebFrameWidget.h"

namespace blink {

class CompositorAnimationHost;
class CompositorMutatorImpl;
class GraphicsLayer;
class WebImage;
class WebLayer;
class WebLayerTreeView;
class WebViewBase;
class HitTestResult;
struct WebPoint;

class CORE_EXPORT WebFrameWidgetBase
    : public GarbageCollectedFinalized<WebFrameWidgetBase>,
      public NON_EXPORTED_BASE(WebFrameWidget) {
 public:
  virtual ~WebFrameWidgetBase() {}

  virtual bool ForSubframe() const = 0;
  virtual void ScheduleAnimation() = 0;
  virtual CompositorMutatorImpl* CompositorMutator() = 0;

  virtual WebWidgetClient* Client() const = 0;

  // Sets the root graphics layer. |GraphicsLayer| can be null when detaching
  // the root layer.
  virtual void SetRootGraphicsLayer(GraphicsLayer*) = 0;
  virtual GraphicsLayer* RootGraphicsLayer() const = 0;

  // Sets the root layer. |WebLayer| can be null when detaching the root layer.
  virtual void SetRootLayer(WebLayer*) = 0;

  virtual WebLayerTreeView* GetLayerTreeView() const = 0;
  virtual CompositorAnimationHost* AnimationHost() const = 0;

  virtual HitTestResult CoreHitTestResultAt(const WebPoint&) = 0;

  // WebFrameWidget implementation.
  WebDragOperation DragTargetDragEnter(const WebDragData&,
                                       const WebPoint& point_in_viewport,
                                       const WebPoint& screen_point,
                                       WebDragOperationsMask operations_allowed,
                                       int modifiers) override;
  WebDragOperation DragTargetDragOver(const WebPoint& point_in_viewport,
                                      const WebPoint& screen_point,
                                      WebDragOperationsMask operations_allowed,
                                      int modifiers) override;
  void DragTargetDragLeave(const WebPoint& point_in_viewport,
                           const WebPoint& screen_point) override;
  void DragTargetDrop(const WebDragData&,
                      const WebPoint& point_in_viewport,
                      const WebPoint& screen_point,
                      int modifiers) override;
  void DragSourceEndedAt(const WebPoint& point_in_viewport,
                         const WebPoint& screen_point,
                         WebDragOperation) override;
  void DragSourceSystemDragEnded() override;

  // Called when a drag-n-drop operation should begin.
  void StartDragging(WebReferrerPolicy,
                     const WebDragData&,
                     WebDragOperationsMask,
                     const WebImage& drag_image,
                     const WebPoint& drag_image_offset);

  bool DoingDragAndDrop() { return doing_drag_and_drop_; }
  static void SetIgnoreInputEvents(bool value) { ignore_input_events_ = value; }
  static bool IgnoreInputEvents() { return ignore_input_events_; }

  // WebWidget methods.
  void DidAcquirePointerLock() override;
  void DidNotAcquirePointerLock() override;
  void DidLosePointerLock() override;
  void ShowContextMenu(WebMenuSourceType) override;

  // Image decode functionality.
  void RequestDecode(const PaintImage&, WTF::Function<void(bool)> callback);

  DECLARE_VIRTUAL_TRACE();

 protected:
  enum DragAction { kDragEnter, kDragOver };

  // Consolidate some common code between starting a drag over a target and
  // updating a drag over a target. If we're starting a drag, |isEntering|
  // should be true.
  WebDragOperation DragTargetDragEnterOrOver(const WebPoint& point_in_viewport,
                                             const WebPoint& screen_point,
                                             DragAction,
                                             int modifiers);

  // Helper function to call VisualViewport::viewportToRootFrame().
  WebPoint ViewportToRootFrame(const WebPoint& point_in_viewport) const;

  WebViewBase* View() const;

  // Returns the page object associated with this widget. This may be null when
  // the page is shutting down, but will be valid at all other times.
  Page* GetPage() const;

  // Helper function to process events while pointer locked.
  void PointerLockMouseEvent(const WebCoalescedInputEvent&);

  // A copy of the web drop data object we received from the browser.
  Member<DataObject> current_drag_data_;

  bool doing_drag_and_drop_ = false;

  // The available drag operations (copy, move link...) allowed by the source.
  WebDragOperation operations_allowed_ = kWebDragOperationNone;

  // The current drag operation as negotiated by the source and destination.
  // When not equal to DragOperationNone, the drag data can be dropped onto the
  // current drop target in this WebView (the drop target can accept the drop).
  WebDragOperation drag_operation_ = kWebDragOperationNone;

 private:
  void CancelDrag();
  LocalFrame* FocusedLocalFrameInWidget() const;

  static bool ignore_input_events_;
  RefPtr<UserGestureToken> pointer_lock_gesture_token_;

  friend class WebViewImpl;
};

DEFINE_TYPE_CASTS(WebFrameWidgetBase, WebFrameWidget, widget, true, true);

}  // namespace blink

#endif
