// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/navigation/view_impl.h"

#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace navigation {

ViewImpl::ViewImpl(content::BrowserContext* browser_context,
                   mojom::ViewClientPtr client,
                   mojom::ViewRequest request,
                   std::unique_ptr<shell::ShellConnectionRef> ref)
    : binding_(this, std::move(request)),
      client_(std::move(client)),
      ref_(std::move(ref)) {
  web_contents_.reset(content::WebContents::Create(
      content::WebContents::CreateParams(browser_context)));
  web_contents_->SetDelegate(this);
}
ViewImpl::~ViewImpl() {}

void ViewImpl::LoadUrl(const GURL& url) {
  web_contents_->GetController().LoadURL(
      url, content::Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
}

void ViewImpl::LoadingStateChanged(content::WebContents* source,
                                   bool to_different_document) {
  client_->LoadingStateChanged(source->IsLoading());
}

}  // navigation
