// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "webkit/tools/test_shell/webview_host.h"
#include "webkit/tools/test_shell/mac/test_shell_webview.h"

#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebSize.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSettings.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"
#include "webkit/glue/webpreferences.h"
#include "webkit/tools/test_shell/test_shell.h"
#include "webkit/tools/test_shell/test_webview_delegate.h"

using WebKit::WebDevToolsAgentClient;
using WebKit::WebSize;
using WebKit::WebView;

// static
WebViewHost* WebViewHost::Create(NSView* parent_view,
                                 TestWebViewDelegate* delegate,
                                 WebDevToolsAgentClient* dev_tools_client,
                                 const webkit_glue::WebPreferences& prefs) {
  WebViewHost* host = new WebViewHost();

  NSRect content_rect = [parent_view frame];
  // bump down the top of the view so that it doesn't overlap the buttons
  // and URL field.  32 is an ad hoc constant.
  // TODO(awalker): replace explicit view layout with a little nib file
  // and use that for view geometry.
  content_rect.size.height -= 32;
  host->view_ = [[TestShellWebView alloc] initWithFrame:content_rect];
  // make the height and width track the window size.
  [host->view_ setAutoresizingMask:(NSViewWidthSizable | NSViewHeightSizable)];
  [parent_view addSubview:host->view_];
  [host->view_ release];

  host->webwidget_ = WebView::create(delegate);
  host->webview()->setDevToolsAgentClient(dev_tools_client);
  prefs.Apply(host->webview());
  host->webview()->settings()->setExperimentalCSSGridLayoutEnabled(true);
  host->webview()->initializeMainFrame(delegate);
  host->webwidget_->resize(WebSize(NSWidth(content_rect),
                                   NSHeight(content_rect)));

  return host;
}

WebView* WebViewHost::webview() const {
  return static_cast<WebView*>(webwidget_);
}

void WebViewHost::SetIsActive(bool active) {
  // Ignore calls in layout test mode so that tests don't mess with each other
  // when running in parallel.
  if (!TestShell::layout_test_mode())
    webview()->setIsActive(active);
}
