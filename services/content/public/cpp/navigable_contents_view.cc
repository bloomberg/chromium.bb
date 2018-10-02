// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/content/public/cpp/navigable_contents_view.h"

#include <map>

#include "base/callback.h"
#include "base/no_destructor.h"
#include "base/synchronization/atomic_flag.h"
#include "base/unguessable_token.h"
#include "services/content/public/cpp/buildflags.h"

#if defined(TOOLKIT_VIEWS)
#include "ui/views/layout/fill_layout.h"  // nogncheck
#include "ui/views/view.h"                // nogncheck

#if BUILDFLAG(ENABLE_REMOTE_NAVIGABLE_CONTENTS_VIEW)
#include "services/ws/public/mojom/window_tree_constants.mojom.h"  // nogncheck
#include "ui/base/ui_base_features.h"                   // nogncheck
#include "ui/views/controls/native/native_view_host.h"  // nogncheck
#include "ui/views/mus/remote_view/remote_view_host.h"  // nogncheck
#endif  // BUILDFLAG(ENABLE_REMOTE_NAVIGABLE_CONTENTS_VIEW)
#endif  // defined(TOOLKIT_VIEWS)

namespace content {

namespace {

using InProcessEmbeddingMap =
    std::map<base::UnguessableToken,
             base::OnceCallback<void(NavigableContentsView*)>>;

InProcessEmbeddingMap& GetInProcessEmbeddingMap() {
  static base::NoDestructor<InProcessEmbeddingMap> embedding_map;
  return *embedding_map;
}

base::AtomicFlag& GetInServiceProcessFlag() {
  static base::NoDestructor<base::AtomicFlag> in_service_process;
  return *in_service_process;
}

#if BUILDFLAG(ENABLE_REMOTE_NAVIGABLE_CONTENTS_VIEW)

NavigableContentsView::RemoteViewFactory& GetRemoteViewFactory() {
  static base::NoDestructor<NavigableContentsView::RemoteViewFactory> factory;
  return *factory;
}

#endif  // BUILDFLAG(ENABLE_REMOTE_NAVIGABLE_CONTENTS_VIEW)

}  // namespace

NavigableContentsView::~NavigableContentsView() = default;

// static
void NavigableContentsView::SetClientRunningInServiceProcess() {
  GetInServiceProcessFlag().Set();
}

// static
bool NavigableContentsView::IsClientRunningInServiceProcess() {
  return GetInServiceProcessFlag().IsSet();
}

NavigableContentsView::NavigableContentsView() {
#if defined(TOOLKIT_VIEWS)
  view_ = std::make_unique<views::View>();
  view_->set_owned_by_client();
  view_->SetLayoutManager(std::make_unique<views::FillLayout>());
#endif  // defined(TOOLKIT_VIEWS)
}

#if BUILDFLAG(ENABLE_REMOTE_NAVIGABLE_CONTENTS_VIEW)

// static
void NavigableContentsView::SetRemoteViewFactory(RemoteViewFactory factory) {
  GetRemoteViewFactory() = std::move(factory);
}

#endif  // BUILDFLAG(ENABLE_REMOTE_NAVIGABLE_CONTENTS_VIEW)

void NavigableContentsView::EmbedUsingToken(
    const base::UnguessableToken& token) {
#if defined(TOOLKIT_VIEWS)
#if BUILDFLAG(ENABLE_REMOTE_NAVIGABLE_CONTENTS_VIEW)
  if (!IsClientRunningInServiceProcess()) {
    const RemoteViewFactory& factory = GetRemoteViewFactory();
    std::unique_ptr<views::NativeViewHost> remote_view_host;
    if (factory) {
      remote_view_host = factory.Run(token);
    } else {
      auto host = std::make_unique<views::RemoteViewHost>();
      constexpr uint32_t kEmbedFlags =
          ws::mojom::kEmbedFlagEmbedderInterceptsEvents |
          ws::mojom::kEmbedFlagEmbedderControlsVisibility;
      host->EmbedUsingToken(token, kEmbedFlags, base::DoNothing());
      remote_view_host = std::move(host);
    }

    auto* raw_remote_view_host = remote_view_host.release();
    view_->AddChildView(raw_remote_view_host);
    native_view_ = raw_remote_view_host->native_view();

    view_->Layout();
    return;
  }
#endif  // BUILDFLAG(ENABLE_REMOTE_NAVIGABLE_CONTENTS_VIEW)

  DCHECK(IsClientRunningInServiceProcess());

  // |token| should already have an embed callback entry in the in-process
  // callback map, injected by the in-process Content Service implementation.
  auto& embeddings = GetInProcessEmbeddingMap();
  auto it = embeddings.find(token);
  if (it == embeddings.end()) {
    DLOG(ERROR) << "Unable to embed with unknown token " << token.ToString();
    return;
  }

  // Invoke a callback provided by the Content Service's host environment. This
  // should parent a web content view to our own |view()|, as well as set
  // |native_view_| to the corresponding web contents' own NativeView.
  auto callback = std::move(it->second);
  embeddings.erase(it);
  std::move(callback).Run(this);
#endif  // defined(TOOLKIT_VIEWS)
}

// static
void NavigableContentsView::RegisterInProcessEmbedCallback(
    const base::UnguessableToken& token,
    base::OnceCallback<void(NavigableContentsView*)> callback) {
  GetInProcessEmbeddingMap()[token] = std::move(callback);
}

}  // namespace content
