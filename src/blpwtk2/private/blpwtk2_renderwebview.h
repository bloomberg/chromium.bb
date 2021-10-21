/*
 * Copyright (C) 2018 Bloomberg Finance L.P.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS," WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef INCLUDED_BLPWTK2_RENDERWEBVIEW_H
#define INCLUDED_BLPWTK2_RENDERWEBVIEW_H

#include <blpwtk2.h>
#include <blpwtk2_config.h>

#include <blpwtk2_dragdrop.h>
#include <blpwtk2_rendermessagedelegate.h>
#include <blpwtk2_scopedhwnd.h>
#include <blpwtk2_webview.h>
#include <blpwtk2_webviewdelegate.h>
#include <blpwtk2_webviewproperties.h>
#include <blpwtk2_webviewproxydelegate.h>

#include <base/i18n/rtl.h>
#include <base/optional.h>
#include <content/browser/renderer_host/input/fling_controller.h>
#include <content/browser/renderer_host/input/input_disposition_handler.h>
#include <content/browser/renderer_host/input/input_router_impl.h>
#include <content/browser/renderer_host/input/input_router_client.h>

#include <content/common/cursors/webcursor.h>
#include <content/public/common/input_event_ack_state.h>
#include <ipc/ipc_listener.h>

#include <third_party/blink/public/mojom/page/widget.mojom.h>
//#include "third_party/blink/public/mojom/page/widget.mojom-blink.h"
#include <third_party/blink/public/mojom/input/pointer_lock_context.mojom.h>
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

#include <ui/base/cursor/cursor_loader.h>
#include <ui/base/ime/input_method_delegate.h>
#include <ui/base/ime/text_input_client.h>
#include <ui/gfx/geometry/rect.h>
#include <ui/gfx/selection_bound.h>
#include <ui/views/corewm/tooltip_win.h>

#include <functional>

namespace blink {
class WebFrameWidget;
}  // close namespace blink

namespace content {
class InputRouterImpl;
}  // close namespace content

namespace views {
namespace corewm {
class Tooltip;
}  // close namespace corewm
}  // close namespace views

namespace ui {
class CursorLoader;
class InputMethod;
class SessionChangeObserver;

#if defined(BLPWTK2_FEATURE_RUBBERBAND)
class RubberbandOutline;
#endif
}  // close namespace ui

struct WidgetHostMsg_SelectionBounds_Params;

namespace blpwtk2 {

class ProfileImpl;
class RenderCompositor;
class MojoLocalFrameHostImpl;
class MojoFrameWidgetHostImpl;
class MojoPopupWidgetHostImpl;

                        // ===================
                        // class RenderWebView
                        // ===================

class RenderWebView final : public WebView
                          , public WebViewDelegate
                          , private WebViewProxyDelegate
                          , private IPC::Listener
                          , private content::InputRouterImplClient
                          , private content::InputDispositionHandler
                          , private content::FlingControllerSchedulerClient
                          , private ui::internal::InputMethodDelegate
                          , private ui::TextInputClient
                          , private DragDropDelegate
                          , private blink::mojom::WidgetHost
                          , private blink::mojom::PointerLockContext
{
    class RenderViewObserver;
    friend class MojoLocalFrameHostImpl;
    friend class MojoFrameWidgetHostImpl;
    friend class MojoPopupWidgetHostImpl;

    // DATA
    WebView *d_proxy;
    WebViewDelegate *d_delegate;
    ProfileImpl *d_profile;
#if defined(BLPWTK2_FEATURE_FOCUS) || defined(BLPWTK2_FEATURE_REROUTEMOUSEWHEEL)
    WebViewProperties d_properties;
#endif

    bool d_pendingDestroy;

    ScopedHWND d_hwnd;

    bool d_hasParent = false;
    bool d_shown = false, d_visible = false;
    gfx::Rect d_geometry;
    bool d_is_fullscreen_entered = false;

    // Track the 'content::RenderWidget':
    bool d_gotRenderViewInfo = false;
    base::Optional<int> d_renderViewRoutingId, d_mainFrameRoutingId;
    RenderViewObserver *d_renderViewObserver = nullptr;

    // The compositor:
    std::unique_ptr<RenderCompositor> d_compositor;

    // State related to processing user input:
    //
    // The input event router:
    std::unique_ptr<content::InputRouterImpl> d_inputRouterImpl;
    mojo::Remote<blink::mojom::WidgetInputHandler> d_widgetInputHandler;

    // State related to mouse events:
    gfx::Point d_mouseScreenPosition;
    gfx::Point d_unlockedMouseScreenPosition, d_unlockedMouseWebViewPosition;
    bool d_ncHitTestEnabled = false;
    int d_ncHitTestResult = 0;
    HRGN d_ncHitTestRegion = NULL;
    bool d_mousePressed = false;
    bool d_mouseEntered = false, d_mouseLocked = false;

    // State related to cursor management:
    content::WebCursor d_currentCursor;
    std::unique_ptr<ui::CursorLoader> d_cursorLoader;
    bool d_isCursorOverridden = false;
    HCURSOR d_currentPlatformCursor = NULL, d_previousPlatformCursor = NULL;

    // State related to mouse wheel events:
    base::OneShotTimer d_mouseWheelEndDispatchTimer;
    blink::WebMouseWheelEvent d_lastMouseWheelEvent;
    gfx::Vector2dF d_firstWheelLocation;
    blink::WebMouseWheelEvent d_initialWheelEvent;
    enum class FirstScrollUpdateAckState {
        kNotArrived = 0,
        kConsumed,
        kNotConsumed,
    } d_firstScrollUpdateAckState = FirstScrollUpdateAckState::kNotArrived;

    // State related to keyboard events:
    bool d_focused = false;
    std::unique_ptr<ui::InputMethod> d_inputMethod;
    gfx::Range d_composition_range;
    std::vector<gfx::Rect> d_compositionCharacterBounds;
    bool d_hasCompositionText = false;
    ui::mojom::TextInputStatePtr d_textInputState;
    gfx::SelectionBound d_selectionAnchor, d_selectionFocus;
    std::u16string d_selectionText;
    std::size_t d_selectionTextOffset = 0;
    gfx::Range d_selectionRange;

    // State related to drag-and-drop:
    std::unique_ptr<ui::CursorFactory> d_cursorFactory;
    scoped_refptr<DragDrop> d_dragDrop;

    // State related to displaying native tooltips:
    std::u16string d_tooltipText, d_lastTooltipText, d_tooltipTextAtMousePress;
    std::unique_ptr<views::corewm::Tooltip> d_tooltip;

    base::OneShotTimer d_tooltipDeferTimer;
        // Timer for requesting delayed updates of the tooltip.

    base::OneShotTimer d_tooltipShownTimer;
        // Timer to timeout the life of an on-screen tooltip. We hide the tooltip
        // when this timer fires.

    // Observe Windows 'session changes':
    std::unique_ptr<ui::SessionChangeObserver> d_windowsSessionChangeObserver;

    RenderMessageDelegate msg_delegate_;

    // One side of a pipe that is held open while the pointer is locked.
    // The other side is held be the renderer.
    mojo::Receiver<blink::mojom::PointerLockContext> d_mouse_lock_context{this};
    mojo::AssociatedReceiver<blink::mojom::WidgetHost> d_blink_widget_host_receiver{this};
    blink::WebFrameWidget *d_frame_widget = nullptr;
    mojo::AssociatedRemote<blink::mojom::FrameWidget> blink_frame_widget_;
    mojo::AssociatedRemote<blink::mojom::Widget> blink_widget_;
    std::unique_ptr<MojoLocalFrameHostImpl> mojo_local_frame_host_impl_;
    std::unique_ptr<MojoFrameWidgetHostImpl> mojo_frame_widget_host_impl_;
    std::unique_ptr<MojoPopupWidgetHostImpl> mojo_popup_widget_host_impl_;

    // For popup webview
    std::function<void()> callback_after_renderer_inited_;

#if defined(BLPWTK2_FEATURE_RUBBERBAND)
    // State related to rubber band selection:
    bool d_enableAltDragRubberBanding = false;
    std::unique_ptr<ui::RubberbandOutline> d_rubberbandOutline;
#endif

    base::WeakPtrFactory<RenderWebView> d_weak_factory{this};

    // blpwtk2::WebView overrides
    void destroy() override;
    WebFrame *mainFrame() override;
    int loadUrl(const StringRef& url) override;
    void loadInspector(unsigned int pid, int routingId) override;
    void inspectElementAt(const POINT& point) override;
    int goBack() override;
    int goForward() override;
    int reload() override;
    void stop() override;
#if defined(BLPWTK2_FEATURE_FOCUS)
    void takeKeyboardFocus() override;
    void setLogicalFocus(bool focused) override;
#endif
    void show() override;
    void hide() override;
    int  setParent(NativeView parent) override;
    void move(int left, int top, int width, int height) override;
    void cutSelection() override;
    void copySelection() override;
    void paste() override;
    void deleteSelection() override;
    void enableNCHitTest(bool enabled) override;
    void onNCHitTestResult(int x, int y, int result) override;
#if defined(BLPWTK2_FEATURE_FULLSCREEN_MODE)
    void onEnterFullscreenModeResult(bool isFullscreen) override;
#endif
    void setNCHitTestRegion(NativeRegion region) override;
    void performCustomContextMenuAction(int actionId) override;
#if defined(BLPWTK2_FEATURE_RUBBERBAND)
    void enableAltDragRubberbanding(bool enabled) override;
    bool forceStartRubberbanding(int x, int y) override;
    bool isRubberbanding() const override;
    void abortRubberbanding() override;
    String getTextInRubberband(const NativeRect&) override;
    void SetRubberbandRect(const ::gfx::Rect& rect) override;
    void HideRubberbandRect() override;
#endif
    void find(const StringRef& text, bool matchCase, bool forward) override;
    void stopFind(bool preserveSelection) override;
    void replaceMisspelledRange(const StringRef& text) override;
    void rootWindowPositionChanged() override;
    void rootWindowSettingsChanged() override;

    void handleInputEvents(const InputEvent *events,
                           size_t            eventsCount) override;
    void setDelegate(WebViewDelegate *delegate) override;
#if defined(BLPWTK2_FEATURE_SCREENPRINT)
    void drawContentsToBlob(Blob *blob, const DrawParams& params) override;
#endif
    int getRoutingId() const override;
    void setBackgroundColor(NativeColor color) override;
    void setRegion(NativeRegion region) override;
#if defined(BLPWTK2_FEATURE_KEYBOARD_LAYOUT)
    void activateKeyboardLayout(unsigned int hkl) override;
#endif
    void clearTooltip() override;
#if defined(BLPWTK2_FEATURE_DWM)
    void rootWindowCompositionChanged() override;
#endif
    v8::MaybeLocal<v8::Value> callFunction(
            v8::Local<v8::Function>  func,
            v8::Local<v8::Value>     recv,
            int                      argc,
            v8::Local<v8::Value>    *argv) override;
#if defined(BLPWTK2_FEATURE_PRINTPDF)
    String printToPDF() override;
#endif

#if defined(BLPWTK2_FEATURE_FASTRESIZE)
    void disableResizeOptimization() override;
#endif

    void setSecurityToken(v8::Isolate *isolate,
                          v8::Local<v8::Value> token) override;

    // WebViewDelegate overrides
    void created(WebView *source) override;
    void didFinishLoad(WebView *source, const StringRef& url) override;
    void didFailLoad(WebView *source, const StringRef& url) override;
    void focused(WebView* source) override;
    void blurred(WebView* source) override;
    void showContextMenu(WebView                  *source,
                         const ContextMenuParams&  params) override;
#if defined(BLPWTK2_FEATURE_FULLSCREEN_MODE)
    void enterFullscreenMode(WebView *source) override;
    void exitFullscreenMode(WebView *source) override;
#endif
    void requestNCHitTest(WebView *source) override;
    void ncDragBegin(WebView      *source,
                     int           hitTestCode,
                     const POINT&  startPoint) override;
    void ncDragMove(WebView *source, const POINT& movePoint) override;
    void ncDragEnd(WebView *source, const POINT& endPoint) override;
    void ncDoubleClick(WebView *source, const POINT& point) override;
    void findState(WebView *source,
                   int      numberOfMatches,
                   int      activeMatchOrdinal,
                   bool     finalUpdate) override;
#if defined(BLPWTK2_FEATURE_DEVTOOLSINTEGRATION)
    void devToolsAgentHostAttached(WebView *source) override;
    void devToolsAgentHostDetached(WebView *source) override;
#endif

#if defined(BLPWTK2_FEATURE_PERFORMANCETIMING)
    void startPerformanceTiming() override;
    void stopPerformanceTiming() override;
#endif

#if defined(BLPWTK2_FEATURE_MEMORY_DIAGNOSTIC)
    std::size_t getDefaultTileMemoryLimit() const override;
    std::size_t getTileMemoryBytes() const override;
    void overrideTileMemoryLimit(std::size_t limit) override;
    void setTag(const char* pTag) override;
#endif

    // WebViewProxyDelegate overrides:
    void notifyRoutingId(int id) override;

    // IPC::Listener overrides
    bool OnMessageReceived(const IPC::Message& message) override;

    // content::InputRouterClient overrides:
    blink::mojom::InputEventResultState FilterInputEvent(
        const blink::WebInputEvent& input_event,
        const ui::LatencyInfo& latency_info) override;
    void IncrementInFlightEventCount() override {}
    void DecrementInFlightEventCount(blink::mojom::InputEventResultSource ack_source) override {}
    void DidOverscroll(const ui::DidOverscrollParams& params) override {}
    void OnSetCompositorAllowedTouchAction(cc::TouchAction) override {}
    void DidStartScrollingViewport() override {}
    void ForwardGestureEventWithLatencyInfo(
        const blink::WebGestureEvent& gesture_event,
        const ui::LatencyInfo& latency_info) override;
    void ForwardWheelEventWithLatencyInfo(
        const blink::WebMouseWheelEvent& wheel_event,
        const ui::LatencyInfo& latency_info) override {}
    bool IsWheelScrollInProgress() override;
    bool IsAutoscrollInProgress() override;

    void SetMouseCapture(bool capture) override {}
    void RequestMouseLock(
      bool from_user_gesture,
      bool unadjusted_movement,
      blink::mojom::WidgetInputHandlerHost::RequestMouseLockCallback response) override;

    gfx::Size GetRootWidgetViewportSize() override;
    void OnInvalidInputEventSource() override {}

    // content::InputRouterImplClient overrides:
    blink::mojom::WidgetInputHandler* GetWidgetInputHandler() override;
    void OnImeCancelComposition() override;
    void OnImeCompositionRangeChanged(
        const gfx::Range& range,
        const std::vector<gfx::Rect>& bounds) override;

    // content::InputDispositionHandler overrides:
    void OnWheelEventAck(
        const content::MouseWheelEventWithLatencyInfo& event,
        blink::mojom::InputEventResultSource ack_source,
        blink::mojom::InputEventResultState ack_result) override {}
    void OnTouchEventAck(
        const content::TouchEventWithLatencyInfo& event,
        blink::mojom::InputEventResultSource ack_source,
        blink::mojom::InputEventResultState ack_result) override {}
    void OnGestureEventAck(
        const content::GestureEventWithLatencyInfo& event,
        blink::mojom::InputEventResultSource ack_source,
        blink::mojom::InputEventResultState ack_result) override {}

    // content::FlingControllerSchedulerClient overrides:
    void ScheduleFlingProgress(
        base::WeakPtr<content::FlingController> fling_controller) override {}
    void DidStopFlingingOnBrowser(
        base::WeakPtr<content::FlingController> fling_controller) override {}
    bool NeedsBeginFrameForFlingProgress() override;

    // ui::internal::InputMethodDelegate overrides:
    ui::EventDispatchDetails DispatchKeyEventPostIME(
        ui::KeyEvent* key_event) override;

    // ui::TextInputClient overrides:
    void SetCompositionText(const ui::CompositionText& composition) override;
    uint32_t ConfirmCompositionText(bool keep_selection) override;
    void ClearCompositionText() override;
    void InsertText(const std::u16string& text, InsertTextCursorBehavior cursor_behavior) override;
    void InsertChar(const ui::KeyEvent& event) override;
    ui::TextInputType GetTextInputType() const override;
    ui::TextInputMode GetTextInputMode() const override;
    base::i18n::TextDirection GetTextDirection() const override;
    int GetTextInputFlags() const override;
    bool CanComposeInline() const override;
    gfx::Rect GetCaretBounds() const override;
    bool GetCompositionCharacterBounds(uint32_t index,
        gfx::Rect* rect) const override;
    bool HasCompositionText() const override;
    FocusReason GetFocusReason() const override;
    bool GetTextRange(gfx::Range* range) const override;
    bool GetCompositionTextRange(gfx::Range* range) const override;
    bool GetEditableSelectionRange(gfx::Range* range) const override;
    bool SetEditableSelectionRange(const gfx::Range& range) override;
    bool DeleteRange(const gfx::Range& range) override;
    bool GetTextFromRange(const gfx::Range& range,
        std::u16string* text) const override;
    void OnInputMethodChanged() override;
    bool ChangeTextDirectionAndLayoutAlignment(
        base::i18n::TextDirection direction) override;
    void ExtendSelectionAndDelete(size_t before, size_t after) override;
    void EnsureCaretNotInRect(const gfx::Rect& rect) override;
    bool IsTextEditCommandEnabled(ui::TextEditCommand command) const override;
    void SetTextEditCommandForNextKeyEvent(ui::TextEditCommand command) override;
    ukm::SourceId GetClientSourceForMetrics() const override;
    bool ShouldDoLearning() override;
    bool SetCompositionFromExistingText(
        const gfx::Range& range,
        const std::vector<ui::ImeTextSpan>& ui_ime_text_spans) override;
    void GetActiveTextInputControlLayoutBounds(
      base::Optional<gfx::Rect>* control_bounds,
      base::Optional<gfx::Rect>* selection_bounds) override {}
    void SetActiveCompositionForAccessibility(
        const gfx::Range& range,
        const std::u16string& active_composition_text,
        bool is_composition_committed) override {}
    gfx::Rect GetSelectionBoundingBox() const override;

    // DragDropDelegate overrides:
    void DragTargetEnter(
        const std::vector<content::DropData::Metadata>& drop_data,
        const gfx::PointF& client_pt,
        const gfx::PointF& screen_pt,
        blink::DragOperationsMask ops_allowed,
        int key_modifiers) override;
    void DragTargetOver(
        const gfx::PointF& client_pt,
        const gfx::PointF& screen_pt,
        blink::DragOperationsMask ops_allowed,
        int key_modifiers) override;
    void DragTargetLeave() override;
    void DragTargetDrop(
        const content::DropData& drop_data,
        const gfx::PointF& client_pt,
        const gfx::PointF& screen_pt,
        int key_modifiers) override;
    void DragSourceEnded(
        const gfx::PointF& client_pt,
        const gfx::PointF& screen_pt,
        blink::DragOperationsMask drag_operation) override;
    void DragSourceSystemEnded() override;

    // blink::mojom::WidgetHost override:
    void SetCursor(const ui::Cursor& cursor) override;
    void UpdateTooltipUnderCursor(const std::u16string& tooltip_text, base::i18n::TextDirection text_direction_hint) override;
    void TextInputStateChanged(ui::mojom::TextInputStatePtr state) override;
    void SelectionBoundsChanged(const gfx::Rect& anchor_rect,
                                base::i18n::TextDirection anchor_dir,
                                const gfx::Rect& focus_rect,
                                base::i18n::TextDirection focus_dir,
                                const gfx::Rect& bounding_box_rect,
                                bool is_anchor_first) override;
    void CreateFrameSink(
        mojo::PendingReceiver<viz::mojom::CompositorFrameSink>
            compositor_frame_sink_receiver,
        mojo::PendingRemote<viz::mojom::CompositorFrameSinkClient>
            compositor_frame_sink_client) override;

    void RegisterRenderFrameMetadataObserver(
        mojo::PendingReceiver<cc::mojom::RenderFrameMetadataObserverClient>
            render_frame_metadata_observer_client_receiver,
        mojo::PendingRemote<cc::mojom::RenderFrameMetadataObserver>
            render_frame_metadata_observer) override;

    // blink::mojom::PointerLockContext overrides
    void RequestMouseLockChange(
        bool unadjusted_movement,
        PointerLockContext::RequestMouseLockChangeCallback response) override;

    // Mouse locking:
    void OnLockMouse(
        bool user_gesture,
        bool request_unadjusted_movement);
    void OnUnlockMouse();

    // Cursor:
    void OnSetCursor(const content::WebCursor& cursor);

    // Keyboard events:
    void OnSelectionChanged(const std::u16string& text,
        uint32_t offset,
        const gfx::Range& range);

    void StartDragging(blink::mojom::DragDataPtr drag_data,
                        blink::DragOperationsMask drag_operations_mask,
                        SkBitmap unsafe_bitmap,
                        gfx::Vector2d bitmap_offset_in_dip,
                        blink::mojom::DragEventSourceInfoPtr event_info);

    void OnUpdateDragCursor(
        ui::mojom::DragOperation drag_operation);

    // Renderer-driven popups:
    void OnShowWidget(const gfx::Rect initial_rect);
    void OnClose();

    // PRIVATE FUNCTIONS:
    explicit RenderWebView(ProfileImpl              *profile,
                           const gfx::Rect&          initialRect);

    static LPCTSTR GetWindowClass();
    static LRESULT CALLBACK WindowProcedure(HWND   hWnd,
                                            UINT   uMsg,
                                            WPARAM wParam,
                                            LPARAM lParam);
    LRESULT windowProcedure(UINT   uMsg,
                            WPARAM wParam,
                            LPARAM lParam);

    void initializeBrowserLike();
    void initializeRendererForWebView();
    void initializeRenderer();
    void updateVisibility();
    void updateGeometry();
    void updateFocus();
    void detachFromRoutingId();
    bool dispatchIPCMessage(const IPC::Message& message);
    void sendScreenRects();
    void setPlatformCursor(HCURSOR cursor);
    void onMouseEventAck(
        const content::MouseEventWithLatencyInfo& event,
        blink::mojom::InputEventResultSource ack_source,
        blink::mojom::InputEventResultState ack_result) {}
    void onKeyboardEventAck(
        const content::NativeWebKeyboardEventWithLatencyInfo& event,
        blink::mojom::InputEventResultSource ack_source,
        blink::mojom::InputEventResultState ack_result) {}
    void onQueueWheelEventWithPhaseEnded();
    void onStartDraggingImpl(
            const content::DropData& drop_data,
            blink::DragOperationsMask drag_operations_mask,
            const SkBitmap& unsafe_bitmap,
            const gfx::Vector2d& bitmap_offset_in_dip);
    void showTooltip();
    void hideTooltip();
    void updateTooltip();
    void onSessionChange(WPARAM status_code, const bool* is_current_session);
    void forceRedrawWindow(int attempts);
    void onDragTargetDropAck();
    void onDragSourceEndedAck();

    // IPC message handlers
    void OnUpdateScreenRectsAck();

    gfx::PointF ConvertWindowPointToViewport(const gfx::PointF& point);

#if defined(BLPWTK2_FEATURE_RUBBERBAND)
    void updateAltDragRubberBanding();
#endif

    base::WeakPtrFactory<RenderWebView> weak_factory_{this};
    DISALLOW_COPY_AND_ASSIGN(RenderWebView);

  public:
    explicit RenderWebView(WebViewDelegate          *delegate,
                           ProfileImpl              *profile,
                           const WebViewProperties&  properties);
    ~RenderWebView() final;

    RenderMessageDelegate& GetMessageDelegate();
};

}  // close namespace blpwtk2

#endif  // INCLUDED_BLPWTK2_RENDERWEBVIEW_H

// vim: ts=4 et
