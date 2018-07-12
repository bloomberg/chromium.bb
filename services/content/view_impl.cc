// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/content/view_impl.h"

#include "base/bind.h"
#include "services/content/public/cpp/buildflags.h"
#include "services/content/service.h"
#include "services/content/service_delegate.h"
#include "services/content/view_delegate.h"

#if BUILDFLAG(ENABLE_AURA_CONTENT_VIEW_EMBEDDING)
#include "ui/aura/window.h"                                 // nogncheck
#include "ui/views/mus/remote_view/remote_view_provider.h"  // nogncheck
#endif

namespace content {

ViewImpl::ViewImpl(Service* service,
                   mojom::ViewParamsPtr params,
                   mojom::ViewRequest request,
                   mojom::ViewClientPtr client)
    : service_(service),
      binding_(this, std::move(request)),
      client_(std::move(client)),
      delegate_(service_->delegate()->CreateViewDelegate(client_.get())),
      native_content_view_(delegate_->GetNativeView()) {
  binding_.set_connection_error_handler(base::BindRepeating(
      &Service::RemoveView, base::Unretained(service_), this));

#if BUILDFLAG(ENABLE_AURA_CONTENT_VIEW_EMBEDDING)
  if (native_content_view_) {
    DCHECK(!remote_view_provider_);
    remote_view_provider_ =
        std::make_unique<views::RemoteViewProvider>(native_content_view_);
  }
#endif
}

ViewImpl::~ViewImpl() = default;

void ViewImpl::PrepareToEmbed(PrepareToEmbedCallback callback) {
#if BUILDFLAG(ENABLE_AURA_CONTENT_VIEW_EMBEDDING)
  if (remote_view_provider_) {
    remote_view_provider_->GetEmbedToken(
        base::BindOnce(&ViewImpl::OnEmbedTokenReceived, base::Unretained(this),
                       std::move(callback)));
    return;
  }
#endif

  DLOG(ERROR) << "View embedding not yet supported on this platform.";
  std::move(callback).Run(base::UnguessableToken::Create());
}

void ViewImpl::Navigate(const GURL& url) {
  // Ignore non-HTTP/HTTPS requests for now.
  if (!url.SchemeIsHTTPOrHTTPS())
    return;

  delegate_->Navigate(url);
}

void ViewImpl::OnEmbedTokenReceived(PrepareToEmbedCallback callback,
                                    const base::UnguessableToken& token) {
#if BUILDFLAG(ENABLE_AURA_CONTENT_VIEW_EMBEDDING)
  if (native_content_view_)
    native_content_view_->Show();
#endif
  std::move(callback).Run(token);
}

}  // namespace content
