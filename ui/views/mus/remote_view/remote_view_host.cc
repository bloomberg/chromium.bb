// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/mus/remote_view/remote_view_host.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/aura/mus/window_port_mus.h"

namespace views {

RemoteViewHost::RemoteViewHost() = default;
RemoteViewHost::~RemoteViewHost() = default;

void RemoteViewHost::EmbedUsingToken(const base::UnguessableToken& embed_token,
                                     int embed_flags,
                                     EmbedCallback callback) {
  // Only works with mus.
  DCHECK_EQ(aura::Env::Mode::MUS, aura::Env::GetInstanceDontCreate()->mode());

  // Should not be attached to anything.
  DCHECK(!native_view());

  embedding_root_ = std::make_unique<aura::Window>(nullptr);
  embedding_root_->set_owned_by_parent(false);

  embedding_root_->SetName("RemoteViewHostWindow");
  embedding_root_->SetProperty(aura::client::kEmbedType,
                               aura::client::WindowEmbedType::EMBED_IN_OWNER);
  embedding_root_->SetType(aura::client::WINDOW_TYPE_CONTROL);
  embedding_root_->Init(ui::LAYER_NOT_DRAWN);

  aura::WindowPortMus::Get(embedding_root_.get())
      ->EmbedUsingToken(embed_token, embed_flags,
                        base::BindOnce(&RemoteViewHost::OnEmbedResult,
                                       weak_ptr_factory_.GetWeakPtr(),
                                       embed_token, std::move(callback)));
}

void RemoteViewHost::OnEmbedResult(const base::UnguessableToken& token,
                                   EmbedCallback callback,
                                   bool success) {
  LOG_IF(ERROR, !success) << "Failed to embed, token=" << token;

  // Attach to |embedding_root_| if embed succeeds and this view is added to a
  // widget but has not yet attached to |embedding_root_|.
  if (success && GetWidget() && !native_view())
    Attach(embedding_root_.get());

  if (!success && embedding_root_)
    embedding_root_.reset();

  if (callback)
    std::move(callback).Run(success);
}

void RemoteViewHost::AddedToWidget() {
  if (embedding_root_ && !native_view())
    Attach(embedding_root_.get());
}

}  // namespace views
