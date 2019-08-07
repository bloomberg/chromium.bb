// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ws/window_lookup.h"

#include "ash/shell.h"
#include "ash/ws/window_service_owner.h"
#include "services/ws/common/util.h"
#include "services/ws/window_service.h"
#include "ui/aura/env.h"
#include "ui/aura/mus/window_port_mus.h"
#include "ui/aura/mus/window_tree_client.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/mus/mus_client.h"

namespace ash {
namespace window_lookup {
namespace {

ws::Id GetTransportId(aura::Window* window) {
  return Shell::Get()
      ->window_service_owner()
      ->window_service()
      ->GetCompleteTransportIdForWindow(window);
}

}  // namespace

bool IsProxyWindow(aura::Window* window) {
  return ws::WindowService::IsProxyWindow(window);
}

aura::Window* GetProxyWindowForClientWindow(aura::Window* window) {
  // This function only make sense in classic/single-process mash.
  DCHECK(!features::IsMultiProcessMash());
  if (!aura::WindowMus::Get(window))
    return nullptr;

  DCHECK(views::MusClient::Exists());
  aura::WindowTreeClient* window_tree_client =
      views::MusClient::Get()->window_tree_client();
  DCHECK(window_tree_client);

  aura::WindowPortMus* window_port_mus = aura::WindowPortMus::Get(window);
  if (!window_tree_client->id().has_value())
    return nullptr;  // The client doesn't know the id yet.

  const ws::ClientSpecificId client_id = *(window_tree_client->id());
  const ws::Id transport_id = ws::BuildTransportId(
      client_id,
      static_cast<ws::ClientSpecificId>(window_port_mus->server_id()));
  return Shell::Get()
      ->window_service_owner()
      ->window_service()
      ->GetWindowByClientId(transport_id);
}

aura::Window* GetClientWindowForProxyWindow(aura::Window* window) {
  // This function only make sense in classic/single-process mash.
  DCHECK(!features::IsMultiProcessMash());
  DCHECK(IsProxyWindow(window));
  DCHECK(views::MusClient::Get());

  const ws::Id window_id = GetTransportId(window);
  if (window_id == ws::kInvalidTransportId)
    return nullptr;

  aura::WindowTreeClient* window_tree_client =
      views::MusClient::Get()->window_tree_client();

  if (!window_tree_client->id().has_value())
    return nullptr;  // The client doesn't know its id yet.

  const ws::ClientSpecificId client_id = *(window_tree_client->id());
  if (ws::ClientIdFromTransportId(window_id) != client_id) {
    // |window| is a proxy window, the client is in another process.
    return nullptr;
  }

  // Clients use a client_id of 0 for all their windows.
  aura::WindowMus* window_mus = window_tree_client->GetWindowByServerId(
      ws::BuildTransportId(0, ws::ClientWindowIdFromTransportId(window_id)));
  return window_mus ? window_mus->GetWindow() : nullptr;
}

bool IsProxyWindowForOutOfProcess(aura::Window* window) {
  if (!IsProxyWindow(window))
    return false;

  const ws::Id window_id = GetTransportId(window);
  if (window_id == ws::kInvalidTransportId)
    return false;

  aura::WindowTreeClient* window_tree_client =
      views::MusClient::Get()->window_tree_client();

  if (!window_tree_client->id().has_value())
    return false;  // The client doesn't know its id yet.

  const ws::ClientSpecificId client_id = *(window_tree_client->id());
  return ws::ClientIdFromTransportId(window_id) != client_id;
}

}  // namespace window_lookup
}  // namespace ash
