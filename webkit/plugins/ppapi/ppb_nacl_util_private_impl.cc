// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_nacl_util_private_impl.h"

#include "ppapi/c/private/ppb_nacl_util_private.h"

namespace webkit {
namespace ppapi {

namespace {

int32_t LaunchSelLdr(PP_Resource file_io) {
  return 0;
}

}  // namespace

const PPB_NaClUtil_Private ppb_nacl_util = {
  &LaunchSelLdr,
};

// static
const PPB_NaClUtil_Private* PPB_NaClUtil_Private_Impl::GetInterface() {
  return &ppb_nacl_util;
}

}  // namespace ppapi
}  // namespace webkit
