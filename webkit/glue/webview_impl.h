// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBVIEW_IMPL_H_
#define WEBKIT_GLUE_WEBVIEW_IMPL_H_

#include <set>
#include <string>
#include <vector>

#include "Page.h"

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "skia/ext/platform_canvas.h"
#include "webkit/api/public/WebPoint.h"
#include "webkit/api/public/WebSize.h"
#include "webkit/api/public/WebString.h"
#include "webkit/api/src/NotificationPresenterImpl.h"
#include "webkit/glue/back_forward_list_client_impl.h"
#include "webkit/glue/chrome_client_impl.h"
#include "webkit/glue/context_menu_client_impl.h"
#include "webkit/glue/dragclient_impl.h"
#include "webkit/glue/editor_client_impl.h"
#include "webkit/glue/inspector_client_impl.h"
#include "webkit/glue/webframe_impl.h"
#include "webkit/glue/webpreferences.h"
#include "webkit/glue/webview.h"

namespace WebCore {
class ChromiumDataObject;
class Frame;
class HistoryItem;
class HitTestResult;
class KeyboardEvent;
class Page;
class PlatformKeyboardEvent;
class PopupContainer;
class Range;
class RenderTheme;
class Widget;
}

namespace WebKit {
class WebAccessibilityObject;
class WebKeyboardEvent;
class WebMouseEvent;
class WebMouseWheelEvent;
class WebSettingsImpl;
}

namespace webkit_glue {
class ImageResourceFetcher;
}

class AutocompletePopupMenuClient;
class SearchableFormData;
class WebHistoryItemImpl;
class WebDevToolsAgent;
class WebDevToolsAgentImpl;
class WebViewDelegate;

class WebViewImpl : public WebView, public base::RefCounted<WebViewImpl> {
 public:
  // WebWidget methods:
  virtual void close();
  virtual WebKit::WebSize size() { return size_; }
  virtual void resize(const WebKit::WebSize& new_size);
  virtual void layout();
  virtual void paint(WebKit::WebCanvas* canvas,
                     const WebKit::WebRect& rect);
  virtual bool handleInputEvent(const WebKit::WebInputEvent& input_event);
  virtual void mouseCaptureLost();
  virtual void setFocus(bool enable);
  virtual bool handleCompositionEvent(WebKit::WebCompositionCommand command,
                                      int cursor_position,
                                      int target_start,
                                      int target_end,
                                      const WebKit::WebString& text);
  virtual bool queryCompositionStatus(bool* enabled,
                                      WebKit::WebRect* caret_rect);
  virtual void setTextDirection(WebKit::WebTextDirection direction);

