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

#include "native_client/src/shared/imc/nacl_imc.h"
#include "ppapi/c/pp_instance.h"


typedef bool (*LaunchNaClProcessFunc)(PP_Instance instance,
                                      const char* url,
                                      int socket_count,
                                      nacl::Handle* result_sockets);


extern LaunchNaClProcessFunc launch_nacl_process;

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_NACL_ENTRY_POINTS_H_
