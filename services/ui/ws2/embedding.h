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
#include "services/ui/ws2/window_service_client_binding.h"

namespace aura {
class Window;
}

namespace ui {
namespace ws2 {

class WindowService;
class WindowServiceClient;

// Embedding is created any time a client calls Embed() (Embedding is not
// created for top-levels). Embedding owns the embedded WindowServiceClient
// (by way of owning WindowServiceClientBinding). Embedding is owned by the
// Window associated with the embedding.
class COMPONENT_EXPORT(WINDOW_SERVICE) Embedding {
 public:
  Embedding(WindowServiceClient* embedding_client,
            aura::Window* window,
            bool embedding_client_intercepts_events);
  ~Embedding();

  void Init(WindowService* window_service,
            mojom::WindowTreeClientPtr window_tree_client_ptr,
            mojom::WindowTreeClient* window_tree_client,
            base::OnceClosure connection_lost_callback);

  WindowServiceClient* embedding_client() { return embedding_client_; }

  bool embedding_client_intercepts_events() const {
    return embedding_client_intercepts_events_;
  }

  WindowServiceClient* embedded_client() {
    return binding_.window_service_client();
  }

  aura::Window* window() { return window_; }

 private:
  // The client that initiated the embedding.
  WindowServiceClient* embedding_client_;

  // The window the embedding is in.
  aura::Window* window_;

  // If true, all events that would normally target the embedded client are
  // instead sent to the the client that created the embedding. For example,
  // consider the Window hierarchy A1->B1->C2 where client 1 created A1 and B1,
  // client 1 embedded client 2 in window B1, and client 2 created C2. If an
  // event occurs that would normally target C2, then the event is instead sent
  // to client 1. Embedded clients can always observe pointer events, regardless
  // of this value.
  const bool embedding_client_intercepts_events_;

  WindowServiceClientBinding binding_;

  DISALLOW_COPY_AND_ASSIGN(Embedding);
};

}  // namespace ws2
}  // namespace ui

#endif  // SERVICES_UI_WS2_EMBEDDING_H_
