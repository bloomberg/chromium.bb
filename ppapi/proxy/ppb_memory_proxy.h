// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PPB_MEMORY_PROXY_H_
#define PPAPI_PPB_MEMORY_PROXY_H_

#include "ppapi/proxy/interface_proxy.h"

struct PPB_Memory_Dev;

namespace ppapi {
namespace proxy {

const PPB_Memory_Dev* GetPPB_Memory_Interface();

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PPB_MEMORY_PROXY_H_
