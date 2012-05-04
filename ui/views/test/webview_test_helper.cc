// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/webview_test_helper.h"

#include "base/message_loop.h"
#include "content/test/mock_render_process_host.h"
#include "content/test/test_browser_thread.h"
#include "content/test/test_content_client_initializer.h"
#include "content/test/test_render_view_host_factory.h"
#include "ui/views/controls/webview/webview.h"

namespace views {

WebViewTestHelper::WebViewTestHelper(MessageLoopForUI* ui_loop) {
  test_content_client_initializer_.reset(
      new content::TestContentClientInitializer);

  // Setup to register a new RenderViewHost factory which manufactures
  // mock render process hosts. This ensures that we never create a 'real'
  // render view host since support for it doesn't exist in unit tests.
  rph_factory_.reset(new content::MockRenderProcessHostFactory());
  rvh_factory_.reset(
      new content::TestRenderViewHostFactory(rph_factory_.get()));

  ui_thread_.reset(
      new content::TestBrowserThread(content::BrowserThread::UI, ui_loop));
}

WebViewTestHelper::~WebViewTestHelper() {
}

}  // namespace views
