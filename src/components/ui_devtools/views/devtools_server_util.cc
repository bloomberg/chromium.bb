// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_devtools/views/devtools_server_util.h"

#include <memory>

#include "base/command_line.h"
#include "components/ui_devtools/css_agent.h"
#include "components/ui_devtools/devtools_server.h"
#include "components/ui_devtools/switches.h"
#include "components/ui_devtools/views/dom_agent_aura.h"
#include "components/ui_devtools/views/overlay_agent_aura.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"

namespace ui_devtools {

std::unique_ptr<UiDevToolsServer> CreateUiDevToolsServerForViews(
    network::mojom::NetworkContext* network_context) {
  constexpr int kUiDevToolsDefaultPort = 9223;
  int port = UiDevToolsServer::GetUiDevToolsPort(switches::kEnableUiDevTools,
                                                 kUiDevToolsDefaultPort);
  auto server = UiDevToolsServer::CreateForViews(network_context, port);
  DCHECK(server);
  auto client =
      std::make_unique<UiDevToolsClient>("UiDevToolsClient", server.get());
  aura::Env* env = aura::Env::GetInstance();
  auto dom_agent_aura = std::make_unique<DOMAgentAura>(env);
  auto* dom_agent_aura_ptr = dom_agent_aura.get();
  client->AddAgent(std::move(dom_agent_aura));
  client->AddAgent(std::make_unique<CSSAgent>(dom_agent_aura_ptr));
  client->AddAgent(std::make_unique<OverlayAgentAura>(dom_agent_aura_ptr, env));
  server->AttachClient(std::move(client));
  return server;
}

void RegisterAdditionalRootWindowsAndEnv(std::vector<aura::Window*> roots) {
  DCHECK(!roots.empty());
  OverlayAgentAura::GetInstance()->RegisterEnv(roots[0]->env());
  DOMAgentAura::GetInstance()->RegisterEnv(roots[0]->env());
  for (auto* root : roots)
    DOMAgentAura::GetInstance()->RegisterRootWindow(root);
}

}  // namespace ui_devtools
