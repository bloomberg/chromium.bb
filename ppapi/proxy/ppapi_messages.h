// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPAPI_MESSAGES_H_
#define PPAPI_PROXY_PPAPI_MESSAGES_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/string16.h"
#include "ipc/ipc_message_utils.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/proxy/ppapi_param_traits.h"
#include "ppapi/proxy/serialized_structs.h"

#define MESSAGES_INTERNAL_FILE "ppapi/proxy/ppapi_messages_internal.h"
#include "ipc/ipc_message_macros.h"

#endif  // PPAPI_PROXY_PPAPI_MESSAGES_H_
