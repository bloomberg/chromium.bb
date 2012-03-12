// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines IPC messages used by Chromoting components.

// Multiply-included message file, no traditional include guard.
#include "ipc/ipc_message_macros.h"

#define IPC_MESSAGE_START ChromotingMsgStart

//-----------------------------------------------------------------------------
// The Chrmomoting session messages

// Asks the service to send the Secure Attention Sequence (SAS) to the current
// console session.
IPC_MESSAGE_CONTROL0(ChromotingHostMsg_SendSasToConsole)
