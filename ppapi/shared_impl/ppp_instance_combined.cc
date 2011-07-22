// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/ppp_instance_combined.h"

namespace ppapi {

PPP_Instance_Combined::PPP_Instance_Combined(
    const PPP_Instance_1_0& instance_if)
    : PPP_Instance_1_0(instance_if) {
}

}  // namespace ppapi

