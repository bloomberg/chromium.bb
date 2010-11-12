// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppapi_messages.h"

#include "base/file_path.h"
#include "ipc/ipc_channel_handle.h"
#include "ppapi/c/dev/pp_file_info_dev.h"
#include "ppapi/c/ppb_var.h"

// This actually defines the implementations of all the IPC message functions.
#define MESSAGES_INTERNAL_IMPL_FILE "ppapi/proxy/ppapi_messages_internal.h"
#include "ipc/ipc_message_impl_macros.h"

