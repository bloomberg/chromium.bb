// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "AccessibilityObject.h"
#include "AXObjectCache.h"
#include "CharacterNames.h"
#include "Console.h"
#include "Cursor.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "DatabaseTracker.h"
#include "FloatRect.h"
#include "FileChooser.h"
#include "FrameLoadRequest.h"
#include "FrameView.h"
#include "HitTestResult.h"
#include "IntRect.h"
#include "Node.h"
#include "Page.h"
#include "PopupMenuChromium.h"
#include "ScriptController.h"
#include "WindowFeatures.h"
#if USE(V8)
#include "V8Proxy.h"
#endif
#undef LOG

#include "webkit/api/public/WebAccessibilityObject.h"
#include "webkit/api/public/WebConsoleMessage.h"
#include "webkit/api/public/WebCursorInfo.h"
#include "webkit/api/public/WebFileChooserCompletion.h"
#include "webkit/api/public/WebFrameClient.h"
#include "webkit/api/public/WebInputEvent.h"
#include "webkit/api/public/WebKit.h"
#include "webkit/api/public/WebPopupMenuInfo.h"
#include "webkit/api/public/WebRect.h"
#include "webkit/api/public/WebTextDirection.h"
#include "webkit/api/public/WebURLRequest.h"
#include "webkit/api/public/WebViewClient.h"
#include "webkit/api/src/NotificationPresenterImpl.h"
#include "webkit/api/src/WebFileChooserCompletionImpl.h"
#include "webkit/api/src/WrappedResourceRequest.h"
#include "webkit/glue/chrome_client_impl.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/webframe_impl.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webpopupmenu_impl.h"
#include "webkit/glue/webview_delegate.h"
#include "webkit/glue/webview_impl.h"

using WebCore::PopupContainer;
using WebCore::PopupItem;

using WebKit::WebAccessibilityObject;
using WebKit::WebConsoleMessage;
using WebKit::WebCursorInfo;
using WebKit::WebFileChooserCompletionImpl;
using WebKit::WebInputEvent;
using WebKit::WebMouseEvent;
using WebKit::WebNavigationPolicy;
using WebKit::WebPopupMenuInfo;
using WebKit::WebRect;
using WebKit::WebString;
using WebKit::WebTextDirection;
using WebKit::WebURL;
using WebKit::WebURLRequest;
using WebKit::WebVector;
using WebKit::WebViewClient;
using WebKit::WebWidget;
using WebKit::WrappedResourceRequest;

using webkit_glue::AccessibilityObjectToWebAccessibilityObject;

ChromeClientImpl::ChromeClientImpl(WebViewImpl* webview)
    : webview_(webview),
      toolbars_visible_(true),
      statusbar_visible_(true),
      scrollbars_visible_(true),
      menubar_visible_(true),
      resizable_(true),
      ignore_next_set_cursor_(false) {
}

ChromeClientImpl::~ChromeClientImpl() {
}

void ChromeClientImpl::chromeDestroyed() {
  // Our lifetime is bound to the WebViewImpl.
}

void ChromeClientImpl::setWindowRect(const WebCore::FloatRect& r) {
  if (webview_->client()) {
    webview_->client()->setWindowRect(
        webkit_glue::IntRectToWebRect(WebCore::IntRect(r)));
  }
}

WebCore::FloatRect ChromeClientImpl::windowRect() {
  WebRect rect;
  if (webview_->client()) {
    rect = webview_->client()->rootWindowRect();
  } else {
    // These numbers will be fairly wrong. The window's x/y coordinates will
    // be the top left corner of the screen and the size will be the content
    // size instead of the window size.
    rect.width = webview_->size().width;
    rect.height = webview_->size().height;
  }
  return WebCore::FloatRect(webkit_glue::WebRectToIntRect(rect));
}

WebCore::FloatRect ChromeClientImpl::pageRect() {
  // We hide the details of the window's border thickness from the web page by
  // simple re-using the window position here.  So, from the point-of-view of
  // the web page, the window has no border.
  return windowRect();
}

float ChromeClientImpl::scaleFactor() {
  // This is supposed to return the scale factor of the web page. It looks like
  // the implementor of the graphics layer is responsible for doing most of the
  // operations associated with scaling. However, this value is used ins some
  // cases by WebCore. For example, this is used as a scaling factor in canvas
  // so that things drawn in it are scaled just like the web page is.
  //
  // We don't currently implement scaling, so just return 1.0 (no scaling).
  return 1.0;
}

