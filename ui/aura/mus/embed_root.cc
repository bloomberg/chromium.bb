// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/mus/embed_root.h"

#include "base/auto_reset.h"
#include "base/bind.h"
#include "ui/aura/mus/embed_root_delegate.h"
#include "ui/aura/mus/window_tree_client.h"
#include "ui/aura/window_tree_host.h"

namespace aura {

EmbedRoot::~EmbedRoot() {
  window_tree_client_->OnEmbedRootDestroyed(this);
}

aura::Window* EmbedRoot::window() {
  return window_tree_host_ ? window_tree_host_->window() : nullptr;
}

EmbedRoot::EmbedRoot(WindowTreeClient* window_tree_client,
                     EmbedRootDelegate* delegate,
                     ui::ClientSpecificId window_id)
    : window_tree_client_(window_tree_client),
      delegate_(delegate),
      weak_factory_(this) {
  window_tree_client_->tree_->ScheduleEmbedForExistingClient(
      window_id, base::BindOnce(&EmbedRoot::OnScheduledEmbedForExistingClient,
                                weak_factory_.GetWeakPtr()));
}

void EmbedRoot::OnScheduledEmbedForExistingClient(
    const base::UnguessableToken& token) {
  token_ = token;
  delegate_->OnEmbedTokenAvailable(token);
}

void EmbedRoot::OnEmbed(std::unique_ptr<WindowTreeHost> window_tree_host) {
  window_tree_host_ = std::move(window_tree_host);
  delegate_->OnEmbed(window());
}

void EmbedRoot::OnUnembed() {
  delegate_->OnUnembed();
}

}  // namespace aura
