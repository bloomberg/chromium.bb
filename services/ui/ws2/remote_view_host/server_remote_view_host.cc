// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws2/remote_view_host/server_remote_view_host.h"

#include <utility>

#include "base/logging.h"
#include "services/ui/ws2/window_service.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"

namespace ui {
namespace ws2 {

ServerRemoteViewHost::ServerRemoteViewHost(WindowService* window_service)
    : window_service_(window_service) {}

ServerRemoteViewHost::~ServerRemoteViewHost() = default;

void ServerRemoteViewHost::EmbedUsingToken(
    const base::UnguessableToken& embed_token,
    int embed_flags,
    EmbedCallback callback) {
  embed_token_ = embed_token;
  embed_flags_ = embed_flags;
  embed_callback_ = std::move(callback);

  // TODO(sky): having to wait for being parented is a bit of a hassle. Fix
  // this.
  if (GetWidget())
    EmbedImpl();
}

void ServerRemoteViewHost::EmbedImpl() {
  // Should not be attached to anything.
  DCHECK(!native_view());

  // There is a pending embed request.
  DCHECK(!embed_token_.is_empty());

  embedding_root_ = std::make_unique<aura::Window>(
      nullptr, aura::client::WINDOW_TYPE_UNKNOWN, window_service_->env());
  embedding_root_->set_owned_by_parent(false);
  embedding_root_->SetName("ServerRemoteViewHostWindow");
  embedding_root_->SetType(aura::client::WINDOW_TYPE_CONTROL);
  embedding_root_->Init(ui::LAYER_NOT_DRAWN);

  // TODO(sky): fix this, only necessary because of restrictions in WindowTree.
  // Must happen before EmbedUsingToken call for window server to figure out
  // the relevant display.
  Attach(embedding_root_.get());

  const bool result = window_service_->CompleteScheduleEmbedForExistingClient(
      embedding_root_.get(), embed_token_, embed_flags_);

  if (!result)
    embedding_root_.reset();

  std::move(embed_callback_).Run(result);
}

void ServerRemoteViewHost::AddedToWidget() {
  if (!native_view() && !embedding_root_)
    EmbedImpl();
}

}  // namespace ws2
}  // namespace ui
