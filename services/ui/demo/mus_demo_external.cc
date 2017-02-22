// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/demo/mus_demo_external.h"

#include "services/service_manager/public/cpp/service_context.h"
#include "services/ui/demo/window_tree_data.h"
#include "services/ui/public/interfaces/constants.mojom.h"
#include "services/ui/public/interfaces/window_tree_host.mojom.h"
#include "ui/aura/mus/window_tree_client.h"
#include "ui/aura/mus/window_tree_host_mus.h"
#include "ui/display/display.h"

namespace ui {
namespace demo {

namespace {

class WindowTreeDataExternal : public WindowTreeData {
 public:
  // Creates a new window tree host associated to the WindowTreeData.
  WindowTreeDataExternal(mojom::WindowTreeHostFactory* factory,
                         mojom::WindowTreeClientPtr tree_client,
                         int square_size)
      : WindowTreeData(square_size) {
    factory->CreateWindowTreeHost(MakeRequest(&host_), std::move(tree_client));
  }

 private:
  // Holds the Mojo pointer to the window tree host.
  mojom::WindowTreeHostPtr host_;
};

// Size of square in pixels to draw.
const int kSquareSize = 500;
}  // namespace

MusDemoExternal::MusDemoExternal() {}

MusDemoExternal::~MusDemoExternal() {}

void MusDemoExternal::OnStartImpl(
    std::unique_ptr<aura::WindowTreeClient>* window_tree_client,
    std::unique_ptr<WindowTreeData>* window_tree_data) {
  context()->connector()->BindInterface(ui::mojom::kServiceName,
                                        &window_tree_host_factory_);
  mojom::WindowTreeClientPtr tree_client;
  *window_tree_client = base::MakeUnique<aura::WindowTreeClient>(
      context()->connector(), this, nullptr, MakeRequest(&tree_client));
  // TODO(tonikitoo,fwang): Open two external windows with different square
  // sizes.
  *window_tree_data = base::MakeUnique<WindowTreeDataExternal>(
      window_tree_host_factory_.get(), std::move(tree_client), kSquareSize);

  // TODO(tonikitoo,fwang): Implement management of displays in external mode.
  // For now, a fake display is created in order to work around an assertion in
  // aura::GetDeviceScaleFactorFromDisplay().
  AddPrimaryDisplay(display::Display(0));
}

void MusDemoExternal::OnEmbed(
    std::unique_ptr<aura::WindowTreeHostMus> window_tree_host) {
  InitWindowTreeData(std::move(window_tree_host));
}

void MusDemoExternal::OnEmbedRootDestroyed(
    aura::WindowTreeHostMus* window_tree_host) {
  CleanupWindowTreeData();
}

}  // namespace demo
}  // namespace ui
