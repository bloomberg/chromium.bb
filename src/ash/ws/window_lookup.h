// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WS_WINDOW_LOOKUP_H_
#define ASH_WS_WINDOW_LOOKUP_H_

#include "ash/ash_export.h"
#include "services/ws/common/types.h"

namespace aura {
class Window;
}

// TODO: this file is only necessary for single-process mash and should be
// removed.
namespace ash {
namespace window_lookup {

// Returns the aura::Window by transport id.
ASH_EXPORT aura::Window* GetWindowByClientId(ws::Id transport_id);

}  // namespace window_lookup
}  // namespace ash

#endif  // ASH_WS_WINDOW_LOOKUP_H_
