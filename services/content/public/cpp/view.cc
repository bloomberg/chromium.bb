// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/content/public/cpp/view.h"

#include "services/content/public/cpp/buildflags.h"

#if BUILDFLAG(ENABLE_AURA_CONTENT_VIEW_EMBEDDING)
#include "services/ui/public/interfaces/window_tree_constants.mojom.h"  // nogncheck
#include "ui/views/layout/fill_layout.h"                // nogncheck
#include "ui/views/mus/remote_view/remote_view_host.h"  // nogncheck
#include "ui/views/view.h"                              // nogncheck
#endif

namespace content {

View::View(mojom::ViewFactory* factory) : client_binding_(this) {
  mojom::ViewClientPtr client;
  client_binding_.Bind(mojo::MakeRequest(&client));
  factory->CreateView(mojom::ViewParams::New(), mojo::MakeRequest(&view_),
                      std::move(client));
}

View::~View() = default;

views::View* View::CreateUI() {
  view_->PrepareToEmbed(
      base::BindOnce(&View::OnEmbedTokenReceived, base::Unretained(this)));
#if BUILDFLAG(ENABLE_AURA_CONTENT_VIEW_EMBEDDING)
  DCHECK(!ui_view_);
  ui_view_ = std::make_unique<views::View>();
  ui_view_->set_owned_by_client();
  ui_view_->SetLayoutManager(std::make_unique<views::FillLayout>());

  DCHECK(!remote_view_host_);
  remote_view_host_ = new views::RemoteViewHost;
  ui_view_->AddChildView(remote_view_host_);
  return ui_view_.get();
#else
  return nullptr;
#endif
}

void View::Navigate(const GURL& url) {
  view_->Navigate(url);
}

void View::DidStopLoading() {
  if (did_stop_loading_callback_)
    did_stop_loading_callback_.Run();
}

void View::OnEmbedTokenReceived(const base::UnguessableToken& token) {
#if BUILDFLAG(ENABLE_AURA_CONTENT_VIEW_EMBEDDING)
  const uint32_t kEmbedFlags = ui::mojom::kEmbedFlagEmbedderInterceptsEvents |
                               ui::mojom::kEmbedFlagEmbedderControlsVisibility;
  remote_view_host_->EmbedUsingToken(token, kEmbedFlags, base::DoNothing());
#endif  // defined(USE_AURA)
}

}  // namespace content
