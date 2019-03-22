// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ws/window_lookup.h"

#include "ash/shell.h"
#include "ash/ws/window_service_owner.h"
#include "services/ws/window_service.h"

namespace ash {
namespace window_lookup {

aura::Window* GetWindowByClientId(ws::Id transport_id) {
  return Shell::Get()
      ->window_service_owner()
      ->window_service()
      ->GetWindowByClientId(transport_id);
}

}  // namespace window_lookup
}  // namespace ash
