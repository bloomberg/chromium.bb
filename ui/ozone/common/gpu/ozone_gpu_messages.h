// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message file, hence no include guard here, but see below
// for a much smaller-than-usual include guard section.

#include "ipc/ipc_message_macros.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/ipc/gfx_param_traits.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/ozone_export.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT OZONE_EXPORT

#define IPC_MESSAGE_START OzoneGpuMsgStart

//------------------------------------------------------------------------------
// GPU Messages
// These are messages from the browser to the GPU process.

// Update the HW cursor bitmap & move to specified location.
IPC_MESSAGE_CONTROL3(OzoneGpuMsg_CursorSet,
                     gfx::AcceleratedWidget, SkBitmap, gfx::Point)

// Move the HW cursor to the specified location.
IPC_MESSAGE_CONTROL2(OzoneGpuMsg_CursorMove,
                     gfx::AcceleratedWidget, gfx::Point)

