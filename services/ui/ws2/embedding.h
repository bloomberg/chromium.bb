// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS2_EMBEDDING_H_
#define SERVICES_UI_WS2_EMBEDDING_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "services/ui/public/interfaces/window_tree.mojom.h"
#include "services/ui/ws2/window_tree_binding.h"

namespace aura {
class Window;
}

namespace ui {
namespace ws2 {

class WindowService;
class WindowTree;

// Embedding is created any time a client calls Embed() (Embedding is not
// created for top-levels). Embedding owns the embedded WindowTree (by way of
// owning WindowTreeBinding). Embedding is owned by the Window associated with
// the embedding.
class COMPONENT_EXPORT(WINDOW_SERVICE) Embedding {
 public:
  Embedding(WindowTree* embedding_tree,
            aura::Window* window,
            bool embedding_tree_intercepts_events);
  ~Embedding();

  void Init(WindowService* window_service,
            mojom::WindowTreeClientPtr window_tree_client_ptr,
            mojom::WindowTreeClient* window_tree_client,
            base::OnceClosure connection_lost_callback);

  WindowTree* embedding_tree() { return embedding_tree_; }

  bool embedding_tree_intercepts_events() const {
    return embedding_tree_intercepts_events_;
  }

  WindowTree* embedded_tree() { return binding_.window_tree(); }

  aura::Window* window() { return window_; }

 private:
  // The client that initiated the embedding.
  WindowTree* embedding_tree_;

  // The window the embedding is in.
  aura::Window* window_;

  // If true, all events that would normally target the embedded tree are
  // instead sent to the tree that created the embedding. For example, consider
  // the Window hierarchy A1->B1->C2 where tree 1 created A1 and B1, tree 1
  // embedded tree 2 in window B1, and tree 2 created C2. If an event occurs
  // that would normally target C2, then the event is instead sent to tree 1.
  // Embedded trees can always observe pointer events, regardless of this value.
  const bool embedding_tree_intercepts_events_;

  WindowTreeBinding binding_;

  DISALLOW_COPY_AND_ASSIGN(Embedding);
};

}  // namespace ws2
}  // namespace ui

#endif  // SERVICES_UI_WS2_EMBEDDING_H_
