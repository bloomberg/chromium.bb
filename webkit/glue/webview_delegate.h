// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// WebCore provides hooks for several kinds of functionality, allowing separate
// classes termed "delegates" to receive notifications (in the form of direct
// function calls) when certain events are about to occur or have just occurred.
// In some cases, the delegate implements the needed functionality; in others,
// the delegate has some control over the behavior but doesn't actually
// implement it.  For example, the UI delegate is responsible for showing a
// dialog box or otherwise handling a JavaScript window.alert() call, via the
// RunJavaScriptAlert() method. On the other hand, the editor delegate doesn't
// actually handle editing functionality, although it could (for example)
// override whether a content-editable node accepts editing focus by returning
// false from ShouldBeginEditing(). (It would also possible for a more
// special-purpose editing delegate to act on the edited node in some way, e.g.
// to highlight modified text in the DidChangeContents() method.)

// WebKit divides the delegated tasks into several different classes, but we
// combine them into a single WebViewDelegate. This single delegate encompasses
// the needed functionality of the WebKit UIDelegate, ContextMenuDelegate,
// PolicyDelegate, FrameLoadDelegate, and EditorDelegate; additional portions
// of ChromeClient and FrameLoaderClient not delegated in the WebKit
// implementation; and some WebView additions.

#ifndef WEBKIT_GLUE_WEBVIEW_DELEGATE_H_
#define WEBKIT_GLUE_WEBVIEW_DELEGATE_H_

#include <vector>

#include "webkit/api/public/WebDragOperation.h"
#include "webkit/api/public/WebFrame.h"
#include "webkit/api/public/WebTextDirection.h"
#include "webkit/api/public/WebWidgetClient.h"
#include "webkit/glue/context_menu.h"

namespace WebCore {
class AccessibilityObject;
}

namespace WebKit {
class WebDragData;
class WebNotificationPresenter;
class WebWidget;
struct WebPopupMenuInfo;
struct WebPoint;
struct WebRect;
}

class FilePath;
class SkBitmap;
class WebDevToolsAgentDelegate;
class WebView;
struct ContextMenuMediaParams;

// Interface passed in to the WebViewDelegate to receive notification of the
// result of an open file dialog.
class WebFileChooserCallback {
 public:
  WebFileChooserCallback() {}
  virtual ~WebFileChooserCallback() {}
  virtual void OnFileChoose(const std::vector<FilePath>& file_names) { }

 private:
  DISALLOW_COPY_AND_ASSIGN(WebFileChooserCallback);
};


// Inheritance here is somewhat weird, but since a WebView is a WebWidget,
// it makes sense that a WebViewDelegate is a WebWidgetClient.
class WebViewDelegate : virtual public WebKit::WebWidgetClient {
 public:
  // WebView additions -------------------------------------------------------

  // This method is called to create a new WebView.  The WebView should not be
  // made visible until the new WebView's Delegate has its Show method called.
  // The returned WebView pointer is assumed to be owned by the host window,
  // and the caller of CreateWebView should not release the given WebView.
  // |user_gesture| is true if a user action initiated this call.
  // |creator_url|, if nonempty, holds the security origin of the page creating
  // this WebView.
  virtual WebView* CreateWebView(WebView* webview,
                                 bool user_gesture,
                                 const GURL& creator_url) {
    return NULL;
  }

  // This method is called to create a new WebWidget to act as a popup
  // (like a drop-down menu).
  virtual WebKit::WebWidget* CreatePopupWidget(
      WebView* webview,
      bool activatable) {
    return NULL;
  }

  // Like CreatePopupWidget, except the actual widget is rendered by the
  // embedder using the supplied info.
  virtual WebKit::WebWidget* CreatePopupWidgetWithInfo(
      WebView* webview,
      const WebKit::WebPopupMenuInfo& info) {
    return NULL;
  }

