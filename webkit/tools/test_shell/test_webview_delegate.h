// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TestWebViewDelegate class:
// This class implements the WebViewDelegate methods for the test shell.  One
// instance is owned by each TestShell.

#ifndef WEBKIT_TOOLS_TEST_SHELL_TEST_WEBVIEW_DELEGATE_H_
#define WEBKIT_TOOLS_TEST_SHELL_TEST_WEBVIEW_DELEGATE_H_

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

#include <map>
#include <string>

#if defined(TOOLKIT_USES_GTK)
#include <gdk/gdkcursor.h>
#endif

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "base/weak_ptr.h"
#include "third_party/WebKit/WebKit/chromium/public/WebContextMenuData.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFrameClient.h"
#include "third_party/WebKit/WebKit/chromium/public/WebRect.h"
#if defined(OS_MACOSX)
#include "third_party/WebKit/WebKit/chromium/public/WebPopupMenuInfo.h"
#endif
#include "third_party/WebKit/WebKit/chromium/public/WebViewClient.h"
#include "webkit/glue/webcursor.h"
#include "webkit/glue/webplugin_page_delegate.h"
#if defined(OS_WIN)
#include "webkit/tools/test_shell/drag_delegate.h"
#include "webkit/tools/test_shell/drop_delegate.h"
#endif
#include "webkit/tools/test_shell/test_navigation_controller.h"

struct WebPreferences;
class GURL;
class TestShell;
class WebWidgetHost;