  // WebView methods:
  virtual void initializeMainFrame(WebKit::WebFrameClient*);
  virtual WebKit::WebSettings* settings();
  virtual WebKit::WebString pageEncoding() const;
  virtual void setPageEncoding(const WebKit::WebString& encoding);
  virtual bool isTransparent() const;
  virtual void setIsTransparent(bool value);
  virtual bool tabsToLinks() const;
  virtual void setTabsToLinks(bool value);
  virtual bool tabKeyCyclesThroughElements() const;
  virtual void setTabKeyCyclesThroughElements(bool value);
  virtual bool isActive() const;
  virtual void setIsActive(bool value);
  virtual bool dispatchBeforeUnloadEvent();
  virtual void dispatchUnloadEvent();
  virtual WebKit::WebFrame* mainFrame();
  virtual WebKit::WebFrame* findFrameByName(
      const WebKit::WebString& name, WebKit::WebFrame* relative_to_frame);
  virtual WebKit::WebFrame* focusedFrame();
  virtual void setFocusedFrame(WebKit::WebFrame* frame);
  virtual void setInitialFocus(bool reverse);
  virtual void clearFocusedNode();
  virtual void zoomIn(bool text_only);
  virtual void zoomOut(bool text_only);
  virtual void zoomDefault();
  virtual void performMediaPlayerAction(
      const WebKit::WebMediaPlayerAction& action,
      const WebKit::WebPoint& location);
  virtual void copyImageAt(const WebKit::WebPoint& point);
  virtual void dragSourceEndedAt(
      const WebKit::WebPoint& client_point,
      const WebKit::WebPoint& screen_point,
      WebKit::WebDragOperation operation);
  virtual void dragSourceMovedTo(
      const WebKit::WebPoint& client_point,
      const WebKit::WebPoint& screen_point);
  virtual void dragSourceSystemDragEnded();
  virtual WebKit::WebDragOperation dragTargetDragEnter(
      const WebKit::WebDragData& drag_data, int identity,
      const WebKit::WebPoint& client_point,
      const WebKit::WebPoint& screen_point,
      WebKit::WebDragOperationsMask operations_allowed);
  virtual WebKit::WebDragOperation dragTargetDragOver(
      const WebKit::WebPoint& client_point,
      const WebKit::WebPoint& screen_point,
      WebKit::WebDragOperationsMask operations_allowed);
  virtual void dragTargetDragLeave();
  virtual void dragTargetDrop(
      const WebKit::WebPoint& client_point,
      const WebKit::WebPoint& screen_point);
  virtual int dragIdentity();
  virtual bool setDropEffect(bool accept);
  virtual void inspectElementAt(const WebKit::WebPoint& point);
  virtual WebKit::WebString inspectorSettings() const;
  virtual void setInspectorSettings(const WebKit::WebString& settings);
  virtual WebKit::WebAccessibilityObject accessibilityObject();
  virtual void applyAutofillSuggestions(
      const WebKit::WebNode&,
      const WebKit::WebVector<WebKit::WebString>& suggestions,
      int defaultSuggestionIndex);
  virtual void hideAutofillPopup();

  // WebView methods:
  virtual void SetIgnoreInputEvents(bool new_value);
  virtual WebDevToolsAgent* GetWebDevToolsAgent();
  WebDevToolsAgentImpl* GetWebDevToolsAgentImpl();

  // WebViewImpl

  const WebKit::WebPoint& last_mouse_down_point() const {
      return last_mouse_down_point_;
  }

  WebCore::Frame* GetFocusedWebCoreFrame();

  // Returns the currently focused Node or NULL if no node has focus.
  WebCore::Node* GetFocusedNode();

  static WebViewImpl* FromPage(WebCore::Page* page);

  WebKit::WebViewClient* client() {
    return delegate_;
  }

  // TODO(darin): Remove this method in favor of client().
  WebViewDelegate* delegate() {
    return delegate_;
  }

  // Returns the page object associated with this view.  This may be NULL when
  // the page is shutting down, but will be valid at all other times.
  WebCore::Page* page() const {
    return page_.get();
  }

  WebCore::RenderTheme* theme() const;

  // Returns the main frame associated with this view.  This may be NULL when
  // the page is shutting down, but will be valid at all other times.
  WebFrameImpl* main_frame() {
    return page_.get() ? WebFrameImpl::FromFrame(page_->mainFrame()) : NULL;
  }

  // History related methods:
  void SetCurrentHistoryItem(WebCore::HistoryItem* item);
  WebCore::HistoryItem* GetPreviousHistoryItem();
  void ObserveNewNavigation();

  // Event related methods:
  void MouseMove(const WebKit::WebMouseEvent& mouse_event);
  void MouseLeave(const WebKit::WebMouseEvent& mouse_event);
  void MouseDown(const WebKit::WebMouseEvent& mouse_event);
  void MouseUp(const WebKit::WebMouseEvent& mouse_event);
  void MouseContextMenu(const WebKit::WebMouseEvent& mouse_event);
  void MouseDoubleClick(const WebKit::WebMouseEvent& mouse_event);
  void MouseWheel(const WebKit::WebMouseWheelEvent& wheel_event);
  bool KeyEvent(const WebKit::WebKeyboardEvent& key_event);
  bool CharEvent(const WebKit::WebKeyboardEvent& key_event);