  // Notifies how many matches have been found so far, for a given request_id.
  // |final_update| specifies whether this is the last update (all frames have
  // completed scoping).
  virtual void ReportFindInPageMatchCount(int count, int request_id,
                                          bool final_update) {
  }

  // Notifies the browser what tick-mark rect is currently selected. Parameter
  // |request_id| lets the recipient know which request this message belongs to,
  // so that it can choose to ignore the message if it has moved on to other
  // things. |selection_rect| is expected to have coordinates relative to the
  // top left corner of the web page area and represent where on the screen the
  // selection rect is currently located.
  virtual void ReportFindInPageSelection(int request_id,
                                         int active_match_ordinal,
                                         const WebKit::WebRect& selection) {
  }

  // Returns whether this WebView was opened by a user gesture.
  virtual bool WasOpenedByUserGesture() const {
    return true;
  }

  // Called by ChromeClientImpl::focus() if accessibility on the renderer side
  // is enabled, and a focus change has occurred. Will retrieve the id of the
  // input AccessibilityObject and send it through IPC for handling on the
  // browser side.
  virtual void FocusAccessibilityObject(WebCore::AccessibilityObject* acc_obj) {
  }

  // FrameLoaderClient -------------------------------------------------------

  virtual bool CanAcceptLoadDrops() const {
    // Always return true here so layout tests (which use the default WebView
    // delegate) continue to pass.
    return true;
  }

  // Notifies the delegate that a load has begun.
  virtual void DidStartLoading(WebView* webview) {
  }

  // Notifies the delegate that all loads are finished.
  virtual void DidStopLoading(WebView* webview) {
  }

  // Notifies that a new script context has been created for this frame.
  // This is similar to WindowObjectCleared but only called once per frame
  // context.
  virtual void DidCreateScriptContextForFrame(WebKit::WebFrame* webframe) {
  }

  // Notifies that this frame's script context has been destroyed.
  virtual void DidDestroyScriptContextForFrame(WebKit::WebFrame* webframe) {
  }

  // Notifies that a garbage-collected context was created - content scripts.
  virtual void DidCreateIsolatedScriptContext(WebKit::WebFrame* webframe) {
  }

  // ChromeClient ------------------------------------------------------------

  // Appends a line to the application's error console.  The message contains
  // an error description or other information, the line_no provides a line
  // number (e.g. for a JavaScript error report), and the source_id contains
  // a URL or other description of the source of the message.
  virtual void AddMessageToConsole(WebView* webview,
                                   const std::wstring& message,
                                   unsigned int line_no,
                                   const std::wstring& source_id) {
  }

  // Queries the browser for suggestions to be shown for the form text field
  // named |field_name|.  |text| is the text entered by the user so far and
  // |node_id| is the id of the node of the input field.
  virtual void QueryFormFieldAutofill(const std::wstring& field_name,
                                      const std::wstring& text,
                                      int64 node_id) {
  }

  // Instructs the browser to remove the autofill entry specified from it DB.
  virtual void RemoveStoredAutofillEntry(const std::wstring& name,
                                         const std::wstring& value) {
  }

  // Called to retrieve the provider of desktop notifications.  Pointer
  // is owned by the implementation of WebViewDelegate.
  virtual WebKit::WebNotificationPresenter* GetNotificationPresenter() {
    return NULL;
  }

  // UIDelegate --------------------------------------------------------------

  // Displays a JavaScript alert panel associated with the given view. Clients
  // should visually indicate that this panel comes from JavaScript and some
  // information about the originating frame (at least the domain). The panel
  // should have a single OK button.
  virtual void RunJavaScriptAlert(WebKit::WebFrame* webframe,
                                  const std::wstring& message) {
  }

  // Displays a JavaScript confirm panel associated with the given view.
  // Clients should visually indicate that this panel comes
  // from JavaScript. The panel should have two buttons, e.g. "OK" and
  // "Cancel". Returns true if the user hit OK, or false if the user hit Cancel.
  virtual bool RunJavaScriptConfirm(WebKit::WebFrame* webframe,
                                    const std::wstring& message) {
    return false;
  }

