/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef WebViewImpl_h
#define WebViewImpl_h

#include <memory>

#include "build/build_config.h"
#include "core/CoreExport.h"
#include "core/exported/WebPagePopupImpl.h"
#include "core/frame/ResizeViewportAnchor.h"
#include "core/page/ChromeClient.h"
#include "core/page/ContextMenuClient.h"
#include "core/page/ContextMenuProvider.h"
#include "core/page/EditorClient.h"
#include "core/page/EventWithHitTestResults.h"
#include "core/page/PageWidgetDelegate.h"
#include "core/page/ScopedPagePauser.h"
#include "platform/animation/CompositorAnimationTimeline.h"
#include "platform/geometry/IntPoint.h"
#include "platform/geometry/IntRect.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/graphics/TouchAction.h"
#include "platform/heap/Handle.h"
#include "platform/scheduler/child/web_scheduler.h"
#include "platform/wtf/Compiler.h"
#include "platform/wtf/HashSet.h"
#include "platform/wtf/StdLibExtras.h"
#include "platform/wtf/Vector.h"
#include "public/platform/WebDisplayMode.h"
#include "public/platform/WebFloatSize.h"
#include "public/platform/WebGestureCurveTarget.h"
#include "public/platform/WebGestureEvent.h"
#include "public/platform/WebInputEvent.h"
#include "public/platform/WebInputEventResult.h"
#include "public/platform/WebLayer.h"
#include "public/platform/WebPoint.h"
#include "public/platform/WebRect.h"
#include "public/platform/WebSize.h"
#include "public/platform/WebString.h"
#include "public/platform/WebVector.h"
#include "public/web/WebNavigationPolicy.h"
#include "public/web/WebPageImportanceSignals.h"
#include "public/web/WebView.h"