  // Handles context menu events orignated via the the keyboard. These
  // include the VK_APPS virtual key and the Shift+F10 combine.
  // Code is based on the Webkit function
  // bool WebView::handleContextMenuEvent(WPARAM wParam, LPARAM lParam) in
  // webkit\webkit\win\WebView.cpp. The only significant change in this
  // function is the code to convert from a Keyboard event to the Right
  // Mouse button down event.
  bool SendContextMenuEvent(const WebKit::WebKeyboardEvent& event);

  // Notifies the WebView that a load has been committed.
  // is_new_navigation will be true if a new session history item should be
  // created for that load.
  void DidCommitLoad(bool* is_new_navigation);

  bool context_menu_allowed() const {
    return context_menu_allowed_;
  }

  // Set the disposition for how this webview is to be initially shown.
  void set_initial_navigation_policy(WebKit::WebNavigationPolicy policy) {
    initial_navigation_policy_ = policy;
  }
  WebKit::WebNavigationPolicy initial_navigation_policy() const {
    return initial_navigation_policy_;
  }

  // Determines whether a page should e.g. be opened in a background tab.
  // Returns false if it has no opinion, in which case it doesn't set *policy.
  static bool NavigationPolicyFromMouseEvent(
      unsigned short button,
      bool ctrl,
      bool shift,
      bool alt,
      bool meta,
      WebKit::WebNavigationPolicy* policy);

  // Start a system drag and drop operation.
  void StartDragging(
      const WebKit::WebPoint& event_pos,
      const WebKit::WebDragData& drag_data,
      WebKit::WebDragOperationsMask drag_source_operation_mask);

  // Hides the autocomplete popup if it is showing.
  void HideAutoCompletePopup();
  void AutoCompletePopupDidHide();

  // Converts |x|, |y| from window coordinates to contents coordinates and gets
  // the underlying Node for them.
  WebCore::Node* GetNodeForWindowPos(int x, int y);

#if ENABLE(NOTIFICATIONS)
  // Returns the provider of desktop notifications.
  WebKit::NotificationPresenterImpl* GetNotificationPresenter();
#endif

  // Tries to scroll a frame or any parent of a frame. Returns true if the view
  // was scrolled.
  bool PropagateScroll(WebCore::ScrollDirection scroll_direction,
                       WebCore::ScrollGranularity scroll_granularity);

 protected:
  friend class WebView;  // So WebView::Create can call our constructor
  friend class base::RefCounted<WebViewImpl>;

  WebViewImpl(WebViewDelegate* delegate);
  ~WebViewImpl();

  void ModifySelection(uint32 message,
                       WebCore::Frame* frame,
                       const WebCore::PlatformKeyboardEvent& e);

  WebViewDelegate* delegate_;

  webkit_glue::BackForwardListClientImpl back_forward_list_client_impl_;
  ChromeClientImpl chrome_client_impl_;
  ContextMenuClientImpl context_menu_client_impl_;
  DragClientImpl drag_client_impl_;
  EditorClientImpl editor_client_impl_;
  InspectorClientImpl inspector_client_impl_;

  WebKit::WebSize size_;

  WebKit::WebPoint last_mouse_position_;
  scoped_ptr<WebCore::Page> page_;

  // This flag is set when a new navigation is detected.  It is used to satisfy
  // the corresponding argument to WebViewDelegate::DidCommitLoadForFrame.
  bool observed_new_navigation_;
#ifndef NDEBUG
  // Used to assert that the new navigation we observed is the same navigation
  // when we make use of observed_new_navigation_.
  const WebCore::DocumentLoader* new_navigation_loader_;
#endif

  // A copy of the WebPreferences object we receive from the browser.
  WebPreferences webprefs_;

  // An object that can be used to manipulate page_->settings() without linking
  // against WebCore.  This is lazily allocated the first time GetWebSettings()
  // is called.
  scoped_ptr<WebKit::WebSettingsImpl> web_settings_;

  // A copy of the web drop data object we received from the browser.
  RefPtr<WebCore::ChromiumDataObject> current_drag_data_;

 private:
  // Returns true if the event was actually processed.
  bool KeyEventDefault(const WebKit::WebKeyboardEvent& event);

  // Returns true if the autocomple has consumed the event.
  bool AutocompleteHandleKeyEvent(const WebKit::WebKeyboardEvent& event);