class TestWebViewDelegate : public WebKit::WebViewClient,
                            public WebKit::WebFrameClient,
                            public webkit_glue::WebPluginPageDelegate,
                            public base::SupportsWeakPtr<TestWebViewDelegate> {
 public:
  struct CapturedContextMenuEvent {
    CapturedContextMenuEvent(int in_node_type,
                             int in_x,
                             int in_y)
      : node_type(in_node_type),
        x(in_x),
        y(in_y) {
    }

    int node_type;
    int x;
    int y;
  };

  typedef std::vector<CapturedContextMenuEvent> CapturedContextMenuEvents;

  // WebKit::WebViewClient
  virtual WebKit::WebView* createView(WebKit::WebFrame* creator);
  virtual WebKit::WebWidget* createPopupMenu(bool activatable);
  virtual WebKit::WebWidget* createPopupMenu(
      const WebKit::WebPopupMenuInfo& info);
  virtual void didAddMessageToConsole(
      const WebKit::WebConsoleMessage& message,
      const WebKit::WebString& source_name, unsigned source_line);
  virtual bool shouldBeginEditing(const WebKit::WebRange& range);
  virtual bool shouldEndEditing(const WebKit::WebRange& range);
  virtual bool shouldInsertNode(
      const WebKit::WebNode& node, const WebKit::WebRange& range,
      WebKit::WebEditingAction action);
  virtual bool shouldInsertText(
      const WebKit::WebString& text, const WebKit::WebRange& range,
      WebKit::WebEditingAction action);
  virtual bool shouldChangeSelectedRange(
      const WebKit::WebRange& from, const WebKit::WebRange& to,
      WebKit::WebTextAffinity affinity, bool still_selecting);
  virtual bool shouldDeleteRange(const WebKit::WebRange& range);
  virtual bool shouldApplyStyle(
      const WebKit::WebString& style, const WebKit::WebRange& range);
  virtual bool isSmartInsertDeleteEnabled();
  virtual bool isSelectTrailingWhitespaceEnabled();
  virtual void didBeginEditing();
  virtual void didChangeSelection(bool is_selection_empty);
  virtual void didChangeContents();
  virtual void didEndEditing();
  virtual bool handleCurrentKeyboardEvent();
  virtual WebKit::WebString autoCorrectWord(
      const WebKit::WebString& misspelled_word);
  virtual void runModalAlertDialog(
      WebKit::WebFrame* frame, const WebKit::WebString& message);
  virtual bool runModalConfirmDialog(
      WebKit::WebFrame* frame, const WebKit::WebString& message);
  virtual bool runModalPromptDialog(
      WebKit::WebFrame* frame, const WebKit::WebString& message,
      const WebKit::WebString& default_value, WebKit::WebString* actual_value);
  virtual bool runModalBeforeUnloadDialog(
      WebKit::WebFrame* frame, const WebKit::WebString& message);
  virtual void showContextMenu(
      WebKit::WebFrame* frame, const WebKit::WebContextMenuData& data);
  virtual void setStatusText(const WebKit::WebString& text);
  virtual void startDragging(
      const WebKit::WebPoint& from, const WebKit::WebDragData& data,
      WebKit::WebDragOperationsMask mask);
  virtual void navigateBackForwardSoon(int offset);
  virtual int historyBackListCount();
  virtual int historyForwardListCount();
  virtual void focusAccessibilityObject(
      const WebKit::WebAccessibilityObject& object);

  // WebKit::WebWidgetClient
  virtual void didInvalidateRect(const WebKit::WebRect& rect);
  virtual void didScrollRect(int dx, int dy,
                             const WebKit::WebRect& clip_rect);
  virtual void didFocus();
  virtual void didBlur();
  virtual void didChangeCursor(const WebKit::WebCursorInfo& cursor);
  virtual void closeWidgetSoon();
  virtual void show(WebKit::WebNavigationPolicy policy);
  virtual void runModal();
  virtual WebKit::WebRect windowRect();
  virtual void setWindowRect(const WebKit::WebRect& rect);
  virtual WebKit::WebRect rootWindowRect();
  virtual WebKit::WebRect windowResizerRect();
  virtual WebKit::WebScreenInfo screenInfo();

  // WebKit::WebFrameClient
  virtual WebKit::WebPlugin* createPlugin(
      WebKit::WebFrame*, const WebKit::WebPluginParams&);
  virtual WebKit::WebWorker* createWorker(
      WebKit::WebFrame*, WebKit::WebWorkerClient*);
  virtual WebKit::WebMediaPlayer* createMediaPlayer(
      WebKit::WebFrame*, WebKit::WebMediaPlayerClient*);
  virtual void loadURLExternally(
      WebKit::WebFrame*, const WebKit::WebURLRequest&,
      WebKit::WebNavigationPolicy);
  virtual WebKit::WebNavigationPolicy decidePolicyForNavigation(
      WebKit::WebFrame*, const WebKit::WebURLRequest&,
      WebKit::WebNavigationType, const WebKit::WebNode&,
      WebKit::WebNavigationPolicy default_policy, bool isRedirect);
  virtual bool canHandleRequest(
      WebKit::WebFrame*, const WebKit::WebURLRequest&);
  virtual WebKit::WebURLError cannotHandleRequestError(
      WebKit::WebFrame*, const WebKit::WebURLRequest& request);
  virtual WebKit::WebURLError cancelledError(
      WebKit::WebFrame*, const WebKit::WebURLRequest& request);
  virtual void unableToImplementPolicyWithError(
      WebKit::WebFrame*, const WebKit::WebURLError&);
  virtual void willPerformClientRedirect(
      WebKit::WebFrame*, const WebKit::WebURL& from, const WebKit::WebURL& to,
      double interval, double fire_time);
  virtual void didCancelClientRedirect(WebKit::WebFrame*);
  virtual void didCreateDataSource(
      WebKit::WebFrame*, WebKit::WebDataSource*);
  virtual void didStartProvisionalLoad(WebKit::WebFrame*);
  virtual void didReceiveServerRedirectForProvisionalLoad(WebKit::WebFrame*);
  virtual void didFailProvisionalLoad(
      WebKit::WebFrame*, const WebKit::WebURLError&);
  virtual void didCommitProvisionalLoad(
      WebKit::WebFrame*, bool is_new_navigation);
  virtual void didClearWindowObject(WebKit::WebFrame*);
  virtual void didReceiveTitle(
      WebKit::WebFrame*, const WebKit::WebString& title);
  virtual void didFinishDocumentLoad(WebKit::WebFrame*);
  virtual void didHandleOnloadEvents(WebKit::WebFrame*);
  virtual void didFailLoad(
      WebKit::WebFrame*, const WebKit::WebURLError&);
  virtual void didFinishLoad(WebKit::WebFrame*);
  virtual void didChangeLocationWithinPage(
      WebKit::WebFrame*, bool isNewNavigation);
  virtual void assignIdentifierToRequest(
      WebKit::WebFrame*, unsigned identifier, const WebKit::WebURLRequest&);
  virtual void willSendRequest(
      WebKit::WebFrame*, unsigned identifier, WebKit::WebURLRequest&,
      const WebKit::WebURLResponse& redirectResponse);
  virtual void didReceiveResponse(
      WebKit::WebFrame*, unsigned identifier, const WebKit::WebURLResponse&);
  virtual void didFinishResourceLoad(
      WebKit::WebFrame*, unsigned identifier);
  virtual void didFailResourceLoad(
      WebKit::WebFrame*, unsigned identifier, const WebKit::WebURLError&);
  virtual void didDisplayInsecureContent(WebKit::WebFrame* frame);
  virtual void didRunInsecureContent(
      WebKit::WebFrame* frame, const WebKit::WebSecurityOrigin& origin);

  // webkit_glue::WebPluginPageDelegate
  virtual webkit_glue::WebPluginDelegate* CreatePluginDelegate(
      const GURL& url,
      const std::string& mime_type,
      std::string* actual_mime_type);
  virtual void CreatedPluginWindow(
      gfx::PluginWindowHandle handle);
  virtual void WillDestroyPluginWindow(
      gfx::PluginWindowHandle handle);
  virtual void DidMovePlugin(
      const webkit_glue::WebPluginGeometry& move);
  virtual void DidStartLoadingForPlugin() {}
  virtual void DidStopLoadingForPlugin() {}
  virtual void ShowModalHTMLDialogForPlugin(
      const GURL& url,
      const gfx::Size& size,
      const std::string& json_arguments,
      std::string* json_retval) {}

  TestWebViewDelegate(TestShell* shell);
  ~TestWebViewDelegate();
  void Reset();

  void SetSmartInsertDeleteEnabled(bool enabled);
  void SetSelectTrailingWhitespaceEnabled(bool enabled);

  // Additional accessors
  WebKit::WebFrame* top_loading_frame() { return top_loading_frame_; }
#if defined(OS_WIN)
  IDropTarget* drop_delegate() { return drop_delegate_.get(); }
  IDropSource* drag_delegate() { return drag_delegate_.get(); }
#endif
  const CapturedContextMenuEvents& captured_context_menu_events() const {
    return captured_context_menu_events_;
  }
  void clear_captured_context_menu_events() {
    captured_context_menu_events_.clear();
  }

  void set_pending_extra_data(TestShellExtraData* extra_data) {
    pending_extra_data_.reset(extra_data);
  }

  // Methods for modifying WebPreferences
  void SetUserStyleSheetEnabled(bool is_enabled);
  void SetUserStyleSheetLocation(const GURL& location);

  // Sets the webview as a drop target.
  void RegisterDragDrop();
  void RevokeDragDrop();

  void ResetDragDrop();

  void SetCustomPolicyDelegate(bool is_custom, bool is_permissive);
  void WaitForPolicyDelegate();

  void set_block_redirects(bool block_redirects) {
    block_redirects_ = block_redirects;
  }
  bool block_redirects() const {
    return block_redirects_;
  }

  void SetEditCommand(const std::string& name, const std::string& value) {
    edit_command_name_ = name;
    edit_command_value_ = value;
  }

  void ClearEditCommand() {
    edit_command_name_.clear();
    edit_command_value_.clear();
  }

 private:

  // Called the title of the page changes.
  // Can be used to update the title of the window.
  void SetPageTitle(const std::wstring& title);

  // Called when the URL of the page changes.
  // Extracts the URL and forwards on to SetAddressBarURL().
  void UpdateAddressBar(WebKit::WebView* webView);

  // Called when the URL of the page changes.
  // Should be used to update the text of the URL bar.
  void SetAddressBarURL(const GURL& url);

  // Show a JavaScript alert as a popup message.
  // The caller should test whether we're in layout test mode and only
  // call this function when we really want a message to pop up.
  void ShowJavaScriptAlert(const std::wstring& message);

  // In the Mac code, this is called to trigger the end of a test after the
  // page has finished loading.  From here, we can generate the dump for the
  // test.
  void LocationChangeDone(WebKit::WebFrame*);

  // Tests that require moving or resizing the main window (via resizeTo() or
  // moveTo()) pass in Chrome even though Chrome disregards move requests for
  // non-popup windows (see TabContents::RequestMove()).  These functions allow
  // the test shell to mimic its behavior.  If setWindowRect() is called for
  // the main window, the passed in WebRect is saved as fake_rect_ and we return
  // it instead of the real window dimensions whenever rootWindowRect() is
  // called.
  WebKit::WebRect fake_window_rect();
  void set_fake_window_rect(const WebKit::WebRect&);

  WebWidgetHost* GetWidgetHost();

  void UpdateForCommittedLoad(WebKit::WebFrame* webframe, bool is_new_navigation);
  void UpdateURL(WebKit::WebFrame* frame);
  void UpdateSessionHistory(WebKit::WebFrame* frame);
  void UpdateSelectionClipboard(bool is_empty_selection);

  // Get a string suitable for dumping a frame to the console.
  std::wstring GetFrameDescription(WebKit::WebFrame* webframe);

  // Causes navigation actions just printout the intended navigation instead
  // of taking you to the page. This is used for cases like mailto, where you
  // don't actually want to open the mail program.
  bool policy_delegate_enabled_;

  // Toggles the behavior of the policy delegate.  If true, then navigations
  // will be allowed.  Otherwise, they will be ignored (dropped).
  bool policy_delegate_is_permissive_;

  // If true, the policy delegate will signal layout test completion.
  bool policy_delegate_should_notify_done_;

  // Non-owning pointer.  The delegate is owned by the host.
  TestShell* shell_;

  // This is non-NULL IFF a load is in progress.
  WebKit::WebFrame* top_loading_frame_;

  // For tracking session history.  See RenderView.
  int page_id_;
  int last_page_id_updated_;

  scoped_ptr<TestShellExtraData> pending_extra_data_;

  // Maps resource identifiers to a descriptive string.
  typedef std::map<uint32, std::string> ResourceMap;
  ResourceMap resource_identifier_map_;
  std::string GetResourceDescription(uint32 identifier);

  CapturedContextMenuEvents captured_context_menu_events_;

  WebCursor current_cursor_;

  WebKit::WebRect fake_rect_;
  bool using_fake_rect_;

#if defined(OS_WIN)
  // Classes needed by drag and drop.
  scoped_refptr<TestDragDelegate> drag_delegate_;
  scoped_refptr<TestDropDelegate> drop_delegate_;
#endif

#if defined(TOOLKIT_USES_GTK)
  // The type of cursor the window is currently using.
  // Used for judging whether a new SetCursor call is actually changing the
  // cursor.
  GdkCursorType cursor_type_;
#endif

#if defined(OS_MACOSX)
  scoped_ptr<WebKit::WebPopupMenuInfo> popup_menu_info_;
  WebKit::WebRect popup_bounds_;
#endif

  // true if we want to enable smart insert/delete.
  bool smart_insert_delete_enabled_;

  // true if we want to enable selection of trailing whitespaces
  bool select_trailing_whitespace_enabled_;

  // true if we should block any redirects
  bool block_redirects_;

  // Edit command associated to the current keyboard event.
  std::string edit_command_name_;
  std::string edit_command_value_;

  DISALLOW_COPY_AND_ASSIGN(TestWebViewDelegate);
};

#endif  // WEBKIT_TOOLS_TEST_SHELL_TEST_WEBVIEW_DELEGATE_H_
