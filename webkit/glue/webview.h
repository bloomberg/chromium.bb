// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBVIEW_H_
#define WEBKIT_GLUE_WEBVIEW_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "webkit/api/public/WebDragOperation.h"
#include "webkit/api/public/WebView.h"

namespace WebKit {
class WebDragData;
class WebFrameClient;
class WebFrame;
class WebSettings;
struct WebPoint;
}

class GURL;
class WebDevToolsAgent;
class WebViewDelegate;
struct MediaPlayerAction;

//
//  @class WebView
//  WebView manages the interaction between WebFrameViews and WebDataSources.
//  Modification of the policies and behavior of the WebKit is largely managed
//  by WebViews and their delegates.
//
//  Typical usage:
//
//  WebView *webView;
//  WebFrame *mainFrame;
//
//  webView  = [[WebView alloc] initWithFrame: NSMakeRect (0,0,640,480)];
//  mainFrame = [webView mainFrame];
//  [mainFrame loadRequest:request];
//
//  WebViews have a WebViewDelegate that the embedding application implements
//  that are required for tasks like opening new windows and controlling the
//  user interface elements in those windows, monitoring the progress of loads,
//  monitoring URL changes, and making determinations about how content of
//  certain types should be handled.
class WebView : public WebKit::WebView {
 public:
  WebView() {}
  virtual ~WebView() {}

  // This method creates a WebView that is NOT yet initialized.  You will need
  // to call InitializeMainFrame to finish the initialization.  You may pass
  // NULL for the editing_client parameter if you are not interested in those
  // notifications.
  static WebView* Create(WebViewDelegate* delegate);

  // After creating a WebView, you should immediately call this function.  You
  // can optionally modify the settings (via GetSettings()) in between.  The
  // frame_client will receive events for the main frame and any child frames.
  virtual void InitializeMainFrame(WebKit::WebFrameClient* frame_client) = 0;

  // Tells all Page instances of this view to update the visited link state for
  // the specified hash.
  static void UpdateVisitedLinkState(uint64 link_hash);

  // Tells all Page instances of this view to update visited state for all their
  // links.
  static void ResetVisitedLinkState();

  // Returns the delegate for this WebView.  This is the pointer that was
  // passed to WebView::Initialize. The caller must check this value before
  // using it, it will be NULL during closing of the view.
  virtual WebViewDelegate* GetDelegate() = 0;

  // Method that controls whether pressing Tab key cycles through page elements
  // or inserts a '\t' char in text area
  virtual void SetTabKeyCyclesThroughElements(bool value) = 0;

  // Returns the frame previous to the specified frame, by traversing the frame
  // tree, wrapping around if necessary.
  virtual WebKit::WebFrame* GetPreviousFrameBefore(WebKit::WebFrame* frame, bool wrap) = 0;

  // Returns the frame after to the specified frame, by traversing the frame
  // tree, wrapping around if necessary.
  virtual WebKit::WebFrame* GetNextFrameAfter(WebKit::WebFrame* frame, bool wrap) = 0;

  // ---- TODO(darin): remove from here ----

  //
  //  - (IBAction)stopLoading:(id)sender;
  virtual void StopLoading() = 0;

  // Sets the maximum size to allow WebCore's internal B/F list to grow to.
  // If not called, the list will have the default capacity specified in
  // BackForwardList.cpp.
  virtual void SetBackForwardListSize(int size) = 0;

  // ---- TODO(darin): remove to here ----

  // Settings used by inspector.
  virtual const std::wstring& GetInspectorSettings() const = 0;
  virtual void SetInspectorSettings(const std::wstring& settings) = 0;

  // Show the JavaScript console.
  virtual void ShowJavaScriptConsole() = 0;

  // Helper method for drag and drop target operations: override the default
  // drop effect with either a "copy" (accept true) or "none" (accept false)
  // effect.  Return true on success.
  virtual bool SetDropEffect(bool accept) = 0;

  // Notifies the webview that autofill suggestions are available for a node.
  virtual void AutofillSuggestionsForNode(
      int64 node_id,
      const std::vector<std::wstring>& suggestions,
      int default_suggestion_index) = 0;

  // Hides the autofill popup if any are showing.
  virtual void HideAutofillPopup() = 0;

  // Returns development tools agent instance belonging to this view.
  virtual WebDevToolsAgent* GetWebDevToolsAgent() = 0;

  // Makes the webview transparent. Useful if you want to have some custom
  // background behind it.
  virtual void SetIsTransparent(bool is_transparent) = 0;
  virtual bool GetIsTransparent() const = 0;

  virtual void SetSpellingPanelVisibility(bool is_visible) = 0;
  virtual bool GetSpellingPanelVisibility() = 0;

  virtual void SetTabsToLinks(bool enable) = 0;
  virtual bool GetTabsToLinks() const = 0;

  // Performs an action from a context menu for the node at the given
  // location.
  virtual void MediaPlayerActionAt(int x,
                                   int y,
                                   const MediaPlayerAction& action) = 0;

  // Updates the WebView's active state (i.e., control tints).
  virtual void SetActive(bool active) = 0;

  // Gets the WebView's active state (i.e., state of control tints).
  virtual bool IsActive() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebView);
};

#endif  // WEBKIT_GLUE_WEBVIEW_H_
