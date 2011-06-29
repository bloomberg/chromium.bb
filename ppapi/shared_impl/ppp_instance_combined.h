// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_PPP_INSTANCE_COMBINED_H_
#define PPAPI_SHARED_IMPL_PPP_INSTANCE_COMBINED_H_

#include "base/basictypes.h"
#include "ppapi/c/ppp_instance.h"

namespace ppapi {

struct PPP_Instance_Combined : public PPP_Instance_0_5 {
 public:
  explicit PPP_Instance_Combined(const PPP_Instance_0_5& instance_if);
  explicit PPP_Instance_Combined(const PPP_Instance_0_4& instance_if);

  PP_Var (*const GetInstanceObject_0_4)(PP_Instance instance);

  DISALLOW_COPY_AND_ASSIGN(PPP_Instance_Combined);
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_PPP_INSTANCE_COMBINED_H_

