// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_crypto_impl.h"

#include "ppapi/c/dev/ppb_crypto_dev.h"
#include "ppapi/shared_impl/crypto_impl.h"

namespace webkit {
namespace ppapi {

namespace {

const PPB_Crypto_Dev ppb_crypto = {
  &::ppapi::CryptoImpl::GetRandomBytes
};

}  // namespace

// static
const PPB_Crypto_Dev* PPB_Crypto_Impl::GetInterface() {
  return &ppb_crypto;
}

}  // namespace ppapi
}  // namespace webkit