void ChromeClientImpl::focus() {
  if (!webview_->client())
    return;

  webview_->client()->didFocus();

  // If accessibility is enabled, we should notify assistive technology that
  // the active AccessibilityObject changed.
  const WebCore::Frame* frame = webview_->GetFocusedWebCoreFrame();
  if (!frame)
    return;

  WebCore::Document* doc = frame->document();

  if (doc && doc->axObjectCache()->accessibilityEnabled()) {
    WebCore::Node* focused_node = webview_->GetFocusedNode();

    if (!focused_node) {
      // Could not retrieve focused Node.
      return;
    }

    // Retrieve the focused AccessibilityObject.
    WebCore::AccessibilityObject* focused_acc_obj =
        doc->axObjectCache()->getOrCreate(focused_node->renderer());

    // Alert assistive technology that focus changed.
    if (focused_acc_obj) {
      webview_->client()->focusAccessibilityObject(
          AccessibilityObjectToWebAccessibilityObject(focused_acc_obj));
    }
  }
}

void ChromeClientImpl::unfocus() {
  if (webview_->client())
    webview_->client()->didBlur();
}

bool ChromeClientImpl::canTakeFocus(WebCore::FocusDirection) {
  // For now the browser can always take focus if we're not running layout
  // tests.
  return !WebKit::layoutTestMode();
}

void ChromeClientImpl::takeFocus(WebCore::FocusDirection direction) {
  if (!webview_->client())
    return;
  if (direction == WebCore::FocusDirectionBackward) {
    webview_->client()->focusPrevious();
  } else {
    webview_->client()->focusNext();
  }
}

WebCore::Page* ChromeClientImpl::createWindow(
    WebCore::Frame* frame, const WebCore::FrameLoadRequest& r,
    const WebCore::WindowFeatures& features) {
  if (!webview_->client())
    return NULL;

  WebViewImpl* new_view = static_cast<WebViewImpl*>(
      webview_->client()->createView(WebFrameImpl::FromFrame(frame)));
  if (!new_view)
    return NULL;

  // The request is empty when we are just being asked to open a blank window.
  // This corresponds to window.open(""), for example.
  if (!r.resourceRequest().isEmpty()) {
    WrappedResourceRequest request(r.resourceRequest());
    new_view->main_frame()->loadRequest(request);
  }

  return new_view->page();
}

static inline bool CurrentEventShouldCauseBackgroundTab(
    const WebInputEvent* input_event) {
  if (!input_event)
    return false;

  if (input_event->type != WebInputEvent::MouseUp)
    return false;

  const WebMouseEvent* mouse_event =
      static_cast<const WebMouseEvent*>(input_event);

  WebNavigationPolicy policy;
  unsigned short button_number;
  switch (mouse_event->button) {
    case WebMouseEvent::ButtonLeft:
      button_number = 0;
      break;
    case WebMouseEvent::ButtonMiddle:
      button_number = 1;
      break;
    case WebMouseEvent::ButtonRight:
      button_number = 2;
      break;
    default:
      return false;
  }
  bool ctrl = mouse_event->modifiers & WebMouseEvent::ControlKey;
  bool shift = mouse_event->modifiers & WebMouseEvent::ShiftKey;
  bool alt = mouse_event->modifiers & WebMouseEvent::AltKey;
  bool meta = mouse_event->modifiers & WebMouseEvent::MetaKey;

  if (!WebViewImpl::NavigationPolicyFromMouseEvent(button_number, ctrl,
      shift, alt, meta, &policy))
    return false;

  return policy == WebKit::WebNavigationPolicyNewBackgroundTab;
}

