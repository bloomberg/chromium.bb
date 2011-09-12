// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PPB_VAR_PROXY_H_
#define PPAPI_PPB_VAR_PROXY_H_

#include "ppapi/proxy/interface_proxy.h"

struct PPB_Var;

namespace ppapi {
namespace proxy {

// Returns a pointer to the plugin-specific implementation of PPB_Var.
// There is no proxy object since this is implemented entirely in-process.
const PPB_Var* GetPPB_Var_Interface();

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PPB_VAR_PROXY_H_
