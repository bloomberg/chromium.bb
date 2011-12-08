// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_PPB_VAR_SHARED_H_
#define PPAPI_SHARED_IMPL_PPB_VAR_SHARED_H_

#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/shared_impl/ppapi_shared_export.h"

struct PP_Var;
struct PPB_Var;
struct PPB_Var_1_0;

namespace ppapi {

class PPAPI_SHARED_EXPORT PPB_Var_Shared {
 public:
  static const PPB_Var* GetVarInterface();
  static const PPB_Var_1_0* GetVarInterface1_0();
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_PPB_VAR_SHARED_H_
