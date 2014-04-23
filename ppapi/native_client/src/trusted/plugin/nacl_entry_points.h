/*
 * Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_NACL_ENTRY_POINTS_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_NACL_ENTRY_POINTS_H_

#include <stddef.h>

#include <map>
#include <string>

#include "native_client/src/shared/imc/nacl_imc_c.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/private/ppb_instance_private.h"
#include "ppapi/c/private/ppb_nacl_private.h"

typedef PP_ExternalPluginResult (*LaunchNaClProcessFunc)(
    PP_Instance instance,
    const char* alleged_url,
    PP_Bool uses_irt,
    PP_Bool uses_ppapi,
    PP_Bool uses_nonsfi_mode,
    PP_Bool enable_ppapi_dev,
    PP_Bool enable_dyncode_syscalls,
    PP_Bool enable_exception_handling,
    PP_Bool enable_crash_throttling,
    const PP_ManifestService* manifest_service_interface,
    void* manifest_service_user_data,
    NaClHandle* result_socket,
    struct PP_Var* error_message,
    PP_CompletionCallback callback);


extern LaunchNaClProcessFunc launch_nacl_process;

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_NACL_ENTRY_POINTS_H_