  // Displays a JavaScript text input panel associated with the given view.
  // Clients should visually indicate that this panel comes from JavaScript.
  // The panel should have two buttons, e.g. "OK" and "Cancel", and an area to
  // type text. The default_value should appear as the initial text in the
  // panel when it is shown. If the user hit OK, returns true and fills result
  // with the text in the box.  The value of result is undefined if the user
  // hit Cancel.
  virtual bool RunJavaScriptPrompt(WebKit::WebFrame* webframe,
                                   const std::wstring& message,
                                   const std::wstring& default_value,
                                   std::wstring* result) {
    return false;
  }

  // Sets the status bar text.
  virtual void SetStatusbarText(WebView* webview,
                                const std::wstring& message) { }

  // Displays a "before unload" confirm panel associated with the given view.
  // The panel should have two buttons, e.g. "OK" and "Cancel", where OK means
  // that the navigation should continue, and Cancel means that the navigation
  // should be cancelled, leaving the user on the current page.  Returns true
  // if the user hit OK, or false if the user hit Cancel.
  virtual bool RunBeforeUnloadConfirm(WebKit::WebFrame* webframe,
                                      const std::wstring& message) {
    return true;  // OK, continue to navigate away
  }

  // Tells the client that we're hovering over a link with a given URL,
  // if the node is not a link, the URL will be an empty GURL.
  virtual void UpdateTargetURL(WebView* webview,
                               const GURL& url) {
  }

  // Called to display a file chooser prompt.  The prompt should be pre-
  // populated with the given initial_filename string.  The WebViewDelegate
  // will own the WebFileChooserCallback object and is responsible for
  // freeing it.
  virtual void RunFileChooser(bool multi_select,
                              const string16& title,
                              const FilePath& initial_filename,
                              WebFileChooserCallback* file_chooser) {
    delete file_chooser;
  }

  // @abstract Shows a context menu with commands relevant to a specific
  //           element on the current page.
  // @param webview The WebView sending the delegate method.
  // @param node_type The type of the node(s) the context menu is being
  // invoked on
  // @param x The x position of the mouse pointer (relative to the webview)
  // @param y The y position of the mouse pointer (relative to the webview)
  // @param link_url The absolute URL of the link that contains the node the
  // mouse right clicked on
  // @param image_url The absolute URL of the image that the mouse right
  // clicked on
  // @param page_url The URL of the page the mouse right clicked on
  // @param frame_url The URL of the subframe the mouse right clicked on
  // @param media_params Extra attributed needed by the context menu for
  // media elements.
  // @param selection_text The raw text of the selection that the mouse right
  // clicked on
  // @param misspelled_word The editable (possibily) misspelled word
  // in the Editor on which dictionary lookup for suggestions will be done.
  // @param edit_flags which edit operations the renderer believes are available
  // @param security_info
  // @param frame_charset which indicates the character encoding of
  // the currently focused frame.
  virtual void ShowContextMenu(WebView* webview,
                               ContextNodeType node_type,
                               int x,
                               int y,
                               const GURL& link_url,
                               const GURL& image_url,
                               const GURL& page_url,
                               const GURL& frame_url,
                               const ContextMenuMediaParams& media_params,
                               const std::wstring& selection_text,
                               const std::wstring& misspelled_word,
                               int edit_flags,
                               const std::string& security_info,
                               const std::string& frame_charset) {
  }

  // Starts a drag session with the supplied contextual information.
  // webview: The WebView sending the delegate method.
  // mouseCoords: Current mouse coordinates
  // drop_data: a WebDropData struct which should contain all the necessary
  // information for dragging data out of the webview.
  // drag_source_operation_mask: indicates what drag operations are allowed
  virtual void StartDragging(WebView* webview,
                             const WebKit::WebPoint &mouseCoords,
                             const WebKit::WebDragData& drag_data,
                             WebKit::WebDragOperationsMask operations_mask) {
  }

