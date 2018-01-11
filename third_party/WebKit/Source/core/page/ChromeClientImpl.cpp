/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
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

#include "core/page/ChromeClientImpl.h"

#include <memory>
#include <utility>

#include "bindings/core/v8/ScriptController.h"
#include "build/build_config.h"
#include "core/dom/Document.h"
#include "core/dom/Node.h"
#include "core/events/WebInputEventConversion.h"
#include "core/exported/WebFileChooserCompletionImpl.h"
#include "core/exported/WebPluginContainerImpl.h"
#include "core/exported/WebRemoteFrameImpl.h"
#include "core/exported/WebSettingsImpl.h"
#include "core/exported/WebViewImpl.h"
#include "core/frame/BrowserControls.h"
#include "core/frame/LocalFrameView.h"
#include "core/frame/Settings.h"
#include "core/frame/UseCounter.h"
#include "core/frame/VisualViewport.h"
#include "core/frame/WebFrameWidgetImpl.h"
#include "core/frame/WebLocalFrameImpl.h"
#include "core/fullscreen/Fullscreen.h"
#include "core/html/forms/ColorChooser.h"
#include "core/html/forms/ColorChooserClient.h"
#include "core/html/forms/ColorChooserPopupUIController.h"
#include "core/html/forms/ColorChooserUIController.h"
#include "core/html/forms/DateTimeChooser.h"
#include "core/html/forms/DateTimeChooserClient.h"
#include "core/html/forms/DateTimeChooserImpl.h"
#include "core/html/forms/ExternalDateTimeChooser.h"
#include "core/html/forms/ExternalPopupMenu.h"
#include "core/html/forms/FileChooser.h"
#include "core/html/forms/HTMLInputElement.h"
#include "core/html/forms/InternalPopupMenu.h"
#include "core/inspector/DevToolsEmulator.h"
#include "core/layout/HitTestResult.h"
#include "core/layout/LayoutEmbeddedContent.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoadRequest.h"
#include "core/page/ChromeClient.h"
#include "core/page/Page.h"
#include "core/page/PopupOpeningObserver.h"
#include "core/paint/compositing/CompositedSelection.h"
#include "platform/Cursor.h"
#include "platform/Histogram.h"
#include "platform/LayoutTestSupport.h"
#include "platform/WebFrameScheduler.h"
#include "platform/animation/CompositorAnimationHost.h"
#include "platform/exported/WrappedResourceRequest.h"
#include "platform/geometry/IntRect.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/graphics/TouchAction.h"
#include "platform/runtime_enabled_features.h"
#include "platform/scheduler/renderer/web_view_scheduler.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/wtf/Optional.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/text/CString.h"
#include "platform/wtf/text/CharacterNames.h"
#include "platform/wtf/text/StringBuilder.h"
#include "platform/wtf/text/StringConcatenate.h"
#include "public/platform/WebCursorInfo.h"
#include "public/platform/WebFloatRect.h"
#include "public/platform/WebRect.h"
#include "public/platform/WebURLRequest.h"
#include "public/web/WebAutofillClient.h"
#include "public/web/WebConsoleMessage.h"
#include "public/web/WebFrameClient.h"
#include "public/web/WebInputElement.h"
#include "public/web/WebKit.h"
#include "public/web/WebNode.h"
#include "public/web/WebPlugin.h"
#include "public/web/WebPopupMenuInfo.h"
#include "public/web/WebSelection.h"
#include "public/web/WebSettings.h"
#include "public/web/WebTextDirection.h"
#include "public/web/WebUserGestureIndicator.h"
#include "public/web/WebUserGestureToken.h"
#include "public/web/WebViewClient.h"
#include "public/web/WebWindowFeatures.h"

