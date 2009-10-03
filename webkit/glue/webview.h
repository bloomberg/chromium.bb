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

  // Notifies the webview that autofill suggestions are available for a node.
  virtual void AutofillSuggestionsForNode(
      int64 node_id,
      const std::vector<std::wstring>& suggestions,
      int default_suggestion_index) = 0;

  // Hides the autofill popup if any are showing.
  virtual void HideAutofillPopup() = 0;

  // Returns development tools agent instance belonging to this view.
  virtual WebDevToolsAgent* GetWebDevToolsAgent() = 0;

  // Performs an action from a context menu for the node at the given
  // location.
  virtual void MediaPlayerActionAt(int x,
                                   int y,
                                   const MediaPlayerAction& action) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebView);
};

#endif  // WEBKIT_GLUE_WEBVIEW_H_
