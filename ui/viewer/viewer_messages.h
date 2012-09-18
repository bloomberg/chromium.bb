// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message file, no traditional include guard.
#include <string>

#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_message_macros.h"

#define IPC_MESSAGE_START ViewerMsgStart

//-----------------------------------------------------------------------------
// Viewer process messages:
// These are messages from the browser to the viewer process.

// Shares the rendering handle to the viewer process.
IPC_MESSAGE_CONTROL1(ViewerMsg_PbufferHandle,
                     std::string /* TODO(scottmg): whatever type */)

//-----------------------------------------------------------------------------
// Viewer process host messages:
// These are messages from the viewer process to the browser.

// Connect to the browser (request a pbuffer handle).
IPC_MESSAGE_CONTROL0(ViewerHostMsg_Connect)
