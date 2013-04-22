// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/ppapi_nacl_channel_args.h"

namespace ppapi {

// We must provide explicit definitions of these functions for builds on
// Windows.
PpapiNaClChannelArgs::PpapiNaClChannelArgs() {
}

PpapiNaClChannelArgs::~PpapiNaClChannelArgs() {
}

}  // namespace ppapi
