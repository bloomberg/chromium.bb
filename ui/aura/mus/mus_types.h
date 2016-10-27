// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_MUS_MUS_TYPES_H_
#define UI_AURA_MUS_MUS_TYPES_H_

#include <stdint.h>

// Typedefs for the transport types. These typedefs match that of the mojom
// file, see it for specifics.

namespace aura {

// Used to identify windows and change ids.
using Id = uint32_t;

// Used to identify a client as well as a client-specific window id. For
// example, the Id for a window consists of the ClientSpecificId of the client
// and the ClientSpecificId of the window.
using ClientSpecificId = uint16_t;

constexpr Id kInvalidServerId = 0;

}  // namespace ui

#endif  // UI_AURA_MUS_MUS_TYPES_H_
