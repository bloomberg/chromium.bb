// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_MUS_CLIENT_SURFACE_EMBEDDER_H_
#define UI_AURA_MUS_CLIENT_SURFACE_EMBEDDER_H_

#include <memory>

#include "base/macros.h"
#include "ui/aura/aura_export.h"

namespace ui {
class LayerOwner;
}

namespace viz {
class SurfaceId;
}

namespace aura {

class Window;

// Used to display content from another client. The other client is rendering
// to viz using the SurfaceId supplied to SetSurfaceId().
class AURA_EXPORT ClientSurfaceEmbedder {
 public:
  explicit ClientSurfaceEmbedder(Window* window);
  ~ClientSurfaceEmbedder();

  // Sets the current SurfaceId *and* updates the size of the layer to match
  // that of the window.
  void SetSurfaceId(const viz::SurfaceId& surface_id);

  // Returns the SurfaceId, empty if SetSurfaceId() has not been called yet.
  viz::SurfaceId GetSurfaceId() const;

 private:
  // The window which embeds the client.
  Window* window_;

  // Contains the client's content. This (and other Layers) are wrapped in a
  // LayerOwner so that animations clone the layer.
  std::unique_ptr<ui::LayerOwner> surface_layer_owner_;

  DISALLOW_COPY_AND_ASSIGN(ClientSurfaceEmbedder);
};

}  // namespace aura

#endif  // UI_AURA_MUS_CLIENT_SURFACE_EMBEDDER_H_