  // Returns the focus to the client.
  // reverse: Whether the focus should go to the previous (if true) or the next
  // focusable element.
  virtual void TakeFocus(WebView* webview, bool reverse) {
  }

  // Notification that a user metric has occurred.
  virtual void UserMetricsRecordAction(const std::wstring& action) { }

  // -------------------------------------------------------------------------

  // Notification that a request to download an image has completed. |errored|
  // indicates if there was a network error. The image is empty if there was
  // a network error, the contents of the page couldn't by converted to an
  // image, or the response from the host was not 200.
  // NOTE: image is empty if the response didn't contain image data.
  virtual void DidDownloadImage(int id,
                                const GURL& image_url,
                                bool errored,
                                const SkBitmap& image) {
  }

  // History Related ---------------------------------------------------------

  // Tells the embedder to navigate back or forward in session history by the
  // given offset (relative to the current position in session history).
  virtual void NavigateBackForwardSoon(int offset) {
  }

  // Returns how many entries are in the back and forward lists, respectively.
  virtual int GetHistoryBackListCount() {
    return 0;
  }
  virtual int GetHistoryForwardListCount() {
    return 0;
  }

  // -------------------------------------------------------------------------

  // Tell the delegate the tooltip text and its directionality hint for the
  // current mouse position.
  virtual void SetTooltipText(WebView* webview,
                              const std::wstring& tooltip_text,
                              WebKit::WebTextDirection text_direction_hint) { }

  // Downloading -------------------------------------------------------------

  virtual void DownloadUrl(const GURL& url, const GURL& referrer) { }

  // InspectorClient ---------------------------------------------------------

  virtual void UpdateInspectorSettings(const std::wstring& raw_settings) { }

  // DevTools ----------------------------------------------------------------

  virtual WebDevToolsAgentDelegate* GetWebDevToolsAgentDelegate() {
    return NULL;
  }

  // Editor Client -----------------------------------------------------------

  // Returns true if the word is spelled correctly. The word may begin or end
  // with whitespace or punctuation, so the implementor should be sure to handle
  // these cases.
  //
  // If the word is misspelled (returns false), the given first and last
  // indices (inclusive) will be filled with the offsets of the boundary of the
  // word within the given buffer. The out pointers must be specified. If the
  // word is correctly spelled (returns true), they will be set to (0,0).
  virtual void SpellCheck(const std::wstring& word, int tag,
                          int* misspell_location,
                          int* misspell_length) {
    *misspell_location = *misspell_length = 0;
  }

  // Computes an auto correct word for a misspelled word. If no word is found,
  // empty string is computed.
  virtual std::wstring GetAutoCorrectWord(const std::wstring& misspelled_word,
                                          int tag) {
    return std::wstring();
  }

  // Returns the document tag for the current WebView.
  virtual int SpellCheckerDocumentTag() { return 0; }

  // Switches the spelling panel to be displayed or not based on |show|.
  virtual void ShowSpellingUI(bool show) { }

  // Update the spelling panel with the |word|.
  virtual void UpdateSpellingUIWithMisspelledWord(const std::wstring& word) { }

  // Asks the user to print the page or a specific frame. Called in response to
  // a window.print() call.
  virtual void ScriptedPrint(WebKit::WebFrame* frame) { }

  // Called when an item was added to the history
  virtual void DidAddHistoryItem() { }

  // The "CurrentKeyboardEvent" refers to the keyboard event passed to
  // WebView's handleInputEvent method.
  //
  // This method is called in response to WebView's handleInputEvent() when
  // the default action for the current keyboard event is not suppressed by the
  // page, to give WebViewDelegate a chance to handle the keyboard event
  // specially.
  //
  // Returns true if the keyboard event was handled by WebViewDelegate.
  virtual bool HandleCurrentKeyboardEvent() {
    return false;
  }

 protected:
  ~WebViewDelegate() { }
};

#endif  // WEBKIT_GLUE_WEBVIEW_DELEGATE_H_