  // Repaints the autofill popup.  Should be called when the suggestions have
  // changed.  Note that this should only be called when the autofill popup is
  // showing.
  void RefreshAutofillPopup();

  // Returns true if the view was scrolled.
  bool ScrollViewWithKeyboard(int key_code, int modifiers);

  // Converts |pos| from window coordinates to contents coordinates and gets
  // the HitTestResult for it.
  WebCore::HitTestResult HitTestResultForWindowPos(
      const WebCore::IntPoint& pos);

  // The point relative to the client area where the mouse was last pressed
  // down. This is used by the drag client to determine what was under the
  // mouse when the drag was initiated. We need to track this here in
  // WebViewImpl since DragClient::startDrag does not pass the position the
  // mouse was at when the drag was initiated, only the current point, which
  // can be misleading as it is usually not over the element the user actually
  // dragged by the time a drag is initiated.
  WebKit::WebPoint last_mouse_down_point_;

  // Keeps track of the current text zoom level.  0 means no zoom, positive
  // values mean larger text, negative numbers mean smaller.
  int zoom_level_;

  bool context_menu_allowed_;

  bool doing_drag_and_drop_;

  bool ignore_input_events_;

  // Webkit expects keyPress events to be suppressed if the associated keyDown
  // event was handled. Safari implements this behavior by peeking out the
  // associated WM_CHAR event if the keydown was handled. We emulate
  // this behavior by setting this flag if the keyDown was handled.
  bool suppress_next_keypress_event_;

  // The policy for how this webview is to be initially shown.
  WebKit::WebNavigationPolicy initial_navigation_policy_;

  // Represents whether or not this object should process incoming IME events.
  bool ime_accept_events_;

  // True while dispatching system drag and drop events to drag/drop targets
  // within this WebView.
  bool drag_target_dispatch_;

  // Valid when drag_target_dispatch_ is true; the identity of the drag data
  // copied from the WebDropData object sent from the browser process.
  int32 drag_identity_;

  // Valid when drag_target_dispatch_ is true.  Used to override the default
  // browser drop effect with the effects "none" or "copy".
  enum DragTargetDropEffect {
    DROP_EFFECT_DEFAULT = -1,
    DROP_EFFECT_NONE,
    DROP_EFFECT_COPY
  } drop_effect_;

  // The available drag operations (copy, move link...) allowed by the source.
  WebKit::WebDragOperation operations_allowed_;

  // The current drag operation as negotiated by the source and destination.
  // When not equal to DragOperationNone, the drag data can be dropped onto the
  // current drop target in this WebView (the drop target can accept the drop).
  WebKit::WebDragOperation drag_operation_;

  // The autocomplete popup.  Kept around and reused every-time new suggestions
  // should be shown.
  RefPtr<WebCore::PopupContainer> autocomplete_popup_;

  // Whether the autocomplete popup is currently showing.
  bool autocomplete_popup_showing_;

  // The autocomplete client.
  scoped_ptr<AutocompletePopupMenuClient> autocomplete_popup_client_;

  scoped_ptr<WebDevToolsAgentImpl> devtools_agent_;

  // Whether the webview is rendering transparently.
  bool is_transparent_;

  // Whether the user can press tab to focus links.
  bool tabs_to_links_;

  // Inspector settings.
  WebKit::WebString inspector_settings_;

#if ENABLE(NOTIFICATIONS)
  // The provider of desktop notifications;
  WebKit::NotificationPresenterImpl notification_presenter_;
#endif

  // HACK: current_input_event is for ChromeClientImpl::show(), until we can fix
  // WebKit to pass enough information up into ChromeClient::show() so we can
  // decide if the window.open event was caused by a middle-mouse click
 public:
  static const WebKit::WebInputEvent* current_input_event() {
    return g_current_input_event;
  }
 private:
  static const WebKit::WebInputEvent* g_current_input_event;

  DISALLOW_COPY_AND_ASSIGN(WebViewImpl);
};

#endif  // WEBKIT_GLUE_WEBVIEW_IMPL_H_
