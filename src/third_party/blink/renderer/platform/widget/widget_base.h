// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WIDGET_WIDGET_BASE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WIDGET_WIDGET_BASE_H_

#include "base/time/time.h"
#include "cc/paint/element_id.h"
#include "cc/trees/browser_controls_params.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/common/metrics/document_update_reason.h"
#include "third_party/blink/public/common/page/content_to_visible_time_reporter.h"
#include "third_party/blink/public/mojom/input/input_handler.mojom-blink.h"
#include "third_party/blink/public/mojom/page/record_content_to_visible_time_request.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/page/widget.mojom-blink.h"
#include "third_party/blink/public/platform/cross_variant_mojo_util.h"
#include "third_party/blink/public/platform/web_text_input_info.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/widget/compositing/layer_tree_view_delegate.h"
#include "third_party/blink/renderer/platform/widget/input/widget_base_input_handler.h"
#include "ui/base/ime/text_input_mode.h"
#include "ui/base/ime/text_input_type.h"

namespace cc {
class AnimationHost;
class LayerTreeHost;
class LayerTreeSettings;
class TaskGraphRunner;
class UkmRecorderFactory;
}  // namespace cc

namespace ui {
class Cursor;
}

namespace blink {
class ImeEventGuard;
class LayerTreeView;
class WidgetBaseClient;
class WidgetInputHandlerManager;
class WidgetCompositor;

namespace scheduler {
class WebRenderWidgetSchedulingState;
class WebThreadScheduler;
}

// This class is the foundational class for all widgets that blink creates.
// (WebPagePopupImpl, WebFrameWidgetBase) will contain an instance of this
// class. For simplicity purposes this class will be a member of those classes.
//
// Co-orindates handled in this class can be in the "blink coordinate space"
// which is scaled DSF baked in if UseZoomForDSF is enabled, otherwise they
// are equivalent to DIPs.
class PLATFORM_EXPORT WidgetBase : public mojom::blink::Widget,
                                   public LayerTreeViewDelegate {
 public:
  WidgetBase(
      WidgetBaseClient* client,
      CrossVariantMojoAssociatedRemote<mojom::WidgetHostInterfaceBase>
          widget_host,
      CrossVariantMojoAssociatedReceiver<mojom::WidgetInterfaceBase> widget,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      bool hidden,
      bool never_composited,
      bool is_for_child_local_root);
  ~WidgetBase() override;

  // Initialize the compositor. |settings| is typically null. When |settings| is
  // null the default settings will be used, tests may provide a |settings|
  // object to override the defaults.
  void InitializeCompositing(
      scheduler::WebThreadScheduler* main_thread_scheduler,
      cc::TaskGraphRunner* task_graph_runner,
      bool for_child_local_root_frame,
      const ScreenInfo& screen_info,
      std::unique_ptr<cc::UkmRecorderFactory> ukm_recorder_factory,
      const cc::LayerTreeSettings* settings);

  // Shutdown the compositor.
  void Shutdown(scoped_refptr<base::SingleThreadTaskRunner> cleanup_runner);

  // Set the compositor as visible. If |visible| is true, then the compositor
  // will request a new layer frame sink, begin producing frames from the
  // compositor scheduler, and in turn will update the document lifecycle.
  void SetCompositorVisible(bool visible);

  void AddPresentationCallback(
      uint32_t frame_token,
      base::OnceCallback<void(base::TimeTicks)> callback);

  // mojom::blink::Widget overrides:
  void ForceRedraw(mojom::blink::Widget::ForceRedrawCallback callback) override;
  void GetWidgetInputHandler(
      mojo::PendingReceiver<mojom::blink::WidgetInputHandler> request,
      mojo::PendingRemote<mojom::blink::WidgetInputHandlerHost> host) override;
  void UpdateVisualProperties(
      const VisualProperties& visual_properties) override;
  void UpdateScreenRects(const gfx::Rect& widget_screen_rect,
                         const gfx::Rect& window_screen_rect,
                         UpdateScreenRectsCallback callback) override;
  void WasHidden() override;
  void WasShown(base::TimeTicks show_request_timestamp,
                bool was_evicted,
                mojom::blink::RecordContentToVisibleTimeRequestPtr
                    record_tab_switch_time_request) override;

  // LayerTreeDelegate overrides:
  // Applies viewport related properties during a commit from the compositor
  // thread.
  void ApplyViewportChanges(const cc::ApplyViewportChangesArgs& args) override;
  void RecordManipulationTypeCounts(cc::ManipulationInfo info) override;
  void SendOverscrollEventFromImplSide(
      const gfx::Vector2dF& overscroll_delta,
      cc::ElementId scroll_latched_element_id) override;
  void SendScrollEndEventFromImplSide(
      cc::ElementId scroll_latched_element_id) override;
  void BeginMainFrame(base::TimeTicks frame_time) override;
  void OnDeferMainFrameUpdatesChanged(bool) override;
  void OnDeferCommitsChanged(bool) override;
  void DidBeginMainFrame() override;
  void RequestNewLayerTreeFrameSink(
      LayerTreeFrameSinkCallback callback) override;
  void DidCommitAndDrawCompositorFrame() override;
  void DidObserveFirstScrollDelay(
      base::TimeDelta first_scroll_delay,
      base::TimeTicks first_scroll_timestamp) override;
  void WillCommitCompositorFrame() override;
  void DidCommitCompositorFrame(base::TimeTicks commit_start_time) override;
  void DidCompletePageScaleAnimation() override;
  void RecordStartOfFrameMetrics() override;
  void RecordEndOfFrameMetrics(
      base::TimeTicks frame_begin_time,
      cc::ActiveFrameSequenceTrackers trackers) override;
  std::unique_ptr<cc::BeginMainFrameMetrics> GetBeginMainFrameMetrics()
      override;
  void BeginUpdateLayers() override;
  void EndUpdateLayers() override;
  void UpdateVisualState() override;
  void WillBeginMainFrame() override;
  void RunPaintBenchmark(int repeat_count,
                         cc::PaintBenchmarkResult& result) override;

  cc::AnimationHost* AnimationHost() const;
  cc::LayerTreeHost* LayerTreeHost() const;
  scheduler::WebRenderWidgetSchedulingState* RendererWidgetSchedulingState()
      const;

  mojom::blink::WidgetHost* GetWidgetHostRemote() { return widget_host_.get(); }

  // Returns if we should gather begin main frame metrics. If there is no
  // compositor thread this returns false.
  static bool ShouldRecordBeginMainFrameMetrics();

  // Set the current cursor relay to browser if necessary.
  void SetCursor(const ui::Cursor& cursor);

  // Dispatch the virtual keyboard and update text input state.
  void ShowVirtualKeyboardOnElementFocus();

  // Process the touch action.
  void ProcessTouchAction(cc::TouchAction touch_action);

  WidgetBaseInputHandler& input_handler() { return input_handler_; }

  WidgetInputHandlerManager* widget_input_handler_manager() {
    return widget_input_handler_manager_.get();
  }

  gfx::Rect CompositorViewportRect() const;

  WidgetBaseClient* client() { return client_; }

  void SetToolTipText(const String& tooltip_text, TextDirection dir);

  void ShowVirtualKeyboard();
  void UpdateSelectionBounds();
  void UpdateTextInputState();
  void ClearTextInputState();
  void ForceTextInputStateUpdate();
  void RequestCompositionUpdates(bool immediate_request, bool monitor_updates);
  void UpdateCompositionInfo(bool immediate_request);
  void SetFocus(bool enable);
  bool has_focus() const { return has_focus_; }
  void MouseCaptureLost();
  void CursorVisibilityChange(bool is_visible);
  void QueueSyntheticEvent(
      std::unique_ptr<blink::WebCoalescedInputEvent> event);
  void SetEditCommandsForNextKeyEvent(
      Vector<mojom::blink::EditCommandPtr> edit_commands);
  void SetMouseCapture(bool capture);
  void ImeSetComposition(const String& text,
                         const Vector<ui::ImeTextSpan>& ime_text_spans,
                         const gfx::Range& replacement_range,
                         int selection_start,
                         int selection_end);
  void ImeCommitText(const String& text,
                     const Vector<ui::ImeTextSpan>& ime_text_spans,
                     const gfx::Range& replacement_range,
                     int relative_cursor_pos);
  void ImeFinishComposingText(bool keep_selection);
  bool IsForProvisionalFrame();
  void FlushInputProcessedCallback();
  void CancelCompositionForPepper();

  void RequestPresentationAfterScrollAnimationEnd(
      mojom::blink::Widget::ForceRedrawCallback callback);

  void OnImeEventGuardStart(ImeEventGuard* guard);
  void OnImeEventGuardFinish(ImeEventGuard* guard);

  bool is_hidden() const { return is_hidden_; }
  void set_is_pasting(bool value) { is_pasting_ = value; }
  bool is_pasting() const { return is_pasting_; }
  void set_handling_select_range(bool value) { handling_select_range_ = value; }
  bool handling_select_range() const { return handling_select_range_; }

  bool ComputePreferCompositingToLCDText();

  const viz::LocalSurfaceId& local_surface_id_from_parent() {
    return local_surface_id_from_parent_;
  }

  // Called to get the position of the widget's window in screen
  // coordinates. Note, the window includes any decorations such as borders,
  // scrollbars, URL bar, tab strip, etc. if they exist.
  gfx::Rect WindowRect();

  // Called to get the view rect in screen coordinates. This is the actual
  // content view area, i.e. doesn't include any window decorations.
  gfx::Rect ViewRect();

  // Sets the pending window rects (in screen coordinates). This is used because
  // the window rect is delivered asynchronously to the browser. Pass in nullptr
  // to clear the pending window rect once the browser has acknowledged the
  // request.
  void SetPendingWindowRect(const gfx::Rect& rect);

  // Must correspond with a previous call to SetPendingWindowRect.
  void AckPendingWindowRect();

  // Returns the location/bounds of the widget (in screen coordinates).
  const gfx::Rect& WidgetScreenRect() const { return widget_screen_rect_; }

  // Returns the bounds of the screen the widget is contained in (in screen
  // coordinates).
  const gfx::Rect& WindowScreenRect() const { return window_screen_rect_; }

  // Sets the screen rects (in screen coordinates).
  void SetScreenRects(const gfx::Rect& widget_screen_rect,
                      const gfx::Rect& window_screen_rect);

  // Returns the visible viewport size.
  const gfx::Size& VisibleViewportSizeInDIPs() const {
    return visible_viewport_size_in_dips_;
  }

  // Set the visible viewport size.
  void SetVisibleViewportSizeInDIPs(const gfx::Size& size) {
    visible_viewport_size_in_dips_ = size;
  }

  // Converts from DIPs to Blink coordinate space (ie. Viewport/Physical
  // pixels).
  gfx::PointF DIPsToBlinkSpace(const gfx::PointF& point);
  gfx::Point DIPsToRoundedBlinkSpace(const gfx::Point& point);
  gfx::Size DIPsToCeiledBlinkSpace(const gfx::Size& size);
  gfx::RectF DIPsToBlinkSpace(const gfx::RectF& rect);
  float DIPsToBlinkSpace(float scalar);

  // Converts from Blink coordinate (ie. Viewport/Physical pixels) space to
  // DIPs.
  gfx::PointF BlinkSpaceToDIPs(const gfx::PointF& point);
  gfx::Point BlinkSpaceToFlooredDIPs(const gfx::Point& point);
  gfx::Size BlinkSpaceToFlooredDIPs(const gfx::Size& size);
  gfx::Rect BlinkSpaceToEnclosedDIPs(const gfx::Rect& rect);
  gfx::RectF BlinkSpaceToDIPs(const gfx::RectF& rectF);

  // Returns whether Zoom for DSF is enabled for the widget.
  bool UseZoomForDsf() { return use_zoom_for_dsf_; }

  void BindWidgetCompositor(
      mojo::PendingReceiver<mojom::blink::WidgetCompositor> receiver);

  base::WeakPtr<WidgetBase> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // Update the surface allocation information, compositor viewport rect and
  // screen info on the widget.
  void UpdateSurfaceAndScreenInfo(
      const viz::LocalSurfaceId& new_local_surface_id,
      const gfx::Rect& compositor_viewport_pixel_rect,
      const ScreenInfo& new_screen_info);
  // Similar to UpdateSurfaceAndScreenInfo but the screen info remains the same.
  void UpdateSurfaceAndCompositorRect(
      const viz::LocalSurfaceId& new_local_surface_id,
      const gfx::Rect& compositor_viewport_pixel_rect);
  // Similar to UpdateSurfaceAndScreenInfo but the surface allocation
  // and compositor viewport rect remains the same.
  void UpdateScreenInfo(const ScreenInfo& new_screen_info);
  // Similar to UpdateSurfaceAndScreenInfo but the surface allocation
  // remains the same.
  void UpdateCompositorViewportAndScreenInfo(
      const gfx::Rect& compositor_viewport_pixel_rect,
      const ScreenInfo& new_screen_info);
  // Similar to UpdateSurfaceAndScreenInfo but the surface allocation and screen
  // info remains the same.
  void UpdateCompositorViewportRect(
      const gfx::Rect& compositor_viewport_pixel_rect);
  const ScreenInfo& GetScreenInfo();

  const viz::LocalSurfaceId& local_surface_id_from_parent() const {
    return local_surface_id_from_parent_;
  }

 private:
  bool CanComposeInline();
  void UpdateTextInputStateInternal(bool show_virtual_keyboard,
                                    bool immediate_request);
  bool ShouldHandleImeEvents();
  // Returns the range of the text that is being composed or the selection if
  // the composition does not exist.
  void GetCompositionRange(gfx::Range* range);
  void GetCompositionCharacterBounds(Vector<gfx::Rect>* bounds);
  ui::TextInputType GetTextInputType();

  // Returns true if the composition range or composition character bounds
  // should be sent to the browser process.
  bool ShouldUpdateCompositionInfo(const gfx::Range& range,
                                   const Vector<gfx::Rect>& bounds);

  // Sets the "hidden" state of this widget.  All modification of is_hidden_
  // should use this method so that we can properly inform the RenderThread of
  // our state.
  void SetHidden(bool hidden);

  std::unique_ptr<LayerTreeView> layer_tree_view_;
  scoped_refptr<WidgetInputHandlerManager> widget_input_handler_manager_;
  WidgetBaseClient* client_;
  mojo::AssociatedRemote<mojom::blink::WidgetHost> widget_host_;
  mojo::AssociatedReceiver<mojom::blink::Widget> receiver_;
  std::unique_ptr<scheduler::WebRenderWidgetSchedulingState>
      render_widget_scheduling_state_;
  bool has_focus_ = false;
  WidgetBaseInputHandler input_handler_{this};
  scoped_refptr<WidgetCompositor> widget_compositor_;

  // Stores the current selection bounds.
  gfx::Rect selection_focus_rect_;
  gfx::Rect selection_anchor_rect_;

  // Stores the current composition character bounds.
  Vector<gfx::Rect> composition_character_bounds_;

  // Stores the current composition range.
  gfx::Range composition_range_ = gfx::Range::InvalidRange();

  // True if the IME requests updated composition info.
  bool monitor_composition_info_ = false;

  // Stores information about the current text input.
  blink::WebTextInputInfo text_input_info_;

  // Stores the current text input type of |webwidget_|.
  ui::TextInputType text_input_type_ = ui::TEXT_INPUT_TYPE_NONE;

  // Stores the current text input mode of |webwidget_|.
  ui::TextInputMode text_input_mode_ = ui::TEXT_INPUT_MODE_DEFAULT;

  // Stores the current virtualkeyboardpolicy of |webwidget_|.
  ui::mojom::VirtualKeyboardPolicy vk_policy_ =
      ui::mojom::VirtualKeyboardPolicy::AUTO;

  // Stores the current text input flags of |webwidget_|.
  int text_input_flags_ = 0;

  // Indicates whether currently focused input field has next/previous focusable
  // form input field.
  int next_previous_flags_;

  // Stores the current type of composition text rendering of |webwidget_|.
  bool can_compose_inline_ = true;

  // Stores whether the IME should always be hidden for |webwidget_|.
  bool always_hide_ime_ = false;

  // Used to inform didChangeSelection() when it is called in the context
  // of handling a FrameInputHandler::SelectRange IPC.
  bool handling_select_range_ = false;

  // Whether or not this RenderWidget is currently pasting.
  bool is_pasting_ = false;

  // Object to record tab switch time into this RenderWidget
  ContentToVisibleTimeReporter tab_switch_time_recorder_;

  // Properties of the screen hosting the WidgetBase. Rects in this structure
  // do not include any scaling by device scale factor, so are logical pixels
  // not physical device pixels.
  ScreenInfo screen_info_;
  viz::LocalSurfaceId local_surface_id_from_parent_;

  // It is possible that one ImeEventGuard is nested inside another
  // ImeEventGuard. We keep track of the outermost one, and update it as needed.
  ImeEventGuard* ime_event_guard_ = nullptr;

  // The screen rects of the view and the window that contains it. These do not
  // include any scaling by device scale factor, so are logical pixels not
  // physical device pixels.
  gfx::Rect widget_screen_rect_;
  gfx::Rect window_screen_rect_;

  // While we are waiting for the browser to update window sizes, we track the
  // pending size temporarily.
  int pending_window_rect_count_ = 0;

  // A pending window rect that is inflight and hasn't been acknowledged by the
  // browser yet. This should only be set if |pending_window_rect_count_| is
  // non-zero.
  base::Optional<gfx::Rect> pending_window_rect_;

  // The size of the visible viewport (in DIPs).
  // TODO(dtapuska): Figure out if we can change this to Blink Space.
  // See https://crbug.com/1131389
  gfx::Size visible_viewport_size_in_dips_;

  const bool use_zoom_for_dsf_;

  // Indicates that we shouldn't bother generated paint events.
  bool is_hidden_;

  // Indicates that we are never visible, so never produce graphical output.
  const bool never_composited_;

  // Indicates this is for a child local root.
  const bool is_for_child_local_root_;

  // The task runner on the main thread used for compositor tasks.
  scoped_refptr<base::SingleThreadTaskRunner>
      main_thread_compositor_task_runner_;

  base::WeakPtrFactory<WidgetBase> weak_ptr_factory_{this};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WIDGET_WIDGET_BASE_H_
