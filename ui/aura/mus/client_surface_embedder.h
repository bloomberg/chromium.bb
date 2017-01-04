// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/macros.h"

namespace cc {
class SurfaceInfo;
}

namespace ui {
class Layer;
}

namespace aura {

class Window;

// Used by WindowPortMus when it is embedding a client. Responsible for setting
// up layers containing content from the client, parenting them to the window's
// layer, and updating them when the client submits new surfaces.
class ClientSurfaceEmbedder {
 public:
  explicit ClientSurfaceEmbedder(Window* window);
  ~ClientSurfaceEmbedder();

  // Updates the surface layer and the clip layer based on the surface info.
  void UpdateSurface(const cc::SurfaceInfo& surface_info);

 private:
  // The window which embeds the client.
  Window* window_;

  // Contains the client's content.
  std::unique_ptr<ui::Layer> surface_layer_;

  // Used for clipping the surface layer to the window bounds.
  std::unique_ptr<ui::Layer> clip_layer_;

  DISALLOW_COPY_AND_ASSIGN(ClientSurfaceEmbedder);
};

}  // namespace aura
