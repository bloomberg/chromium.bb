// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/private/net_address_private_impl.h"

#include "build/build_config.h"
#include "ppapi/c/private/ppb_net_address_private.h"

namespace ppapi {

#if !defined(OS_NACL)
// static
const PP_NetAddress_Private NetAddressPrivateImpl::kInvalidNetAddress = { 0 };
#endif  // !defined(OS_NACL)

}  // namespace ppapi
