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
    : web_view_(new views::WebView(NULL)) {
  DCHECK(delegate);
  SetLayoutManager(new views::FillLayout());
  AddChildView(web_view_);
  web_contents_.reset(delegate->PrepareForSignin());
}

SigninView::~SigninView() {
  web_view_->SetWebContents(NULL);
}

void SigninView::ViewHierarchyChanged(bool is_add,
                                      views::View* parent,
                                      views::View* child) {
  if (is_add && child == this)
    web_view_->SetWebContents(web_contents_.get());
}

}  // namespace app_list
