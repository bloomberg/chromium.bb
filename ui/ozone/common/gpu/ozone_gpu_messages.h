// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message file, hence no include guard here, but see below
// for a much smaller-than-usual include guard section.

#include <vector>

#include "ipc/ipc_message_macros.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/ipc/gfx_param_traits.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/common/gpu/ozone_gpu_message_params.h"
#include "ui/ozone/ozone_export.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT OZONE_EXPORT

#define IPC_MESSAGE_START OzoneGpuMsgStart

IPC_ENUM_TRAITS_MAX_VALUE(ui::DisplayConnectionType,
                          ui::DISPLAY_CONNECTION_TYPE_LAST)

IPC_STRUCT_TRAITS_BEGIN(ui::DisplayMode_Params)
  IPC_STRUCT_TRAITS_MEMBER(size)
  IPC_STRUCT_TRAITS_MEMBER(is_interlaced)
  IPC_STRUCT_TRAITS_MEMBER(refresh_rate)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(ui::DisplaySnapshot_Params)
  IPC_STRUCT_TRAITS_MEMBER(display_id)
  IPC_STRUCT_TRAITS_MEMBER(has_proper_display_id)
  IPC_STRUCT_TRAITS_MEMBER(origin)
  IPC_STRUCT_TRAITS_MEMBER(physical_size)
  IPC_STRUCT_TRAITS_MEMBER(type)
  IPC_STRUCT_TRAITS_MEMBER(is_aspect_preserving_scaling)
  IPC_STRUCT_TRAITS_MEMBER(has_overscan)
  IPC_STRUCT_TRAITS_MEMBER(display_name)
  IPC_STRUCT_TRAITS_MEMBER(modes)
  IPC_STRUCT_TRAITS_MEMBER(has_current_mode)
  IPC_STRUCT_TRAITS_MEMBER(current_mode)
  IPC_STRUCT_TRAITS_MEMBER(has_native_mode)
  IPC_STRUCT_TRAITS_MEMBER(native_mode)
  IPC_STRUCT_TRAITS_MEMBER(string_representation)
IPC_STRUCT_TRAITS_END()

//------------------------------------------------------------------------------
// GPU Messages
// These are messages from the browser to the GPU process.

// Update the HW cursor bitmap & move to specified location.
IPC_MESSAGE_CONTROL3(OzoneGpuMsg_CursorSet,
                     gfx::AcceleratedWidget, SkBitmap, gfx::Point)

// Move the HW cursor to the specified location.
IPC_MESSAGE_CONTROL2(OzoneGpuMsg_CursorMove,
                     gfx::AcceleratedWidget, gfx::Point)

#if defined(OS_CHROMEOS)
// Force the DPMS state of the display to on.
IPC_MESSAGE_CONTROL0(OzoneGpuMsg_ForceDPMSOn)

// Trigger a display reconfiguration. OzoneHostMsg_UpdateNativeDisplays will be
// sent as a response.
IPC_MESSAGE_CONTROL0(OzoneGpuMsg_RefreshNativeDisplays)

// Configure a display with the specified mode at the specified location.
IPC_MESSAGE_CONTROL3(OzoneGpuMsg_ConfigureNativeDisplay,
                     int64_t,  // display ID
                     ui::DisplayMode_Params,  // display mode
                     gfx::Point)  // origin for the display

IPC_MESSAGE_CONTROL1(OzoneGpuMsg_DisableNativeDisplay,
                     int64_t)  // display ID

//------------------------------------------------------------------------------
// Browser Messages
// These messages are from the GPU to the browser process.

// Updates the list of active displays.
IPC_MESSAGE_CONTROL1(OzoneHostMsg_UpdateNativeDisplays,
                     std::vector<ui::DisplaySnapshot_Params>)
#endif