void ChromeClientImpl::show() {
  WebViewDelegate* delegate = webview_->delegate();
  if (delegate) {
    // If our default configuration was modified by a script or wasn't
    // created by a user gesture, then show as a popup. Else, let this
    // new window be opened as a toplevel window.
    //
    bool as_popup =
        !toolbars_visible_ ||
        !statusbar_visible_ ||
        !scrollbars_visible_ ||
        !menubar_visible_ ||
        !resizable_ ||
        !delegate->WasOpenedByUserGesture();

    WebNavigationPolicy policy = WebKit::WebNavigationPolicyNewForegroundTab;
    if (as_popup)
      policy = WebKit::WebNavigationPolicyNewPopup;
    if (CurrentEventShouldCauseBackgroundTab(
          WebViewImpl::current_input_event()))
      policy = WebKit::WebNavigationPolicyNewBackgroundTab;

    delegate->show(policy);
  }
}

bool ChromeClientImpl::canRunModal() {
  return webview_->client() != NULL;
}

void ChromeClientImpl::runModal() {
  if (webview_->client())
    webview_->client()->runModal();
}

void ChromeClientImpl::setToolbarsVisible(bool value) {
  toolbars_visible_ = value;
}

bool ChromeClientImpl::toolbarsVisible() {
  return toolbars_visible_;
}

void ChromeClientImpl::setStatusbarVisible(bool value) {
  statusbar_visible_ = value;
}

bool ChromeClientImpl::statusbarVisible() {
  return statusbar_visible_;
}

void ChromeClientImpl::setScrollbarsVisible(bool value) {
  scrollbars_visible_ = value;
  WebFrameImpl* web_frame = static_cast<WebFrameImpl*>(webview_->mainFrame());
  if (web_frame)
    web_frame->SetAllowsScrolling(value);
}

bool ChromeClientImpl::scrollbarsVisible() {
  return scrollbars_visible_;
}

void ChromeClientImpl::setMenubarVisible(bool value) {
  menubar_visible_ = value;
}

bool ChromeClientImpl::menubarVisible() {
  return menubar_visible_;
}

void ChromeClientImpl::setResizable(bool value) {
  resizable_ = value;
}

void ChromeClientImpl::addMessageToConsole(WebCore::MessageSource source,
                                           WebCore::MessageType type,
                                           WebCore::MessageLevel level,
                                           const WebCore::String& message,
                                           unsigned int line_no,
                                           const WebCore::String& source_id) {
  if (webview_->client()) {
    webview_->client()->didAddMessageToConsole(
        WebConsoleMessage(static_cast<WebConsoleMessage::Level>(level),
                          webkit_glue::StringToWebString(message)),
        webkit_glue::StringToWebString(source_id),
        line_no);
  }
}

bool ChromeClientImpl::canRunBeforeUnloadConfirmPanel() {
  return webview_->client() != NULL;
}

bool ChromeClientImpl::runBeforeUnloadConfirmPanel(
    const WebCore::String& message,
    WebCore::Frame* frame) {
  if (webview_->client()) {
    return webview_->client()->runModalBeforeUnloadDialog(
        WebFrameImpl::FromFrame(frame),
        webkit_glue::StringToWebString(message));
  }
  return false;
}

void ChromeClientImpl::closeWindowSoon() {
  // Make sure this Page can no longer be found by JS.
  webview_->page()->setGroupName(WebCore::String());

  // Make sure that all loading is stopped.  Ensures that JS stops executing!
  webview_->mainFrame()->stopLoading();

  if (webview_->client())
    webview_->client()->closeWidgetSoon();
}

// Although a WebCore::Frame is passed in, we don't actually use it, since we
// already know our own webview_.
void ChromeClientImpl::runJavaScriptAlert(WebCore::Frame* frame,
                                          const WebCore::String& message) {
  if (webview_->client()) {
#if USE(V8)
    // Before showing the JavaScript dialog, we give the proxy implementation
    // a chance to process any pending console messages.
    WebCore::V8Proxy::processConsoleMessages();
#endif
    webview_->client()->runModalAlertDialog(
        WebFrameImpl::FromFrame(frame),
        webkit_glue::StringToWebString(message));
  }
}

// See comments for runJavaScriptAlert().
bool ChromeClientImpl::runJavaScriptConfirm(WebCore::Frame* frame,
                                            const WebCore::String& message) {
  if (webview_->client()) {
    return webview_->client()->runModalConfirmDialog(
        WebFrameImpl::FromFrame(frame),
        webkit_glue::StringToWebString(message));
  }
  return false;
}

