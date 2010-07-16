/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_NPAPI_ASYNC_RECEIVE_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_NPAPI_ASYNC_RECEIVE_H_

#include "native_client/src/trusted/plugin/npapi/plugin_npapi.h"

namespace plugin {

struct ReceiveThreadArgs {
  NPP plugin;
  NPObject* callback;
  nacl::DescWrapper* socket;
};

void WINAPI AsyncReceiveThread(void* handle);

}

#endif