namespace blink {

namespace {

const char* DialogTypeToString(ChromeClient::DialogType dialog_type) {
  switch (dialog_type) {
    case ChromeClient::kAlertDialog:
      return "alert";
    case ChromeClient::kConfirmDialog:
      return "confirm";
    case ChromeClient::kPromptDialog:
      return "prompt";
    case ChromeClient::kPrintDialog:
      return "print";
    case ChromeClient::kHTMLDialog:
      NOTREACHED();
  }
  NOTREACHED();
  return "";
}

const char* DismissalTypeToString(Document::PageDismissalType dismissal_type) {
  switch (dismissal_type) {
    case Document::kBeforeUnloadDismissal:
      return "beforeunload";
    case Document::kPageHideDismissal:
      return "pagehide";
    case Document::kUnloadVisibilityChangeDismissal:
      return "visibilitychange";
    case Document::kUnloadDismissal:
      return "unload";
    case Document::kNoDismissal:
      NOTREACHED();
  }
  NOTREACHED();
  return "";
}

}  // namespace

class CompositorAnimationTimeline;

ChromeClientImpl::ChromeClientImpl(WebViewImpl* web_view)
    : web_view_(web_view),
      cursor_overridden_(false),
      did_request_non_empty_tool_tip_(false) {}

ChromeClientImpl::~ChromeClientImpl() = default;

ChromeClientImpl* ChromeClientImpl::Create(WebViewImpl* web_view) {
  return new ChromeClientImpl(web_view);
}

WebViewImpl* ChromeClientImpl::GetWebView() const {
  return web_view_;
}

void ChromeClientImpl::ChromeDestroyed() {
  // Our lifetime is bound to the WebViewImpl.
}

void ChromeClientImpl::SetWindowRect(const IntRect& r, LocalFrame& frame) {
  DCHECK_EQ(&frame, web_view_->MainFrameImpl()->GetFrame());
  WebWidgetClient* client =
      WebLocalFrameImpl::FromFrame(&frame)->FrameWidget()->Client();
  client->SetWindowRect(r);
}

IntRect ChromeClientImpl::RootWindowRect() {
  WebRect rect;
  if (web_view_->Client()) {
    rect = web_view_->Client()->RootWindowRect();
  } else {
    // These numbers will be fairly wrong. The window's x/y coordinates will
    // be the top left corner of the screen and the size will be the content
    // size instead of the window size.
    rect.width = web_view_->Size().width;
    rect.height = web_view_->Size().height;
  }
  return IntRect(rect);
}

IntRect ChromeClientImpl::PageRect() {
  // We hide the details of the window's border thickness from the web page by
  // simple re-using the window position here.  So, from the point-of-view of
  // the web page, the window has no border.
  return RootWindowRect();
}

void ChromeClientImpl::Focus() {
  if (web_view_->Client())
    web_view_->Client()->DidFocus();
}

bool ChromeClientImpl::CanTakeFocus(WebFocusType) {
  // For now the browser can always take focus if we're not running layout
  // tests.
  return !LayoutTestSupport::IsRunningLayoutTest();
}

void ChromeClientImpl::TakeFocus(WebFocusType type) {
  if (!web_view_->Client())
    return;
  if (type == kWebFocusTypeBackward)
    web_view_->Client()->FocusPrevious();
  else
    web_view_->Client()->FocusNext();
}

void ChromeClientImpl::FocusedNodeChanged(Node* from_node, Node* to_node) {
  if (!web_view_->Client())
    return;

  web_view_->Client()->FocusedNodeChanged(WebNode(from_node), WebNode(to_node));

  WebURL focus_url;
  if (to_node && to_node->IsElementNode() && ToElement(to_node)->IsLiveLink() &&
      to_node->ShouldHaveFocusAppearance())
    focus_url = ToElement(to_node)->HrefURL();
  web_view_->Client()->SetKeyboardFocusURL(focus_url);
}

bool ChromeClientImpl::HadFormInteraction() const {
  return web_view_->PageImportanceSignals() &&
         web_view_->PageImportanceSignals()->HadFormInteraction();
}

void ChromeClientImpl::StartDragging(LocalFrame* frame,
                                     const WebDragData& drag_data,
                                     WebDragOperationsMask mask,
                                     const WebImage& drag_image,
                                     const WebPoint& drag_image_offset) {
  WebLocalFrameImpl* web_frame = WebLocalFrameImpl::FromFrame(frame);
  WebReferrerPolicy policy = web_frame->GetDocument().GetReferrerPolicy();
  web_frame->LocalRoot()->FrameWidget()->StartDragging(
      policy, drag_data, mask, drag_image, drag_image_offset);
}

bool ChromeClientImpl::AcceptsLoadDrops() const {
  return !web_view_->Client() || web_view_->Client()->AcceptsLoadDrops();
}

Page* ChromeClientImpl::CreateWindow(LocalFrame* frame,
                                     const FrameLoadRequest& r,
                                     const WebWindowFeatures& features,
                                     NavigationPolicy navigation_policy,
                                     SandboxFlags sandbox_flags) {
  if (!web_view_->Client())
    return nullptr;

  if (!frame->GetPage() || frame->GetPage()->Paused())
    return nullptr;

  const AtomicString& frame_name =
      !EqualIgnoringASCIICase(r.FrameName(), "_blank") ? r.FrameName()
                                                       : g_empty_atom;
  WebViewImpl* new_view =
      static_cast<WebViewImpl*>(web_view_->Client()->CreateView(
          WebLocalFrameImpl::FromFrame(frame),
          WrappedResourceRequest(r.GetResourceRequest()), features, frame_name,
          static_cast<WebNavigationPolicy>(navigation_policy),
          r.GetShouldSetOpener() == kNeverSetOpener,
          static_cast<WebSandboxFlags>(sandbox_flags)));
  if (!new_view)
    return nullptr;
  return new_view->GetPage();
}

void ChromeClientImpl::DidOverscroll(const FloatSize& overscroll_delta,
                                     const FloatSize& accumulated_overscroll,
                                     const FloatPoint& position_in_viewport,
                                     const FloatSize& velocity_in_viewport,
                                     const WebOverscrollBehavior& behavior) {
  if (!web_view_->Client())
    return;

  web_view_->Client()->DidOverscroll(overscroll_delta, accumulated_overscroll,
                                     position_in_viewport, velocity_in_viewport,
                                     behavior);
}

void ChromeClientImpl::Show(NavigationPolicy navigation_policy) {
  if (web_view_->Client()) {
    web_view_->Client()->Show(
        static_cast<WebNavigationPolicy>(navigation_policy));
  }
}

bool ChromeClientImpl::ShouldReportDetailedMessageForSource(
    LocalFrame& local_frame,
    const String& url) {
  WebLocalFrameImpl* webframe =
      WebLocalFrameImpl::FromFrame(&local_frame.LocalFrameRoot());
  return webframe && webframe->Client() &&
         webframe->Client()->ShouldReportDetailedMessageForSource(url);
}

void ChromeClientImpl::AddMessageToConsole(LocalFrame* local_frame,
                                           MessageSource source,
                                           MessageLevel level,
                                           const String& message,
                                           unsigned line_number,
                                           const String& source_id,
                                           const String& stack_trace) {
  WebLocalFrameImpl* frame = WebLocalFrameImpl::FromFrame(local_frame);
  if (frame && frame->Client()) {
    frame->Client()->DidAddMessageToConsole(
        WebConsoleMessage(static_cast<WebConsoleMessage::Level>(level),
                          message),
        source_id, line_number, stack_trace);
  }
}

bool ChromeClientImpl::CanOpenBeforeUnloadConfirmPanel() {
  return !!web_view_->Client();
}

bool ChromeClientImpl::OpenBeforeUnloadConfirmPanelDelegate(LocalFrame* frame,
                                                            bool is_reload) {
  NotifyPopupOpeningObservers();
  WebLocalFrameImpl* webframe = WebLocalFrameImpl::FromFrame(frame);
  return webframe->Client() &&
         webframe->Client()->RunModalBeforeUnloadDialog(is_reload);
}

void ChromeClientImpl::CloseWindowSoon() {
  if (web_view_->Client())
    web_view_->Client()->CloseWidgetSoon();
}

// Although a LocalFrame is passed in, we don't actually use it, since we
// already know our own m_webView.
bool ChromeClientImpl::OpenJavaScriptAlertDelegate(LocalFrame* frame,
                                                   const String& message) {
  NotifyPopupOpeningObservers();
  WebLocalFrameImpl* webframe = WebLocalFrameImpl::FromFrame(frame);
  if (webframe->Client()) {
    // (TODO(mustaq): why is it going through the web layer? crbug.com/781328
    if (WebUserGestureIndicator::IsProcessingUserGesture(webframe))
      WebUserGestureIndicator::DisableTimeout();
    webframe->Client()->RunModalAlertDialog(message);
    return true;
  }
  return false;
}

// See comments for openJavaScriptAlertDelegate().
bool ChromeClientImpl::OpenJavaScriptConfirmDelegate(LocalFrame* frame,
                                                     const String& message) {
  NotifyPopupOpeningObservers();
  WebLocalFrameImpl* webframe = WebLocalFrameImpl::FromFrame(frame);
  if (webframe->Client()) {
    // (TODO(mustaq): why is it going through the web layer? crbug.com/781328
    if (WebUserGestureIndicator::IsProcessingUserGesture(webframe))
      WebUserGestureIndicator::DisableTimeout();
    return webframe->Client()->RunModalConfirmDialog(message);
  }
  return false;
}

// See comments for openJavaScriptAlertDelegate().
bool ChromeClientImpl::OpenJavaScriptPromptDelegate(LocalFrame* frame,
                                                    const String& message,
                                                    const String& default_value,
                                                    String& result) {
  NotifyPopupOpeningObservers();
  WebLocalFrameImpl* webframe = WebLocalFrameImpl::FromFrame(frame);
  if (webframe->Client()) {
    // (TODO(mustaq): why is it going through the web layer?
    if (WebUserGestureIndicator::IsProcessingUserGesture(webframe))
      WebUserGestureIndicator::DisableTimeout();
    WebString actual_value;
    bool ok = webframe->Client()->RunModalPromptDialog(message, default_value,
                                                       &actual_value);
    if (ok)
      result = actual_value;
    return ok;
  }
  return false;
}
bool ChromeClientImpl::TabsToLinks() {
  return web_view_->TabsToLinks();
}

void ChromeClientImpl::InvalidateRect(const IntRect& update_rect) {
  if (!update_rect.IsEmpty())
    web_view_->InvalidateRect(update_rect);
}

void ChromeClientImpl::ScheduleAnimation(
    const PlatformFrameView* platform_frame_view) {
  DCHECK(platform_frame_view->IsLocalFrameView());
  LocalFrame& frame =
      ToLocalFrameView(platform_frame_view)->GetFrame().LocalFrameRoot();
  // If the frame is still being created, it might not yet have a WebWidget.
  // FIXME: Is this the right thing to do? Is there a way to avoid having
  // a local frame root that doesn't have a WebWidget? During initialization
  // there is no content to draw so this call serves no purpose.
  if (WebLocalFrameImpl::FromFrame(&frame) &&
      WebLocalFrameImpl::FromFrame(&frame)->FrameWidget())
    WebLocalFrameImpl::FromFrame(&frame)->FrameWidget()->ScheduleAnimation();
}

IntRect ChromeClientImpl::ViewportToScreen(
    const IntRect& rect_in_viewport,
    const PlatformFrameView* platform_frame_view) const {
  WebRect screen_rect(rect_in_viewport);

  DCHECK(platform_frame_view->IsLocalFrameView());
  const LocalFrameView* view = ToLocalFrameView(platform_frame_view);
  LocalFrame& frame = view->GetFrame().LocalFrameRoot();

  WebWidgetClient* client =
      WebLocalFrameImpl::FromFrame(&frame)->FrameWidget()->Client();

  if (client) {
    client->ConvertViewportToWindow(&screen_rect);
    WebRect view_rect = client->ViewRect();
    screen_rect.x += view_rect.x;
    screen_rect.y += view_rect.y;
  }

  return screen_rect;
}

float ChromeClientImpl::WindowToViewportScalar(const float scalar_value) const {
  if (!web_view_->Client())
    return scalar_value;
  WebFloatRect viewport_rect(0, 0, scalar_value, 0);
  web_view_->Client()->ConvertWindowToViewport(&viewport_rect);
  return viewport_rect.width;
}

WebScreenInfo ChromeClientImpl::GetScreenInfo() const {
  return web_view_->Client() ? web_view_->Client()->GetScreenInfo()
                             : WebScreenInfo();
}

WTF::Optional<IntRect> ChromeClientImpl::VisibleContentRectForPainting() const {
  return web_view_->GetDevToolsEmulator()->VisibleContentRectForPainting();
}

void ChromeClientImpl::ContentsSizeChanged(LocalFrame* frame,
                                           const IntSize& size) const {
  web_view_->DidChangeContentsSize();

  WebLocalFrameImpl* webframe = WebLocalFrameImpl::FromFrame(frame);
  webframe->DidChangeContentsSize(size);
}

void ChromeClientImpl::PageScaleFactorChanged() const {
  web_view_->PageScaleFactorChanged();
}

void ChromeClientImpl::MainFrameScrollOffsetChanged() const {
  web_view_->MainFrameScrollOffsetChanged();
}

float ChromeClientImpl::ClampPageScaleFactorToLimits(float scale) const {
  return web_view_->ClampPageScaleFactorToLimits(scale);
}

void ChromeClientImpl::ResizeAfterLayout() const {
  web_view_->ResizeAfterLayout();
}

void ChromeClientImpl::LayoutUpdated() const {
  web_view_->LayoutUpdated();
}

void ChromeClientImpl::ShowMouseOverURL(const HitTestResult& result) {
  if (!web_view_->Client())
    return;

  WebURL url;

  // Ignore URL if hitTest include scrollbar since we might have both a
  // scrollbar and an element in the case of overlay scrollbars.
  if (!result.GetScrollbar()) {
    // Find out if the mouse is over a link, and if so, let our UI know...
    if (result.IsLiveLink() &&
        !result.AbsoluteLinkURL().GetString().IsEmpty()) {
      url = result.AbsoluteLinkURL();
    } else if (result.InnerNode() &&
               (IsHTMLObjectElement(*result.InnerNode()) ||
                IsHTMLEmbedElement(*result.InnerNode()))) {
      LayoutObject* object = result.InnerNode()->GetLayoutObject();
      if (object && object->IsLayoutEmbeddedContent()) {
        WebPluginContainerImpl* plugin_view =
            ToLayoutEmbeddedContent(object)->Plugin();
        if (plugin_view) {
          url = plugin_view->Plugin()->LinkAtPosition(
              result.RoundedPointInInnerNodeFrame());
        }
      }
    }
  }

  web_view_->Client()->SetMouseOverURL(url);
}

void ChromeClientImpl::SetToolTip(LocalFrame& frame,
                                  const String& tooltip_text,
                                  TextDirection dir) {
  WebLocalFrameImpl* web_frame =
      WebLocalFrameImpl::FromFrame(&frame)->LocalRoot();
  if (!tooltip_text.IsEmpty()) {
    web_frame->FrameWidget()->Client()->SetToolTipText(tooltip_text,
                                                       ToWebTextDirection(dir));
    did_request_non_empty_tool_tip_ = true;
  } else if (did_request_non_empty_tool_tip_) {
    // WebWidgetClient::setToolTipText will send an IPC message.  We'd like to
    // reduce the number of setToolTipText calls.
    web_frame->FrameWidget()->Client()->SetToolTipText(tooltip_text,
                                                       ToWebTextDirection(dir));
    did_request_non_empty_tool_tip_ = false;
  }
}

void ChromeClientImpl::DispatchViewportPropertiesDidChange(
    const ViewportDescription& description) const {
  web_view_->UpdatePageDefinedViewportConstraints(description);
}

void ChromeClientImpl::PrintDelegate(LocalFrame* frame) {
  NotifyPopupOpeningObservers();
  if (web_view_->Client())
    web_view_->Client()->PrintPage(WebLocalFrameImpl::FromFrame(frame));
}

ColorChooser* ChromeClientImpl::OpenColorChooser(
    LocalFrame* frame,
    ColorChooserClient* chooser_client,
    const Color&) {
  NotifyPopupOpeningObservers();
  ColorChooserUIController* controller = nullptr;

  if (frame->GetDocument()->GetSettings()->GetPagePopupsSuppressed())
    return nullptr;

  if (RuntimeEnabledFeatures::PagePopupEnabled()) {
    controller =
        ColorChooserPopupUIController::Create(frame, this, chooser_client);
  } else {
    controller = ColorChooserUIController::Create(frame, chooser_client);
  }
  controller->OpenUI();
  return controller;
}

DateTimeChooser* ChromeClientImpl::OpenDateTimeChooser(
    DateTimeChooserClient* picker_client,
    const DateTimeChooserParameters& parameters) {
  if (picker_client->OwnerElement()
          .GetDocument()
          .GetSettings()
          ->GetPagePopupsSuppressed())
    return nullptr;

  NotifyPopupOpeningObservers();
  if (RuntimeEnabledFeatures::InputMultipleFieldsUIEnabled())
    return DateTimeChooserImpl::Create(this, picker_client, parameters);
  return ExternalDateTimeChooser::Create(this, web_view_->Client(),
                                         picker_client, parameters);
}

void ChromeClientImpl::OpenFileChooser(
    LocalFrame* frame,
    scoped_refptr<FileChooser> file_chooser) {
  NotifyPopupOpeningObservers();
  WebFrameClient* client = WebLocalFrameImpl::FromFrame(frame)->Client();
  if (!client)
    return;

  const WebFileChooserParams& params = file_chooser->Params();
  WebFileChooserCompletionImpl* chooser_completion =
      new WebFileChooserCompletionImpl(std::move(file_chooser));
  if (client->RunFileChooser(params, chooser_completion))
    return;
  // Choosing failed, so do callback with an empty list.
  chooser_completion->DidChooseFile(WebVector<WebString>());
}

void ChromeClientImpl::EnumerateChosenDirectory(FileChooser* file_chooser) {
  WebViewClient* client = web_view_->Client();
  if (!client)
    return;

  WebFileChooserCompletionImpl* chooser_completion =
      new WebFileChooserCompletionImpl(file_chooser);

  DCHECK(file_chooser);
  DCHECK(file_chooser->Params().selected_files.size());

  // If the enumeration can't happen, call the callback with an empty list.
  if (!client->EnumerateChosenDirectory(
          file_chooser->Params().selected_files[0], chooser_completion))
    chooser_completion->DidChooseFile(WebVector<WebString>());
}

Cursor ChromeClientImpl::LastSetCursorForTesting() const {
  return last_set_mouse_cursor_for_testing_;
}

void ChromeClientImpl::SetCursor(const Cursor& cursor,
                                 LocalFrame* local_frame) {
  last_set_mouse_cursor_for_testing_ = cursor;
  SetCursor(WebCursorInfo(cursor), local_frame);
}

void ChromeClientImpl::SetCursor(const WebCursorInfo& cursor,
                                 LocalFrame* local_frame) {
  if (cursor_overridden_)
    return;

#if defined(OS_MACOSX)
  // On Mac the mousemove event propagates to both the popup and main window.
  // If a popup is open we don't want the main window to change the cursor.
  if (web_view_->HasOpenedPopup())
    return;
#endif

  LocalFrame& local_root = local_frame->LocalFrameRoot();
  if (WebFrameWidgetBase* widget =
          WebLocalFrameImpl::FromFrame(&local_root)->FrameWidget())
    widget->Client()->DidChangeCursor(cursor);
}

void ChromeClientImpl::SetCursorForPlugin(const WebCursorInfo& cursor,
                                          LocalFrame* local_frame) {
  SetCursor(cursor, local_frame);
}

void ChromeClientImpl::SetCursorOverridden(bool overridden) {
  cursor_overridden_ = overridden;
}

void ChromeClientImpl::AutoscrollStart(WebFloatPoint viewport_point,
                                       LocalFrame* local_frame) {
  LocalFrame& local_root = local_frame->LocalFrameRoot();
  if (WebFrameWidgetBase* widget =
          WebLocalFrameImpl::FromFrame(&local_root)->FrameWidget())
    widget->Client()->AutoscrollStart(viewport_point);
}

void ChromeClientImpl::AutoscrollFling(WebFloatSize velocity,
                                       LocalFrame* local_frame) {
  LocalFrame& local_root = local_frame->LocalFrameRoot();
  if (WebFrameWidgetBase* widget =
          WebLocalFrameImpl::FromFrame(&local_root)->FrameWidget())
    widget->Client()->AutoscrollFling(velocity);
}

void ChromeClientImpl::AutoscrollEnd(LocalFrame* local_frame) {
  LocalFrame& local_root = local_frame->LocalFrameRoot();
  if (WebFrameWidgetBase* widget =
          WebLocalFrameImpl::FromFrame(&local_root)->FrameWidget())
    widget->Client()->AutoscrollEnd();
}

String ChromeClientImpl::AcceptLanguages() {
  return web_view_->Client()->AcceptLanguages();
}

void ChromeClientImpl::AttachRootGraphicsLayer(GraphicsLayer* root_layer,
                                               LocalFrame* local_frame) {
  DCHECK(!RuntimeEnabledFeatures::SlimmingPaintV2Enabled());
  WebLocalFrameImpl* web_frame =
      WebLocalFrameImpl::FromFrame(local_frame)->LocalRoot();

  // This method can be called while the frame is being detached. In that
  // case, the rootLayer is null, and the widget is already destroyed.
  DCHECK(web_frame->FrameWidget() || !root_layer);
  if (web_frame->FrameWidget())
    web_frame->FrameWidget()->SetRootGraphicsLayer(root_layer);
}

void ChromeClientImpl::AttachRootLayer(WebLayer* root_layer,
                                       LocalFrame* local_frame) {
  WebLocalFrameImpl* web_frame =
      WebLocalFrameImpl::FromFrame(local_frame)->LocalRoot();

  // This method can be called while the frame is being detached. In that
  // case, the rootLayer is null, and the widget is already destroyed.
  DCHECK(web_frame->FrameWidget() || !root_layer);
  if (web_frame->FrameWidget())
    web_frame->FrameWidget()->SetRootLayer(root_layer);
}

void ChromeClientImpl::AttachCompositorAnimationTimeline(
    CompositorAnimationTimeline* compositor_timeline,
    LocalFrame* local_frame) {
  WebLocalFrameImpl* web_frame =
      WebLocalFrameImpl::FromFrame(local_frame)->LocalRoot();
  if (CompositorAnimationHost* animation_host =
          web_frame->FrameWidget()->AnimationHost())
    animation_host->AddTimeline(*compositor_timeline);
}

void ChromeClientImpl::DetachCompositorAnimationTimeline(
    CompositorAnimationTimeline* compositor_timeline,
    LocalFrame* local_frame) {
  WebLocalFrameImpl* web_frame =
      WebLocalFrameImpl::FromFrame(local_frame)->LocalRoot();

  // This method can be called when the frame is being detached, after the
  // widget is destroyed.
  if (web_frame->FrameWidget()) {
    if (CompositorAnimationHost* animation_host =
            web_frame->FrameWidget()->AnimationHost())
      animation_host->RemoveTimeline(*compositor_timeline);
  }
}

void ChromeClientImpl::EnterFullscreen(LocalFrame& frame) {
  web_view_->EnterFullscreen(frame);
}

void ChromeClientImpl::ExitFullscreen(LocalFrame& frame) {
  web_view_->ExitFullscreen(frame);
}

void ChromeClientImpl::FullscreenElementChanged(Element* old_element,
                                                Element* new_element) {
  web_view_->FullscreenElementChanged(old_element, new_element);
}

void ChromeClientImpl::ClearCompositedSelection(LocalFrame* frame) {
  LocalFrame& local_root = frame->LocalFrameRoot();
  WebFrameWidgetBase* widget =
      WebLocalFrameImpl::FromFrame(&local_root)->FrameWidget();
  WebWidgetClient* client = widget->Client();
  if (!client)
    return;

  if (WebLayerTreeView* layer_tree_view = widget->GetLayerTreeView())
    layer_tree_view->ClearSelection();
}

void ChromeClientImpl::UpdateCompositedSelection(
    LocalFrame* frame,
    const CompositedSelection& selection) {
  LocalFrame& local_root = frame->LocalFrameRoot();
  WebFrameWidgetBase* widget =
      WebLocalFrameImpl::FromFrame(&local_root)->FrameWidget();
  WebWidgetClient* client = widget->Client();
  if (!client)
    return;

  if (WebLayerTreeView* layer_tree_view = widget->GetLayerTreeView())
    layer_tree_view->RegisterSelection(WebSelection(selection));
}

bool ChromeClientImpl::HasOpenedPopup() const {
  return web_view_->HasOpenedPopup();
}

PopupMenu* ChromeClientImpl::OpenPopupMenu(LocalFrame& frame,
                                           HTMLSelectElement& select) {
  if (frame.GetDocument()->GetSettings()->GetPagePopupsSuppressed())
    return nullptr;

  NotifyPopupOpeningObservers();
  if (WebViewImpl::UseExternalPopupMenus())
    return new ExternalPopupMenu(frame, select, *web_view_);

  DCHECK(RuntimeEnabledFeatures::PagePopupEnabled());
  return InternalPopupMenu::Create(this, select);
}

PagePopup* ChromeClientImpl::OpenPagePopup(PagePopupClient* client) {
  return web_view_->OpenPagePopup(client);
}

void ChromeClientImpl::ClosePagePopup(PagePopup* popup) {
  web_view_->ClosePagePopup(popup);
}

DOMWindow* ChromeClientImpl::PagePopupWindowForTesting() const {
  return web_view_->PagePopupWindow();
}

void ChromeClientImpl::SetBrowserControlsState(float top_height,
                                               float bottom_height,
                                               bool shrinks_layout) {
  WebSize size = web_view_->Size();
  if (shrinks_layout)
    size.height -= top_height + bottom_height;

  web_view_->ResizeWithBrowserControls(size, top_height, bottom_height,
                                       shrinks_layout);
}

void ChromeClientImpl::SetBrowserControlsShownRatio(float ratio) {
  web_view_->GetBrowserControls().SetShownRatio(ratio);
  web_view_->DidUpdateBrowserControls();
}

bool ChromeClientImpl::ShouldOpenModalDialogDuringPageDismissal(
    LocalFrame& frame,
    DialogType dialog_type,
    const String& dialog_message,
    Document::PageDismissalType dismissal_type) const {
  String message = String("Blocked ") + DialogTypeToString(dialog_type) + "('" +
                   dialog_message + "') during " +
                   DismissalTypeToString(dismissal_type) + ".";
  WebLocalFrameImpl::FromFrame(frame)->AddMessageToConsole(
      WebConsoleMessage(WebConsoleMessage::kLevelError, message));

  return false;
}

WebLayerTreeView* ChromeClientImpl::GetWebLayerTreeView(LocalFrame* frame) {
  WebLocalFrameImpl* web_frame = WebLocalFrameImpl::FromFrame(frame);
  return web_frame->LocalRoot()->FrameWidget()->GetLayerTreeView();
}

void ChromeClientImpl::RequestDecode(LocalFrame* frame,
                                     const PaintImage& image,
                                     base::OnceCallback<void(bool)> callback) {
  WebLocalFrameImpl* web_frame = WebLocalFrameImpl::FromFrame(frame);
  web_frame->LocalRoot()->FrameWidget()->RequestDecode(image,
                                                       std::move(callback));
}

void ChromeClientImpl::SetEventListenerProperties(
    LocalFrame* frame,
    WebEventListenerClass event_class,
    WebEventListenerProperties properties) {
  // |frame| might be null if called via TreeScopeAdopter::
  // moveNodeToNewDocument() and the new document has no frame attached.
  // Since a document without a frame cannot attach one later, it is safe to
  // exit early.
  if (!frame)
    return;

  WebLocalFrameImpl* web_frame = WebLocalFrameImpl::FromFrame(frame);
  WebFrameWidgetBase* widget = web_frame->LocalRoot()->FrameWidget();
  // The widget may be nullptr if the frame is provisional.
  // TODO(dcheng): This needs to be cleaned up at some point.
  // https://crbug.com/578349
  if (!widget) {
    // If we hit a provisional frame, we expect it to be during initialization
    // in which case the |properties| should be 'nothing'.
    DCHECK(properties == WebEventListenerProperties::kNothing);
    return;
  }

  // This relies on widget always pointing to a WebFrameWidgetImpl when
  // |frame| points to an OOPIF frame, i.e. |frame|'s mainFrame() is
  // remote.
  WebWidgetClient* client = widget->Client();
  if (WebLayerTreeView* tree_view = widget->GetLayerTreeView()) {
    tree_view->SetEventListenerProperties(event_class, properties);
    if (event_class == WebEventListenerClass::kTouchStartOrMove) {
      client->HasTouchEventHandlers(
          properties != WebEventListenerProperties::kNothing ||
          tree_view->EventListenerProperties(
              WebEventListenerClass::kTouchEndOrCancel) !=
              WebEventListenerProperties::kNothing);
    } else if (event_class == WebEventListenerClass::kTouchEndOrCancel) {
      client->HasTouchEventHandlers(
          properties != WebEventListenerProperties::kNothing ||
          tree_view->EventListenerProperties(
              WebEventListenerClass::kTouchStartOrMove) !=
              WebEventListenerProperties::kNothing);
    }
  } else {
    client->HasTouchEventHandlers(true);
  }
}

void ChromeClientImpl::UpdateEventRectsForSubframeIfNecessary(
    LocalFrame* frame) {
  WebLocalFrameImpl* web_frame = WebLocalFrameImpl::FromFrame(frame);
  WebFrameWidgetBase* widget = web_frame->LocalRoot()->FrameWidget();
  if (WebLayerTreeView* tree_view = widget->GetLayerTreeView())
    tree_view->UpdateEventRectsForSubframeIfNecessary();
}

void ChromeClientImpl::BeginLifecycleUpdates() {
  if (WebLayerTreeView* tree_view = web_view_->LayerTreeView()) {
    tree_view->SetDeferCommits(false);
    tree_view->SetNeedsBeginFrame();
  }
}

WebEventListenerProperties ChromeClientImpl::EventListenerProperties(
    LocalFrame* frame,
    WebEventListenerClass event_class) const {
  if (!frame)
    return WebEventListenerProperties::kNothing;

  WebFrameWidgetBase* widget =
      WebLocalFrameImpl::FromFrame(frame)->LocalRoot()->FrameWidget();

  if (!widget || !widget->GetLayerTreeView())
    return WebEventListenerProperties::kNothing;
  return widget->GetLayerTreeView()->EventListenerProperties(event_class);
}

void ChromeClientImpl::SetHasScrollEventHandlers(LocalFrame* frame,
                                                 bool has_event_handlers) {
  // |frame| might be null if called via TreeScopeAdopter::
  // moveNodeToNewDocument() and the new document has no frame attached.
  // Since a document without a frame cannot attach one later, it is safe to
  // exit early.
  if (!frame)
    return;

  WebFrameWidgetBase* widget =
      WebLocalFrameImpl::FromFrame(frame)->LocalRoot()->FrameWidget();
  // While a frame is shutting down, we may get called after the layerTreeView
  // is gone: in this case we always expect |hasEventHandlers| to be false.
  DCHECK(!widget || widget->GetLayerTreeView() || !has_event_handlers);
  if (widget && widget->GetLayerTreeView())
    widget->GetLayerTreeView()->SetHaveScrollEventHandlers(has_event_handlers);
}

void ChromeClientImpl::SetNeedsLowLatencyInput(LocalFrame* frame,
                                               bool needs_low_latency) {
  DCHECK(frame);
  WebLocalFrameImpl* web_frame = WebLocalFrameImpl::FromFrame(frame);
  WebFrameWidgetBase* widget = web_frame->LocalRoot()->FrameWidget();
  if (!widget)
    return;

  if (WebWidgetClient* client = widget->Client())
    client->SetNeedsLowLatencyInput(needs_low_latency);
}

void ChromeClientImpl::RequestUnbufferedInputEvents(LocalFrame* frame) {
  DCHECK(frame);
  WebLocalFrameImpl* web_frame = WebLocalFrameImpl::FromFrame(frame);
  WebFrameWidgetBase* widget = web_frame->LocalRoot()->FrameWidget();
  if (!widget)
    return;

  if (WebWidgetClient* client = widget->Client())
    client->RequestUnbufferedInputEvents();
}

void ChromeClientImpl::SetTouchAction(LocalFrame* frame,
                                      TouchAction touch_action) {
  DCHECK(frame);
  WebLocalFrameImpl* web_frame = WebLocalFrameImpl::FromFrame(frame);
  WebFrameWidgetBase* widget = web_frame->LocalRoot()->FrameWidget();
  if (!widget)
    return;

  if (WebWidgetClient* client = widget->Client())
    client->SetTouchAction(static_cast<TouchAction>(touch_action));
}

bool ChromeClientImpl::RequestPointerLock(LocalFrame* frame) {
  LocalFrame& local_root = frame->LocalFrameRoot();
  return WebLocalFrameImpl::FromFrame(&local_root)
      ->FrameWidget()
      ->Client()
      ->RequestPointerLock();
}

void ChromeClientImpl::RequestPointerUnlock(LocalFrame* frame) {
  LocalFrame& local_root = frame->LocalFrameRoot();
  return WebLocalFrameImpl::FromFrame(&local_root)
      ->FrameWidget()
      ->Client()
      ->RequestPointerUnlock();
}

void ChromeClientImpl::DidAssociateFormControlsAfterLoad(LocalFrame* frame) {
  if (auto* fill_client = AutofillClientFromFrame(frame))
    fill_client->DidAssociateFormControlsDynamically();
}

void ChromeClientImpl::ShowVirtualKeyboardOnElementFocus(LocalFrame& frame) {
  WebLocalFrameImpl::FromFrame(frame.LocalFrameRoot())
      ->FrameWidget()
      ->Client()
      ->ShowVirtualKeyboardOnElementFocus();
}

void ChromeClientImpl::ShowUnhandledTapUIIfNeeded(WebTappedInfo& tapped_info) {
  //  Node* node = tapped_info.RawNode();
  //  web_node(WebNode(node));
  //  tapped_info.SetNode(web_node);
  if (web_view_->Client()) {
    web_view_->Client()->ShowUnhandledTapUIIfNeeded(tapped_info);
  }
}

void ChromeClientImpl::OnMouseDown(Node& mouse_down_node) {
  if (auto* fill_client =
          AutofillClientFromFrame(mouse_down_node.GetDocument().GetFrame())) {
    fill_client->DidReceiveLeftMouseDownOrGestureTapInNode(
        WebNode(&mouse_down_node));
  }
}

void ChromeClientImpl::HandleKeyboardEventOnTextField(
    HTMLInputElement& input_element,
    KeyboardEvent& event) {
  if (auto* fill_client =
          AutofillClientFromFrame(input_element.GetDocument().GetFrame())) {
    fill_client->TextFieldDidReceiveKeyDown(WebInputElement(&input_element),
                                            WebKeyboardEventBuilder(event));
  }
}

void ChromeClientImpl::DidChangeValueInTextField(
    HTMLFormControlElement& element) {
  Document& doc = element.GetDocument();
  if (auto* fill_client = AutofillClientFromFrame(doc.GetFrame()))
    fill_client->TextFieldDidChange(WebFormControlElement(&element));

  // Value changes caused by |document.execCommand| calls should not be
  // interpreted as a user action. See https://crbug.com/764760.
  if (!doc.IsRunningExecCommand()) {
    UseCounter::Count(doc, doc.IsSecureContext()
                               ? WebFeature::kFieldEditInSecureContext
                               : WebFeature::kFieldEditInNonSecureContext);
    doc.MaybeQueueSendDidEditFieldInInsecureContext();
    web_view_->PageImportanceSignals()->SetHadFormInteraction();
  }
}

void ChromeClientImpl::DidEndEditingOnTextField(
    HTMLInputElement& input_element) {
  if (auto* fill_client =
          AutofillClientFromFrame(input_element.GetDocument().GetFrame())) {
    fill_client->TextFieldDidEndEditing(WebInputElement(&input_element));
  }
}

void ChromeClientImpl::OpenTextDataListChooser(HTMLInputElement& input) {
  NotifyPopupOpeningObservers();
  if (auto* fill_client =
          AutofillClientFromFrame(input.GetDocument().GetFrame())) {
    fill_client->OpenTextDataListChooser(WebInputElement(&input));
  }
}

void ChromeClientImpl::TextFieldDataListChanged(HTMLInputElement& input) {
  if (auto* fill_client =
          AutofillClientFromFrame(input.GetDocument().GetFrame())) {
    fill_client->DataListOptionsChanged(WebInputElement(&input));
  }
}

void ChromeClientImpl::AjaxSucceeded(LocalFrame* frame) {
  if (auto* fill_client = AutofillClientFromFrame(frame))
    fill_client->AjaxSucceeded();
}

void ChromeClientImpl::RegisterViewportLayers() const {
  if (web_view_->RootGraphicsLayer() && web_view_->LayerTreeView())
    web_view_->RegisterViewportLayersWithCompositor();
}

void ChromeClientImpl::DidUpdateBrowserControls() const {
  web_view_->DidUpdateBrowserControls();
}

void ChromeClientImpl::SetOverscrollBehavior(
    const WebOverscrollBehavior& overscroll_behavior) {
  web_view_->SetOverscrollBehavior(overscroll_behavior);
}

void ChromeClientImpl::RegisterPopupOpeningObserver(
    PopupOpeningObserver* observer) {
  DCHECK(observer);
  popup_opening_observers_.push_back(observer);
}

void ChromeClientImpl::UnregisterPopupOpeningObserver(
    PopupOpeningObserver* observer) {
  size_t index = popup_opening_observers_.Find(observer);
  DCHECK_NE(index, kNotFound);
  popup_opening_observers_.EraseAt(index);
}

void ChromeClientImpl::NotifyPopupOpeningObservers() const {
  const Vector<PopupOpeningObserver*> observers(popup_opening_observers_);
  for (const auto& observer : observers)
    observer->WillOpenPopup();
}

FloatSize ChromeClientImpl::ElasticOverscroll() const {
  return web_view_->ElasticOverscroll();
}

std::unique_ptr<WebFrameScheduler> ChromeClientImpl::CreateFrameScheduler(
    BlameContext* blame_context,
    WebFrameScheduler::FrameType frame_type) {
  return web_view_->Scheduler()->CreateFrameScheduler(blame_context,
                                                      frame_type);
}

WebAutofillClient* ChromeClientImpl::AutofillClientFromFrame(
    LocalFrame* frame) {
  if (!frame) {
    // It is possible to pass nullptr to this method. For instance the call from
    // OnMouseDown might be nullptr. See https://crbug.com/739199.
    return nullptr;
  }

  return WebLocalFrameImpl::FromFrame(frame)->AutofillClient();
}

}  // namespace blink
