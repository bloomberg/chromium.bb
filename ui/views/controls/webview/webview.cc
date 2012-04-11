// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/webview/webview.h"

#include "content/public/browser/browser_context.h"
#include "ipc/ipc_message.h"
#include "ui/views/controls/native/native_view_host.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
// WebView, public:

WebView::WebView(content::BrowserContext* browser_context)
    : wcv_holder_(new NativeViewHost),
      browser_context_(browser_context) {
  Init();
}

WebView::~WebView() {
}

////////////////////////////////////////////////////////////////////////////////
// WebView, private:

void WebView::Init() {
  AddChildView(wcv_holder_);
  web_contents_.reset(
      content::WebContents::Create(browser_context_, NULL, MSG_ROUTING_NONE,
                                   NULL, NULL));
}

////////////////////////////////////////////////////////////////////////////////
// WebView, View overrides:

void WebView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  wcv_holder_->SetSize(bounds().size());
}

void WebView::ViewHierarchyChanged(bool is_add, View* parent, View* child) {
  if (is_add && child == this)
    wcv_holder_->Attach(web_contents_->GetNativeView());
}

}  // namespace views
