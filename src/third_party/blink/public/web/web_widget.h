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

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_WIDGET_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_WIDGET_H_

#include "base/callback.h"
#include "base/time/time.h"
#include "cc/input/browser_controls_state.h"
#include "cc/metrics/begin_main_frame_metrics.h"
#include "cc/paint/element_id.h"
#include "cc/trees/layer_tree_host_client.h"
#include "third_party/blink/public/common/input/web_menu_source_type.h"
#include "third_party/blink/public/common/metrics/document_update_reason.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom-shared.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_input_event_result.h"
#include "third_party/blink/public/platform/web_rect.h"
#include "third_party/blink/public/platform/web_size.h"
#include "third_party/blink/public/platform/web_text_input_info.h"
#include "third_party/blink/public/web/web_hit_test_result.h"
#include "third_party/blink/public/web/web_ime_text_span.h"
#include "third_party/blink/public/web/web_lifecycle_update.h"
#include "third_party/blink/public/web/web_range.h"
#include "third_party/blink/public/web/web_swap_result.h"

namespace cc {
class LayerTreeHost;
class TaskGraphRunner;
class UkmRecorderFactory;
class LayerTreeSettings;
}

namespace ui {
class Cursor;
}

namespace blink {
class WebCoalescedInputEvent;

namespace scheduler {
class WebRenderWidgetSchedulingState;
}

class WebWidget {
 public:
  // Initialize compositing. This will create a LayerTreeHost but will not
  // allocate a frame sink or begin producing frames until SetCompositorVisible
  // is called.
  virtual cc::LayerTreeHost* InitializeCompositing(
      cc::TaskGraphRunner* task_graph_runner,
      const cc::LayerTreeSettings& settings,
      std::unique_ptr<cc::UkmRecorderFactory> ukm_recorder_factory) = 0;

  // This method closes and deletes the WebWidget. If a |cleanup_task| is
  // provided it should run on the |cleanup_runner| after the WebWidget has
  // added its own tasks to the |cleanup_runner|.
  virtual void Close(
      scoped_refptr<base::SingleThreadTaskRunner> cleanup_runner = nullptr,
      base::OnceCallback<void()> cleanup_task = base::OnceCallback<void()>()) {}

  // Set the compositor as visible. If |visible| is true, then the compositor
  // will request a new layer frame sink and begin producing frames from the
  // compositor.
  virtual void SetCompositorVisible(bool visible) = 0;

  // Returns the current size of the WebWidget.
  virtual WebSize Size() { return WebSize(); }

  // Called to resize the WebWidget.
  virtual void Resize(const WebSize&) {}

  // Called to notify the WebWidget of entering/exiting fullscreen mode.
  virtual void DidEnterFullscreen() {}
  virtual void DidExitFullscreen() {}

  // Called to run through the entire set of document lifecycle phases needed
  // to render a frame of the web widget. This MUST be called before Paint,
  // and it may result in calls to WebViewClient::DidInvalidateRect (for
  // non-composited WebViews).
  // |reason| must be used to indicate the source of the
  // update for the purposes of metrics gathering.
  virtual void UpdateAllLifecyclePhases(DocumentUpdateReason reason) {
    UpdateLifecycle(WebLifecycleUpdate::kAll, reason);
  }

  // UpdateLifecycle is used to update to a specific lifestyle phase, as given
  // by |LifecycleUpdate|. To update all lifecycle phases, use
  // UpdateAllLifecyclePhases.
  // |reason| must be used to indicate the source of the
  // update for the purposes of metrics gathering.
  virtual void UpdateLifecycle(WebLifecycleUpdate requested_update,
                               DocumentUpdateReason reason) {}

  // Called to inform the WebWidget of a change in theme.
  // Implementors that cache rendered copies of widgets need to re-render
  // on receiving this message
  virtual void ThemeChanged() {}

  // Do a hit test at given point and return the WebHitTestResult.
  virtual WebHitTestResult HitTestResultAt(const gfx::PointF&) = 0;

  // Called to inform the WebWidget of an input event.
  virtual WebInputEventResult HandleInputEvent(const WebCoalescedInputEvent&) {
    return WebInputEventResult::kNotHandled;
  }

  // Send any outstanding touch events. Touch events need to be grouped together
  // and any changes since the last time a touch event is going to be sent in
  // the new touch event.
  virtual WebInputEventResult DispatchBufferedTouchEvents() {
    return WebInputEventResult::kNotHandled;
  }

  // Called to inform the WebWidget of the mouse cursor's visibility.
  virtual void SetCursorVisibilityState(bool is_visible) {}

  // Called to inform the WebWidget that mouse capture was lost.
  virtual void MouseCaptureLost() {}

  // Called to inform the WebWidget that it has gained or lost keyboard focus.
  virtual void SetFocus(bool) {}

  // Sets the display mode, which comes from the top-level browsing context and
  // is applied to all widgets.
  virtual void SetDisplayMode(mojom::DisplayMode) {}

  // Returns the anchor and focus bounds of the current selection.
  // If the selection range is empty, it returns the caret bounds.
  virtual bool SelectionBounds(WebRect& anchor, WebRect& focus) const {
    return false;
  }

  // Calling WebWidgetClient::requestPointerLock() will result in one
  // return call to didAcquirePointerLock() or didNotAcquirePointerLock().
  virtual void DidAcquirePointerLock() {}
  virtual void DidNotAcquirePointerLock() {}

  // Pointer lock was held, but has been lost. This may be due to a
  // request via WebWidgetClient::requestPointerUnlock(), or for other
  // reasons such as the user exiting lock, window focus changing, etc.
  virtual void DidLosePointerLock() {}

  // Called by client to request showing the context menu.
  virtual void ShowContextMenu(WebMenuSourceType) {}

  // Accessor to the WebWidget scheduing state.
  virtual scheduler::WebRenderWidgetSchedulingState*
  RendererWidgetSchedulingState() = 0;

  // When the WebWidget is part of a frame tree, returns the active url for
  // main frame of that tree, if the main frame is local in that tree. When
  // the WebWidget is of a different kind (e.g. a popup) it returns the active
  // url for the main frame of the frame tree that spawned the WebWidget, if
  // the main frame is local in that tree. When the relevant main frame is
  // remote in that frame tree, then the url is not known, and an empty url is
  // returned.
  virtual WebURL GetURLForDebugTrace() = 0;

  virtual void SetCursor(const ui::Cursor& cursor) = 0;

 protected:
  ~WebWidget() = default;
};

}  // namespace blink

#endif
