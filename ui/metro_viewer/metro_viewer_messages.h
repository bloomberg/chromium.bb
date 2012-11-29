// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message file, no include guard.

#include "base/basictypes.h"
#include "ipc/ipc_message_macros.h"
#include "ui/base/events/event_constants.h"
#include "ui/gfx/native_widget_types.h"

#define IPC_MESSAGE_START MetroViewerMsgStart

IPC_ENUM_TRAITS(ui::EventType)
IPC_ENUM_TRAITS(ui::EventFlags)

// Messages sent from the viewer to the browser.

// Inform the browser of the surface to target for compositing.
IPC_MESSAGE_CONTROL1(MetroViewerHostMsg_SetTargetSurface,
                     gfx::NativeViewId /* target hwnd */)
// Informs the browser that the mouse moved.
IPC_MESSAGE_CONTROL3(MetroViewerHostMsg_MouseMoved,
                     int32,       /* x-coordinate */
                     int32,       /* y-coordinate */
                     int32        /* flags */)
// Informs the brower that a mouse button was pressed.
IPC_MESSAGE_CONTROL5(MetroViewerHostMsg_MouseButton,
                     int32,           /* x-coordinate */
                     int32,           /* y-coordinate */
                     int32,           /* extra */
                     ui::EventType,   /* event type */
                     ui::EventFlags   /* event flags */)
// Informs the browser that a key was pressed.
IPC_MESSAGE_CONTROL4(MetroViewerHostMsg_KeyDown,
                     uint32,       /* virtual key */
                     uint32,       /* repeat count */
                     uint32,       /* scan code */
                     uint32        /* key state */);
// Informs the browser that a key was released.
IPC_MESSAGE_CONTROL4(MetroViewerHostMsg_KeyUp,
                     uint32,       /* virtual key */
                     uint32,       /* repeat count */
                     uint32,       /* scan code */
                     uint32        /* key state */);
IPC_MESSAGE_CONTROL4(MetroViewerHostMsg_Character,
                     uint32,       /* virtual key */
                     uint32,       /* repeat count */
                     uint32,       /* scan code */
                     uint32        /* key state */);
// Informs the browser that the visibiliy of the viewer has changed.
IPC_MESSAGE_CONTROL1(MetroViewerHostMsg_VisibilityChanged,
                     bool          /* visible */);
