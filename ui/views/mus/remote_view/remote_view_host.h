// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_MUS_REMOTE_VIEW_REMOTE_VIEW_HOST_H_
#define UI_VIEWS_MUS_REMOTE_VIEW_REMOTE_VIEW_HOST_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
#include "ui/aura/window.h"
#include "ui/views/controls/native/native_view_host.h"

namespace views {

// A view at the embedder side to embed an aura::Window from another window
// tree. Note this only works with mus.
class RemoteViewHost : public views::NativeViewHost {
 public:
  RemoteViewHost();
  ~RemoteViewHost() override;

  // Creates an aura::window to embed the remote contents and attach to it when
  // the embed succeeds. |embed_token| is the token obtained from the WindowTree
  // embed API (ScheduleEmbed/ForExistingClient). |embed_flags| are the
  // embedding flags (see window_tree_constants.mojom). |callback| is an
  // optional callback invoked with the embed result.
  using EmbedCallback = base::OnceCallback<void(bool success)>;
  void EmbedUsingToken(const base::UnguessableToken& embed_token,
                       int embed_flags,
                       EmbedCallback callback);

 private:
  // Invoked after the embed operation.
  void OnEmbedResult(const base::UnguessableToken& token,
                     EmbedCallback callback,
                     bool success);

  // views::NativeViewHost:
  void AddedToWidget() override;

  std::unique_ptr<aura::Window> embedding_root_;
  base::WeakPtrFactory<RemoteViewHost> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(RemoteViewHost);
};

}  // namespace views

#endif  // UI_VIEWS_MUS_REMOTE_VIEW_REMOTE_VIEW_HOST_H_