// See comments for runJavaScriptAlert().
bool ChromeClientImpl::runJavaScriptPrompt(WebCore::Frame* frame,
                                           const WebCore::String& message,
                                           const WebCore::String& default_value,
                                           WebCore::String& result) {
  if (webview_->client()) {
    WebString actual_value;
    bool ok = webview_->client()->runModalPromptDialog(
        WebFrameImpl::FromFrame(frame),
        webkit_glue::StringToWebString(message),
        webkit_glue::StringToWebString(default_value),
        &actual_value);
    if (ok)
      result = webkit_glue::WebStringToString(actual_value);
    return ok;
  }
  return false;
}

void ChromeClientImpl::setStatusbarText(const WebCore::String& message) {
  if (webview_->client()) {
    webview_->client()->setStatusText(
        webkit_glue::StringToWebString(message));
  }
}

bool ChromeClientImpl::shouldInterruptJavaScript() {
  // TODO(mbelshe): implement me
  return false;
}

bool ChromeClientImpl::tabsToLinks() const {
  return webview_->tabsToLinks();
}

WebCore::IntRect ChromeClientImpl::windowResizerRect() const {
  WebCore::IntRect result;
  if (webview_->client()) {
    result = webkit_glue::WebRectToIntRect(
        webview_->client()->windowResizerRect());
  }
  return result;
}

void ChromeClientImpl::repaint(
    const WebCore::IntRect& paint_rect, bool content_changed, bool immediate,
    bool repaint_content_only) {
  // Ignore spurious calls.
  if (!content_changed || paint_rect.isEmpty())
    return;
  if (webview_->client()) {
    webview_->client()->didInvalidateRect(
        webkit_glue::IntRectToWebRect(paint_rect));
  }
}

void ChromeClientImpl::scroll(
    const WebCore::IntSize& scroll_delta, const WebCore::IntRect& scroll_rect,
    const WebCore::IntRect& clip_rect) {
  if (webview_->client()) {
    int dx = scroll_delta.width();
    int dy = scroll_delta.height();
    webview_->client()->didScrollRect(
        dx, dy, webkit_glue::IntRectToWebRect(clip_rect));
  }
}

WebCore::IntPoint ChromeClientImpl::screenToWindow(
    const WebCore::IntPoint&) const {
  NOTIMPLEMENTED();
  return WebCore::IntPoint();
}

WebCore::IntRect ChromeClientImpl::windowToScreen(
    const WebCore::IntRect& rect) const {
  WebCore::IntRect screen_rect(rect);

  if (webview_->client()) {
    WebRect window_rect = webview_->client()->windowRect();
    screen_rect.move(window_rect.x, window_rect.y);
  }

  return screen_rect;
}

void ChromeClientImpl::contentsSizeChanged(WebCore::Frame* frame, const
    WebCore::IntSize& size) const {
  WebFrameImpl* webframe = WebFrameImpl::FromFrame(frame);
  if (webframe->client()) {
    webframe->client()->didChangeContentsSize(
        webframe, webkit_glue::IntSizeToWebSize(size));
  }
}

void ChromeClientImpl::scrollbarsModeDidChange() const {
}

void ChromeClientImpl::mouseDidMoveOverElement(
    const WebCore::HitTestResult& result, unsigned modifier_flags) {
  if (!webview_->client())
    return;
  // Find out if the mouse is over a link, and if so, let our UI know...
  if (result.isLiveLink() && !result.absoluteLinkURL().string().isEmpty()) {
    webview_->client()->setMouseOverURL(
        webkit_glue::KURLToWebURL(result.absoluteLinkURL()));
  } else {
    webview_->client()->setMouseOverURL(WebURL());
  }
}

void ChromeClientImpl::setToolTip(const WebCore::String& tooltip_text,
                                  WebCore::TextDirection dir) {
  if (!webview_->client())
    return;
  WebTextDirection text_direction = (dir == WebCore::RTL) ?
      WebKit::WebTextDirectionRightToLeft :
      WebKit::WebTextDirectionLeftToRight;
  webview_->client()->setToolTipText(
      webkit_glue::StringToWebString(tooltip_text), text_direction);
}

void ChromeClientImpl::print(WebCore::Frame* frame) {
  if (webview_->client())
    webview_->client()->printPage(WebFrameImpl::FromFrame(frame));
}

