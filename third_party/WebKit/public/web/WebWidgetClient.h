/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebWidgetClient_h
#define WebWidgetClient_h

#include "WebNavigationPolicy.h"
#include "public/platform/WebCommon.h"
#include "public/platform/WebDragOperation.h"
#include "public/platform/WebLayerTreeView.h"
#include "public/platform/WebPoint.h"
#include "public/platform/WebRect.h"
#include "public/platform/WebReferrerPolicy.h"
#include "public/platform/WebScreenInfo.h"
#include "public/platform/WebTouchAction.h"
#include "public/web/WebMeaningfulLayout.h"
#include "public/web/WebTextDirection.h"

namespace blink {

class WebDragData;
class WebGestureEvent;
class WebImage;
class WebNode;
class WebString;
class WebWidget;
struct WebCursorInfo;
struct WebFloatPoint;
struct WebFloatRect;
struct WebFloatSize;

class WebWidgetClient {
 public:
  virtual ~WebWidgetClient() {}

  // Called when a region of the WebWidget needs to be re-painted.
  virtual void DidInvalidateRect(const WebRect&) {}

  // Attempt to initialize compositing view for this widget. If successful,
  // returns a valid WebLayerTreeView which is owned by the
  // WebWidgetClient.
  virtual WebLayerTreeView* InitializeLayerTreeView() { return nullptr; }

  // FIXME: Remove all overrides of this.
  virtual bool AllowsBrokenNullLayerTreeView() const { return false; }

  // Called when a call to WebWidget::animate is required
  virtual void ScheduleAnimation() {}

  // Called immediately following the first compositor-driven (frame-generating)
  // layout that happened after an interesting document lifecyle change (see
  // WebMeaningfulLayout for details.)
  virtual void DidMeaningfulLayout(WebMeaningfulLayout) {}

  virtual void DidFirstLayoutAfterFinishedParsing() {}

  // Called when the cursor for the widget changes.
  virtual void DidChangeCursor(const WebCursorInfo&) {}

  virtual void AutoscrollStart(const WebFloatPoint&) {}
  virtual void AutoscrollFling(const WebFloatSize& velocity) {}
  virtual void AutoscrollEnd() {}

  // Called when the widget should be closed.  WebWidget::close() should
  // be called asynchronously as a result of this notification.
  virtual void CloseWidgetSoon() {}

  // Called to show the widget according to the given policy.
  virtual void Show(WebNavigationPolicy) {}

  // Called to get/set the position of the widget's window in screen
  // coordinates. Note, the window includes any decorations such as borders,
  // scrollbars, URL bar, tab strip, etc. if they exist.
  virtual WebRect WindowRect() { return WebRect(); }
  virtual void SetWindowRect(const WebRect&) {}

  // Called to get the view rect in screen coordinates. This is the actual
  // content view area, i.e. doesn't include any window decorations.
  virtual WebRect ViewRect() { return WebRect(); }

  // Called when a tooltip should be shown at the current cursor position.
  virtual void SetToolTipText(const WebString&, WebTextDirection hint) {}

  // Called to query information about the screen where this widget is
  // displayed.
  virtual WebScreenInfo GetScreenInfo() { return WebScreenInfo(); }

  // Requests to lock the mouse cursor. If true is returned, the success
  // result will be asynchronously returned via a single call to
  // WebWidget::didAcquirePointerLock() or
  // WebWidget::didNotAcquirePointerLock().
  // If false, the request has been denied synchronously.
  virtual bool RequestPointerLock() { return false; }

  // Cause the pointer lock to be released. This may be called at any time,
  // including when a lock is pending but not yet acquired.
  // WebWidget::didLosePointerLock() is called when unlock is complete.
  virtual void RequestPointerUnlock() {}

  // Returns true iff the pointer is locked to this widget.
  virtual bool IsPointerLocked() { return false; }

  // Called when a gesture event is handled.
  virtual void DidHandleGestureEvent(const WebGestureEvent& event,
                                     bool event_cancelled) {}

  // Called when overscrolled on main thread. All parameters are in
  // viewport-space.
  virtual void DidOverscroll(const WebFloatSize& overscroll_delta,
                             const WebFloatSize& accumulated_overscroll,
                             const WebFloatPoint& position_in_viewport,
                             const WebFloatSize& velocity_in_viewport) {}

  // Called to update if touch events should be sent.
  virtual void HasTouchEventHandlers(bool) {}

  // Called to update whether low latency input mode is enabled or not.
  virtual void SetNeedsLowLatencyInput(bool) {}

  // Called during WebWidget::HandleInputEvent for a TouchStart event to inform
  // the embedder of the touch actions that are permitted for this touch.
  virtual void SetTouchAction(WebTouchAction touch_action) {}

  // Request the browser to show virtual keyboard for current input type.
  virtual void ShowVirtualKeyboardOnElementFocus() {}

  // Request that the browser show a UI for an unhandled tap, if needed.
  // Invoked during the handling of a GestureTap input event whenever the
  // event is not consumed.
  // |tappedPosition| is the point where the mouseDown occurred in client
  // coordinates.
  // |tappedNode| is the node that the mouseDown event hit, provided so the
  // UI can be shown only on certain kinds of nodes in specific locations.
  // |pageChanged| is true if and only if the DOM tree or style was
  // modified during the dispatch of the mouse*, or click events associated
  // with the tap.
  // This provides a heuristic to help identify when a page is doing
  // something as a result of a tap without explicitly consuming the event.
  virtual void ShowUnhandledTapUIIfNeeded(const WebPoint& tapped_position,
                                          const WebNode& tapped_node,
                                          bool page_changed) {}

  // Converts the |rect| from Blink's Viewport coordinates to the
  // coordinates in the native window used to display the content, in
  // DIP.  They're identical in tradional world, but will differ when
  // use-zoom-for-dsf feature is eanbled, and Viewport coordinates
  // becomes DSF times larger than window coordinates.
  // TODO(oshima): Update the comment when the migration is completed.
  virtual void ConvertViewportToWindow(WebRect* rect) {}

  // Converts the |rect| from the coordinates in native window in
  // DIP to Blink's Viewport coordinates. They're identical in
  // tradional world, but will differ when use-zoom-for-dsf feature
  // is eanbled.  TODO(oshima): Update the comment when the
  // migration is completed.
  virtual void ConvertWindowToViewport(WebFloatRect* rect) {}

  // Called when a drag-and-drop operation should begin.
  virtual void StartDragging(WebReferrerPolicy,
                             const WebDragData&,
                             WebDragOperationsMask,
                             const WebImage& drag_image,
                             const WebPoint& drag_image_offset) {}
};

}  // namespace blink

#endif