namespace blink {

class BrowserControls;
class CompositorAnimationHost;
class DevToolsEmulator;
class Frame;
class FullscreenController;
class LinkHighlightImpl;
class PageOverlay;
class PageScaleConstraintsSet;
class PaintLayerCompositor;
class UserGestureToken;
class ValidationMessageClient;
class WebDevToolsAgentImpl;
class WebElement;
class WebInputMethodController;
class WebLayerTreeView;
class WebLocalFrame;
class WebLocalFrameImpl;
class CompositorMutatorImpl;
class WebRemoteFrame;
class WebSettingsImpl;
class WebViewScheduler;

class CORE_EXPORT WebViewImpl final
    : public WebView,
      public RefCounted<WebViewImpl>,
      public PageWidgetEventHandler,
      public WebScheduler::InterventionReporter,
      public WebViewScheduler::WebViewSchedulerDelegate {
 public:
  static WebViewImpl* Create(WebViewClient*, mojom::PageVisibilityState);
  static HashSet<WebViewImpl*>& AllInstances();
  static const WebInputEvent* CurrentInputEvent();
  // Returns true if popup menus should be rendered by the browser, false if
  // they should be rendered by WebKit (which is the default).
  static bool UseExternalPopupMenus();

  // WebWidget methods:
  void Close() override;
  WebSize Size() override;
  void Resize(const WebSize&) override;
  void ResizeVisualViewport(const WebSize&) override;
  void DidEnterFullscreen() override;
  void DidExitFullscreen() override;

  void SetSuppressFrameRequestsWorkaroundFor704763Only(bool) override;
  void BeginFrame(double last_frame_time_monotonic) override;

  void UpdateAllLifecyclePhases() override;
  void Paint(WebCanvas*, const WebRect&) override;
#if defined(OS_ANDROID)
  void PaintIgnoringCompositing(WebCanvas*, const WebRect&) override;
#endif
  void LayoutAndPaintAsync(WebLayoutAndPaintAsyncCallback*) override;
  void CompositeAndReadbackAsync(
      WebCompositeAndReadbackAsyncCallback*) override;
  void ThemeChanged() override;
  WebInputEventResult HandleInputEvent(const WebCoalescedInputEvent&) override;
  void SetCursorVisibilityState(bool is_visible) override;

  void ApplyViewportDeltas(const WebFloatSize& visual_viewport_delta,
                           const WebFloatSize& layout_viewport_delta,
                           const WebFloatSize& elastic_overscroll_delta,
                           float page_scale_delta,
                           float browser_controls_shown_ratio_delta) override;
  void RecordWheelAndTouchScrollingCount(bool has_scrolled_by_wheel,
                                         bool has_scrolled_by_touch) override;
  void MouseCaptureLost() override;
  void SetFocus(bool enable) override;
  WebColor BackgroundColor() const override;
  WebPagePopupImpl* GetPagePopup() const override;
  bool SelectionBounds(WebRect& anchor, WebRect& focus) const override;
  bool IsAcceleratedCompositingActive() const override;
  void WillCloseLayerTreeView() override;
  void DidAcquirePointerLock() override;
  void DidNotAcquirePointerLock() override;
  void DidLosePointerLock() override;
  void ShowContextMenu(WebMenuSourceType) override;

  // WebView methods:
  virtual bool IsWebView() const { return true; }
  void SetCredentialManagerClient(WebCredentialManagerClient*) override;
  void SetPrerendererClient(WebPrerendererClient*) override;
  WebSettings* GetSettings() override;
  WebString PageEncoding() const override;
  bool TabsToLinks() const override;
  void SetTabsToLinks(bool value) override;
  bool TabKeyCyclesThroughElements() const override;
  void SetTabKeyCyclesThroughElements(bool value) override;
  bool IsActive() const override;
  void SetIsActive(bool value) override;
  void SetDomainRelaxationForbidden(bool, const WebString& scheme) override;
  void SetWindowFeatures(const WebWindowFeatures&) override;
  void SetOpenedByDOM() override;
  void ResizeWithBrowserControls(const WebSize&,
                                 float top_controls_height,
                                 float bottom_controls_height,
                                 bool browser_controls_shrink_layout) override;
  WebFrame* MainFrame() override;
  WebLocalFrame* FocusedFrame() override;
  void SetFocusedFrame(WebFrame*) override;
  void FocusDocumentView(WebFrame*) override;
  void SetInitialFocus(bool reverse) override;
  void ClearFocusedElement() override;
  bool ScrollFocusedEditableElementIntoView() override;
  void SmoothScroll(int target_x, int target_y, long duration_ms) override;
  void ZoomToFindInPageRect(const WebRect&);
  void AdvanceFocus(bool reverse) override;
  void AdvanceFocusAcrossFrames(WebFocusType,
                                WebRemoteFrame* from,
                                WebLocalFrame* to) override;
  double ZoomLevel() override;
  double SetZoomLevel(double) override;
  void ZoomLimitsChanged(double minimum_zoom_level,
                         double maximum_zoom_level) override;
  float TextZoomFactor() override;
  float SetTextZoomFactor(float) override;
  bool ZoomToMultipleTargetsRect(const WebRect&) override;
  float PageScaleFactor() const override;
  void SetDefaultPageScaleLimits(float min_scale, float max_scale) override;
  void SetInitialPageScaleOverride(float) override;
  void SetMaximumLegibleScale(float) override;
  void SetPageScaleFactor(float) override;
  void SetVisualViewportOffset(const WebFloatPoint&) override;
  WebFloatPoint VisualViewportOffset() const override;
  WebFloatSize VisualViewportSize() const override;
  void ResetScrollAndScaleState() override;
  void SetIgnoreViewportTagScaleLimits(bool) override;
  WebSize ContentsPreferredMinimumSize() override;
  void SetDisplayMode(WebDisplayMode) override;

  void SetDeviceScaleFactor(float) override;
  void SetZoomFactorForDeviceScaleFactor(float) override;
  float ZoomFactorForDeviceScaleFactor() override {
    return zoom_factor_for_device_scale_factor_;
  };

  void EnableAutoResizeMode(const WebSize& min_size,
                            const WebSize& max_size) override;
  void DisableAutoResizeMode() override;
  void PerformMediaPlayerAction(const WebMediaPlayerAction&,
                                const WebPoint& location) override;
  void PerformPluginAction(const WebPluginAction&, const WebPoint&) override;
  void AudioStateChanged(bool is_audio_playing) override;
  WebHitTestResult HitTestResultAt(const WebPoint&) override;
  WebHitTestResult HitTestResultForTap(const WebPoint&,
                                       const WebSize&) override;
  unsigned long CreateUniqueIdentifierForRequest() override;
  void EnableDeviceEmulation(const WebDeviceEmulationParams&) override;
  void DisableDeviceEmulation() override;
  void SetSelectionColors(unsigned active_background_color,
                          unsigned active_foreground_color,
                          unsigned inactive_background_color,
                          unsigned inactive_foreground_color) override;
  void PerformCustomContextMenuAction(unsigned action) override;
  void DidCloseContextMenu() override;
  void HidePopups() override;
  void SetPageOverlayColor(WebColor) override;
  WebPageImportanceSignals* PageImportanceSignals() override;
  void SetShowPaintRects(bool) override;
  void SetShowDebugBorders(bool);
  void SetShowFPSCounter(bool) override;
  void SetShowScrollBottleneckRects(bool) override;
  void AcceptLanguagesChanged() override;

  // WebScheduler::InterventionReporter implementation:
  void ReportIntervention(const WebString& message) override;

  // WebViewScheduler::WebViewSchedulerDelegate implementation:
  void RequestBeginMainFrameNotExpected(bool new_state) override;
  void SetPageStopped(bool stopped) override;

  void DidUpdateFullscreenSize();

  float DefaultMinimumPageScaleFactor() const;
  float DefaultMaximumPageScaleFactor() const;
  float MinimumPageScaleFactor() const;
  float MaximumPageScaleFactor() const;
  float ClampPageScaleFactorToLimits(float) const;
  void ResetScaleStateImmediately();

  HitTestResult CoreHitTestResultAt(const WebPoint&);
  void InvalidateRect(const IntRect&);

  void SetBaseBackgroundColor(WebColor);
  void SetBaseBackgroundColorOverride(WebColor);
  void ClearBaseBackgroundColorOverride();
  void SetBackgroundColorOverride(WebColor);
  void ClearBackgroundColorOverride();
  void SetZoomFactorOverride(float);
  void SetCompositorDeviceScaleFactorOverride(float);
  void SetDeviceEmulationTransform(const TransformationMatrix&);
  TransformationMatrix GetDeviceEmulationTransformForTesting() const;

  Color BaseBackgroundColor() const;
  bool BackgroundColorOverrideEnabled() const {
    return background_color_override_enabled_;
  }
  WebColor BackgroundColorOverride() const {
    return background_color_override_;
  }

  Frame* FocusedCoreFrame() const;

  // Returns the currently focused Element or null if no element has focus.
  Element* FocusedElement() const;

  WebViewClient* Client() { return client_; }

  // Returns the page object associated with this view. This may be null when
  // the page is shutting down, but will be valid at all other times.
  Page* GetPage() const { return page_.Get(); }

  // Returns a ValidationMessageClient associated to the Page. This is nullable.
  ValidationMessageClient* GetValidationMessageClient() const;
  WebDevToolsAgentImpl* MainFrameDevToolsAgentImpl();

  DevToolsEmulator* GetDevToolsEmulator() const {
    return dev_tools_emulator_.Get();
  }

  // Returns the main frame associated with this view. This may be null when
  // the page is shutting down, but will be valid at all other times.
  WebLocalFrameImpl* MainFrameImpl() const;

  // Event related methods:
  void MouseContextMenu(const WebMouseEvent&);
  void MouseDoubleClick(const WebMouseEvent&);

  bool StartPageScaleAnimation(const IntPoint& target_position,
                               bool use_anchor,
                               float new_scale,
                               double duration_in_seconds);

  // Handles context menu events orignated via the the keyboard. These
  // include the VK_APPS virtual key and the Shift+F10 combine. Code is
  // based on the Webkit function bool WebView::handleContextMenuEvent(WPARAM
  // wParam, LPARAM lParam) in webkit\webkit\win\WebView.cpp. The only
  // significant change in this function is the code to convert from a
  // Keyboard event to the Right Mouse button down event.
  WebInputEventResult SendContextMenuEvent();

  void ShowContextMenuAtPoint(float x, float y, ContextMenuProvider*);

  void ShowContextMenuForElement(WebElement);

  // Notifies the WebView that a load has been committed. isNewNavigation
  // will be true if a new session history item should be created for that
  // load. isNavigationWithinPage will be true if the navigation does
  // not take the user away from the current page.
  void DidCommitLoad(bool is_new_navigation, bool is_navigation_within_page);

  // Indicates two things:
  //   1) This view may have a new layout now.
  //   2) Calling updateAllLifecyclePhases() is a no-op.
  // After calling WebWidget::updateAllLifecyclePhases(), expect to get this
  // notification unless the view did not need a layout.
  void LayoutUpdated();
  void ResizeAfterLayout();

  void DidChangeContentsSize();
  void PageScaleFactorChanged();
  void MainFrameScrollOffsetChanged();

  bool ShouldAutoResize() const { return should_auto_resize_; }

  IntSize MinAutoSize() const { return min_auto_size_; }

  IntSize MaxAutoSize() const { return max_auto_size_; }

  void UpdateMainFrameLayoutSize();
  void UpdatePageDefinedViewportConstraints(const ViewportDescription&);

  PagePopup* OpenPagePopup(PagePopupClient*);
  void ClosePagePopup(PagePopup*);
  void CleanupPagePopup();
  LocalDOMWindow* PagePopupWindow() const;

  GraphicsLayer* RootGraphicsLayer();
  void RegisterViewportLayersWithCompositor();
  PaintLayerCompositor* Compositor() const;
  CompositorAnimationTimeline* LinkHighlightsTimeline() const {
    return link_highlights_timeline_.get();
  }

  WebViewScheduler* Scheduler() const override;
  void SetVisibilityState(mojom::PageVisibilityState, bool) override;

  bool HasOpenedPopup() const { return page_popup_.get(); }

  // Called by a full frame plugin inside this view to inform it that its
  // zoom level has been updated.  The plugin should only call this function
  // if the zoom change was triggered by the browser, it's only needed in case
  // a plugin can update its own zoom, say because of its own UI.
  void FullFramePluginZoomLevelChanged(double zoom_level);

  void ComputeScaleAndScrollForBlockRect(
      const WebPoint& hit_point,
      const WebRect& block_rect,
      float padding,
      float default_scale_when_already_legible,
      float& scale,
      WebPoint& scroll);
  Node* BestTapNode(const GestureEventWithHitTestResults& targeted_tap_event);
  void EnableTapHighlightAtPoint(
      const GestureEventWithHitTestResults& targeted_tap_event);
  void EnableTapHighlights(HeapVector<Member<Node>>&);
  void ComputeScaleAndScrollForFocusedNode(Node* focused_node,
                                           bool zoom_in_to_legible_scale,
                                           float& scale,
                                           IntPoint& scroll,
                                           bool& need_animation);

  void AnimateDoubleTapZoom(const IntPoint&);

  void ResolveTapDisambiguation(double timestamp_seconds,
                                WebPoint tap_viewport_offset,
                                bool is_long_press) override;

  void EnableFakePageScaleAnimationForTesting(bool);
  bool FakeDoubleTapAnimationPendingForTesting() const {
    return double_tap_zoom_pending_;
  }
  IntPoint FakePageScaleAnimationTargetPositionForTesting() const {
    return fake_page_scale_animation_target_position_;
  }
  float FakePageScaleAnimationPageScaleForTesting() const {
    return fake_page_scale_animation_page_scale_factor_;
  }
  bool FakePageScaleAnimationUseAnchorForTesting() const {
    return fake_page_scale_animation_use_anchor_;
  }

  void EnterFullscreen(LocalFrame&);
  void ExitFullscreen(LocalFrame&);
  void FullscreenElementChanged(Element* old_element, Element* new_element);

  // Exposed for the purpose of overriding device metrics.
  void SendResizeEventAndRepaint();

  // Exposed for testing purposes.
  bool HasHorizontalScrollbar();
  bool HasVerticalScrollbar();

  // Exposed for tests.
  unsigned NumLinkHighlights() { return link_highlights_.size(); }
  LinkHighlightImpl* GetLinkHighlight(int i) {
    return link_highlights_[i].get();
  }

  WebSettingsImpl* SettingsImpl();

  // Returns the bounding box of the block type node touched by the WebPoint.
  WebRect ComputeBlockBound(const WebPoint&, bool ignore_clipping);

  WebLayerTreeView* LayerTreeView() const { return layer_tree_view_; }
  CompositorAnimationHost* AnimationHost() const {
    return animation_host_.get();
  }

  bool MatchesHeuristicsForGpuRasterizationForTesting() const {
    return matches_heuristics_for_gpu_rasterization_;
  }

  void UpdateBrowserControlsState(WebBrowserControlsState constraint,
                                  WebBrowserControlsState current,
                                  bool animate) override;

  BrowserControls& GetBrowserControls();
  // Called anytime browser controls layout height or content offset have
  // changed.
  void DidUpdateBrowserControls();

  void SetOverscrollBehavior(const WebOverscrollBehavior&);

  void ForceNextWebGLContextCreationToFail() override;
  void ForceNextDrawingBufferCreationToFail() override;

  IntSize MainFrameSize();
  WebDisplayMode DisplayMode() const { return display_mode_; }

  PageScaleConstraintsSet& GetPageScaleConstraintsSet() const;

  FloatSize ElasticOverscroll() const { return elastic_overscroll_; }

  class ChromeClient& GetChromeClient() const {
    return *chrome_client_.Get();
  }

  // Returns the currently active WebInputMethodController which is the one
  // corresponding to the focused frame. It will return nullptr if there is no
  // focused frame, or if the there is one but it belongs to a different local
  // root.
  WebInputMethodController* GetActiveWebInputMethodController() const;

  void SetLastHiddenPagePopup(WebPagePopupImpl* page_popup) {
    last_hidden_page_popup_ = page_popup;
  }

  void RequestDecode(const PaintImage&, WTF::Function<void(bool)> callback);

 private:
  void SetPageScaleFactorAndLocation(float, const FloatPoint&);
  void PropagateZoomFactorToLocalFrameRoots(Frame*, float);

  void ScrollAndRescaleViewports(float scale_factor,
                                 const IntPoint& main_frame_origin,
                                 const FloatPoint& visual_viewport_origin);

  float MaximumLegiblePageScale() const;
  void RefreshPageScaleFactorAfterLayout();
  IntSize ContentsSize() const;

  void UpdateICBAndResizeViewport();
  void ResizeViewWhileAnchored(float top_controls_height,
                               float bottom_controls_height,
                               bool browser_controls_shrink_layout);

  // Overrides the compositor visibility. See the description of
  // m_overrideCompositorVisibility for more details.
  void SetCompositorVisibility(bool);

  // TODO(lfg): Remove once WebViewFrameWidget is deleted.
  void ScheduleAnimationForWidget();
  bool GetCompositionCharacterBounds(WebVector<WebRect>&) override;

  void UpdateBaseBackgroundColor();

  friend class WebView;  // So WebView::Create can call our constructor
  friend class WebViewFrameWidget;
  friend class WTF::RefCounted<WebViewImpl>;

  explicit WebViewImpl(WebViewClient*, mojom::PageVisibilityState);
  ~WebViewImpl() override;

  void HideSelectPopup();

  HitTestResult HitTestResultForRootFramePos(const LayoutPoint&);

  void ConfigureAutoResizeMode();

  void InitializeLayerTreeView();

  void SetIsAcceleratedCompositingActive(bool);
  void DoComposite();
  void ReallocateRenderer();
  void UpdateLayerTreeViewport();
  void UpdateLayerTreeBackgroundColor();
  void UpdateDeviceEmulationTransform();
  void UpdateLayerTreeDeviceScaleFactor();

  // Helper function: Widens the width of |source| by the specified margins
  // while keeping it smaller than page width.
  WebRect WidenRectWithinPageBounds(const WebRect& source,
                                    int target_margin,
                                    int minimum_margin);

  // PageWidgetEventHandler functions
  void HandleMouseLeave(LocalFrame&, const WebMouseEvent&) override;
  void HandleMouseDown(LocalFrame&, const WebMouseEvent&) override;
  void HandleMouseUp(LocalFrame&, const WebMouseEvent&) override;
  WebInputEventResult HandleMouseWheel(LocalFrame&,
                                       const WebMouseWheelEvent&) override;
  WebInputEventResult HandleGestureEvent(const WebGestureEvent&) override;
  WebInputEventResult HandleKeyEvent(const WebKeyboardEvent&) override;
  WebInputEventResult HandleCharEvent(const WebKeyboardEvent&) override;

  WebInputEventResult HandleSyntheticWheelFromTouchpadPinchEvent(
      const WebGestureEvent&);

  void EnablePopupMouseWheelEventListener(WebLocalFrameImpl* local_root);
  void DisablePopupMouseWheelEventListener();

  void CancelPagePopup();
  void UpdatePageOverlays();

  float DeviceScaleFactor() const;

  void SetRootGraphicsLayer(GraphicsLayer*);
  void SetRootLayer(WebLayer*);
  void AttachCompositorAnimationTimeline(CompositorAnimationTimeline*);
  void DetachCompositorAnimationTimeline(CompositorAnimationTimeline*);

  LocalFrame* FocusedLocalFrameInWidget() const;
  LocalFrame* FocusedLocalFrameAvailableForIme() const;

  CompositorMutatorImpl& Mutator();
  CompositorMutatorImpl* CompositorMutator();

  WebViewClient* client_;  // Can be 0 (e.g. unittests, shared workers, etc.)

  Persistent<ChromeClient> chrome_client_;
  ContextMenuClient context_menu_client_;
  EditorClient editor_client_;

  WebSize size_;
  // If true, automatically resize the layout view around its content.
  bool should_auto_resize_;
  // The lower bound on the size when auto-resizing.
  IntSize min_auto_size_;
  // The upper bound on the size when auto-resizing.
  IntSize max_auto_size_;

  Persistent<Page> page_;

  // An object that can be used to manipulate m_page->settings() without linking
  // against WebCore. This is lazily allocated the first time GetWebSettings()
  // is called.
  std::unique_ptr<WebSettingsImpl> web_settings_;

  // Keeps track of the current zoom level. 0 means no zoom, positive numbers
  // mean zoom in, negative numbers mean zoom out.
  double zoom_level_;

  double minimum_zoom_level_;

  double maximum_zoom_level_;

  // Additional zoom factor used to scale the content by device scale factor.
  double zoom_factor_for_device_scale_factor_;

  // This value, when multiplied by the font scale factor, gives the maximum
  // page scale that can result from automatic zooms.
  float maximum_legible_scale_;

  // The scale moved to by the latest double tap zoom, if any.
  float double_tap_zoom_page_scale_factor_;
  // Have we sent a double-tap zoom and not yet heard back the scale?
  bool double_tap_zoom_pending_;

  // Used for testing purposes.
  bool enable_fake_page_scale_animation_for_testing_;
  IntPoint fake_page_scale_animation_target_position_;
  float fake_page_scale_animation_page_scale_factor_;
  bool fake_page_scale_animation_use_anchor_;

  float compositor_device_scale_factor_override_;
  TransformationMatrix device_emulation_transform_;

  // Webkit expects keyPress events to be suppressed if the associated keyDown
  // event was handled. Safari implements this behavior by peeking out the
  // associated WM_CHAR event if the keydown was handled. We emulate
  // this behavior by setting this flag if the keyDown was handled.
  bool suppress_next_keypress_event_;

  // TODO(ekaramad): Can we remove this and make sure IME events are not called
  // when there is no page focus?
  // Represents whether or not this object should process incoming IME events.
  bool ime_accept_events_;

  // The popup associated with an input/select element.
  scoped_refptr<WebPagePopupImpl> page_popup_;

  // This stores the last hidden page popup. If a GestureTap attempts to open
  // the popup that is closed by its previous GestureTapDown, the popup remains
  // closed.
  scoped_refptr<WebPagePopupImpl> last_hidden_page_popup_;

  Persistent<DevToolsEmulator> dev_tools_emulator_;
  std::unique_ptr<PageOverlay> page_color_overlay_;

  // Whether the user can press tab to focus links.
  bool tabs_to_links_;

  // If set, the (plugin) node which has mouse capture.
  Persistent<Node> mouse_capture_node_;
  scoped_refptr<UserGestureToken> mouse_capture_gesture_token_;

  WebLayerTreeView* layer_tree_view_;
  std::unique_ptr<CompositorAnimationHost> animation_host_;

  WebLayer* root_layer_;
  GraphicsLayer* root_graphics_layer_;
  GraphicsLayer* visual_viewport_container_layer_;
  bool matches_heuristics_for_gpu_rasterization_;

  Vector<std::unique_ptr<LinkHighlightImpl>> link_highlights_;
  std::unique_ptr<CompositorAnimationTimeline> link_highlights_timeline_;
  std::unique_ptr<FullscreenController> fullscreen_controller_;

  WebPoint last_tap_disambiguation_best_candidate_position_;

  WebColor base_background_color_;
  bool base_background_color_override_enabled_;
  WebColor base_background_color_override_;
  bool background_color_override_enabled_;
  WebColor background_color_override_;
  float zoom_factor_override_;

  bool should_dispatch_first_visually_non_empty_layout_;
  bool should_dispatch_first_layout_after_finished_parsing_;
  bool should_dispatch_first_layout_after_finished_loading_;
  WebDisplayMode display_mode_;

  FloatSize elastic_overscroll_;

  // This is owned by the LayerTreeHostImpl, and should only be used on the
  // compositor thread. The LayerTreeHostImpl is indirectly owned by this
  // class so this pointer should be valid until this class is destructed.
  CrossThreadPersistent<CompositorMutatorImpl> mutator_;

  Persistent<EventListener> popup_mouse_wheel_event_listener_;

  // The local root whose document has |popup_mouse_wheel_event_listener_|
  // registered.
  WeakPersistent<WebLocalFrameImpl> local_root_with_empty_mouse_wheel_listener_;

  WebPageImportanceSignals page_importance_signals_;

  const std::unique_ptr<WebViewScheduler> scheduler_;

  // TODO(lfg): This is used in order to disable compositor visibility while
  // the page is still visible. This is needed until the WebView and WebWidget
  // split is complete, since in out-of-process iframes the page can be
  // visible, but the WebView should not be used as a widget.
  bool override_compositor_visibility_;

  Persistent<ResizeViewportAnchor> resize_viewport_anchor_;

  static const WebInputEvent* current_input_event_;
};

// We have no ways to check if the specified WebView is an instance of
// WebViewImpl because WebViewImpl is the only implementation of WebView.
DEFINE_TYPE_CASTS(WebViewImpl, WebView, webView, true, true);

}  // namespace blink

#endif