void ChromeClientImpl::exceededDatabaseQuota(WebCore::Frame* frame,
    const WebCore::String& databaseName) {
  // set a reasonable quota for now -- 5Mb should be enough for anybody
  // TODO(dglazkov): this should be configurable
  WebCore::SecurityOrigin* origin = frame->document()->securityOrigin();
  WebCore::DatabaseTracker::tracker().setQuota(origin, 1024 * 1024 * 5);
}

void ChromeClientImpl::runOpenPanel(WebCore::Frame* frame,
  PassRefPtr<WebCore::FileChooser> file_chooser) {
  WebViewClient* client = webview_->client();
  if (!client)
    return;

  bool multiple_files = file_chooser->allowsMultipleFiles();

  WebString suggestion;
  if (file_chooser->filenames().size() > 0)
    suggestion = webkit_glue::StringToWebString(file_chooser->filenames()[0]);

  WebFileChooserCompletionImpl* chooser_completion =
      new WebFileChooserCompletionImpl(file_chooser);
  bool ok = client->runFileChooser(multiple_files,
                                   WebString(),
                                   suggestion,
                                   chooser_completion);
  if (!ok) {
    // Choosing failed, so do callback with an empty list.
    chooser_completion->didChooseFile(WebVector<WebString>());
  }
}

void ChromeClientImpl::popupOpened(PopupContainer* popup_container,
                                   const WebCore::IntRect& bounds,
                                   bool activatable,
                                   bool handle_externally) {
  if (!webview_->client())
    return;

  WebWidget* webwidget;
  if (handle_externally) {
    WebPopupMenuInfo popup_info;
    GetPopupMenuInfo(popup_container, &popup_info);
    webwidget = webview_->client()->createPopupMenu(popup_info);
  } else {
    webwidget = webview_->client()->createPopupMenu(activatable);
  }

  static_cast<WebPopupMenuImpl*>(webwidget)->Init(
      popup_container, webkit_glue::IntRectToWebRect(bounds));
}

void ChromeClientImpl::SetCursor(const WebCursorInfo& cursor) {
  if (ignore_next_set_cursor_) {
    ignore_next_set_cursor_ = false;
    return;
  }

  if (webview_->client())
    webview_->client()->didChangeCursor(cursor);
}

void ChromeClientImpl::SetCursorForPlugin(const WebCursorInfo& cursor) {
  SetCursor(cursor);
  // Currently, Widget::setCursor is always called after this function in
  // EventHandler.cpp and since we don't want that we set a flag indicating
  // that the next SetCursor call is to be ignored.
  ignore_next_set_cursor_ = true;
}

void ChromeClientImpl::formStateDidChange(const WebCore::Node* node) {
  // The current history item is not updated yet.  That happens lazily when
  // WebFrame::currentHistoryItem is requested.
  WebFrameImpl* webframe = WebFrameImpl::FromFrame(node->document()->frame());
  if (webframe->client())
    webframe->client()->didUpdateCurrentHistoryItem(webframe);
}

void ChromeClientImpl::GetPopupMenuInfo(PopupContainer* popup_container,
                                        WebPopupMenuInfo* info) {
  const Vector<PopupItem*>& input_items = popup_container->popupData();

  WebVector<WebPopupMenuInfo::Item> output_items(input_items.size());

  for (size_t i = 0; i < input_items.size(); ++i) {
    const PopupItem& input_item = *input_items[i];
    WebPopupMenuInfo::Item& output_item = output_items[i];

    output_item.label = webkit_glue::StringToWebString(input_item.label);
    output_item.enabled = input_item.enabled;

    switch (input_item.type) {
      case PopupItem::TypeOption:
        output_item.type = WebPopupMenuInfo::Item::Option;
        break;
      case PopupItem::TypeGroup:
        output_item.type = WebPopupMenuInfo::Item::Group;
        break;
      case PopupItem::TypeSeparator:
        output_item.type = WebPopupMenuInfo::Item::Separator;
        break;
      default:
        NOTREACHED();
    }
  }

  info->itemHeight = popup_container->menuItemHeight();
  info->selectedIndex = popup_container->selectedIndex();
  info->items.swap(output_items);
}

#if ENABLE(NOTIFICATIONS)
WebCore::NotificationPresenter* ChromeClientImpl::notificationPresenter() const {
  return webview_->GetNotificationPresenter();
}
#endif
