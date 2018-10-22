// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_IPC_COLOR_GFX_PARAM_TRAITS_MACROS_H_
#define UI_GFX_IPC_COLOR_GFX_PARAM_TRAITS_MACROS_H_

#include "ipc/ipc_message_utils.h"
#include "ipc/param_traits_macros.h"
#include "ipc/ipc_message_macros.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/icc_profile.h"
#include "ui/gfx/ipc/color/gfx_ipc_color_export.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT GFX_IPC_COLOR_EXPORT

IPC_ENUM_TRAITS_MAX_VALUE(gfx::ColorSpace::PrimaryID,
                          gfx::ColorSpace::PrimaryID::LAST);
IPC_ENUM_TRAITS_MAX_VALUE(gfx::ColorSpace::TransferID,
                          gfx::ColorSpace::TransferID::LAST);
IPC_ENUM_TRAITS_MAX_VALUE(gfx::ColorSpace::MatrixID,
                          gfx::ColorSpace::MatrixID::LAST);
IPC_ENUM_TRAITS_MAX_VALUE(gfx::ColorSpace::RangeID,
                          gfx::ColorSpace::RangeID::LAST);

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT

#endif  // UI_GFX_IPC_COLOR_GFX_PARAM_TRAITS_MACROS_H_
