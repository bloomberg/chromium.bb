// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBVIEW_H_
#define WEBKIT_GLUE_WEBVIEW_H_

#include "base/basictypes.h"
#include "webkit/api/public/WebView.h"

class WebViewDelegate;

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

 private:
  DISALLOW_COPY_AND_ASSIGN(WebView);
};

#endif  // WEBKIT_GLUE_WEBVIEW_H_
