// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/content/public/cpp/navigable_contents.h"

#include "base/memory/ptr_util.h"
#include "services/content/public/cpp/navigable_contents_view.h"

namespace content {

NavigableContents::NavigableContents(mojom::NavigableContentsFactory* factory)
    : client_binding_(this) {
  mojom::NavigableContentsClientPtr client;
  client_binding_.Bind(mojo::MakeRequest(&client));
  factory->CreateContents(mojom::NavigableContentsParams::New(),
                          mojo::MakeRequest(&contents_), std::move(client));
}

NavigableContents::~NavigableContents() = default;

NavigableContentsView* NavigableContents::GetView() {
  if (!view_) {
    view_ = base::WrapUnique(new NavigableContentsView);
    contents_->CreateView(
        NavigableContentsView::IsClientRunningInServiceProcess(),
        base::BindOnce(&NavigableContents::OnEmbedTokenReceived,
                       base::Unretained(this)));
  }
  return view_.get();
}

void NavigableContents::Navigate(const GURL& url) {
  contents_->Navigate(url);
}

void NavigableContents::DidStopLoading() {
  if (did_stop_loading_callback_)
    did_stop_loading_callback_.Run();
}

void NavigableContents::OnEmbedTokenReceived(
    const base::UnguessableToken& token) {
  DCHECK(view_);
  view_->EmbedUsingToken(token);
}

}  // namespace content
