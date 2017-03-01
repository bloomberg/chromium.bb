// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/demo/mus_demo_external.h"

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
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
    // TODO(tonikitoo,fwang): Extend the API to allow creating WindowTreeHost
    // using the WindowTreeClient.
    factory->CreateWindowTreeHost(MakeRequest(&host_), std::move(tree_client));
  }

 private:
  // Holds the Mojo pointer to the window tree host.
  mojom::WindowTreeHostPtr host_;

  DISALLOW_COPY_AND_ASSIGN(WindowTreeDataExternal);
};

int GetSquareSizeForWindow(int window_index) {
  return 50 * window_index + 400;
};

}  // namespace

MusDemoExternal::MusDemoExternal() {}

MusDemoExternal::~MusDemoExternal() {}

std::unique_ptr<aura::WindowTreeClient>
MusDemoExternal::CreateWindowTreeClient() {
  return base::MakeUnique<aura::WindowTreeClient>(
      context()->connector(), this, nullptr,
      MakeRequest(&window_tree_client_mojo_));
}

void MusDemoExternal::OnStartImpl() {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch("external-window-count")) {
    if (!base::StringToSizeT(
            command_line->GetSwitchValueASCII("external-window-count"),
            &number_of_windows_)) {
      LOG(FATAL) << "Invalid value for \'external-window-count\'";
      return;
    }
  }

  // TODO(tonikitoo,fwang): Extend the WindowTreeClient API to allow connection
  // to the window tree host factory using window_tree_client().
  context()->connector()->BindInterface(ui::mojom::kServiceName,
                                        &window_tree_host_factory_);

  // TODO(tonikitoo,fwang): Implement management of displays in external mode.
  // For now, a fake display is created in order to work around an assertion in
  // aura::GetDeviceScaleFactorFromDisplay().
  AddPrimaryDisplay(display::Display(0));

  // The number of windows to open is specified by number_of_windows_. The
  // windows are opened sequentially (the first one here and the others after
  // each call to OnEmbed) to ensure that the WindowTreeHostMus passed to
  // OnEmbed corresponds to the WindowTreeDataExternal::host_ created in
  // OpenNewWindow.
  OpenNewWindow();
}

void MusDemoExternal::OpenNewWindow() {
  // TODO(tonikitoo,fwang): Extend the WindowTreeClient API to allow creation
  // of window tree host. Then pass window_tree_client() here and remove
  // window_tree_host_factory_ and window_tree_client_mojo_. Currently
  // window_tree_client_mojo_ is only initialized once so this is incorrect when
  // kNumberOfWindows > 1.
  AppendWindowTreeData(base::MakeUnique<WindowTreeDataExternal>(
      window_tree_host_factory_.get(), std::move(window_tree_client_mojo_),
      GetSquareSizeForWindow(initialized_windows_count_)));
}

void MusDemoExternal::OnEmbed(
    std::unique_ptr<aura::WindowTreeHostMus> window_tree_host) {
  InitWindowTreeData(std::move(window_tree_host));
  initialized_windows_count_++;

  // Open the next window until the requested number of windows is reached.
  if (initialized_windows_count_ < number_of_windows_)
    OpenNewWindow();
}

void MusDemoExternal::OnEmbedRootDestroyed(
    aura::WindowTreeHostMus* window_tree_host) {
  RemoveWindowTreeData(window_tree_host);
}

}  // namespace demo
}  // namespace ui
