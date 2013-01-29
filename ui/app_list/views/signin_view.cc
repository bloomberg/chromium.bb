// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/signin_view.h"

#include "content/public/browser/web_contents.h"
#include "ui/app_list/signin_delegate.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"

namespace app_list {

SigninView::SigninView(SigninDelegate* delegate)
    : web_view_(new views::WebView(NULL)), delegate_(delegate) {
  SetLayoutManager(new views::FillLayout());
  AddChildView(web_view_);
}

SigninView::~SigninView() {
  web_view_->SetWebContents(NULL);
}

void SigninView::BeginSignin() {
  if (!delegate_)
    return;

  web_contents_.reset(delegate_->PrepareForSignin());
  web_view_->SetWebContents(web_contents_.get());
}

}  // namespace app_list
