// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPB_VAR_IMPL_H_
#define WEBKIT_PLUGINS_PPAPI_PPB_VAR_IMPL_H_

struct PPB_Var;
struct PPB_Var_Deprecated;

namespace webkit {
namespace ppapi {

class PPB_Var_Impl {
 public:
  static const PPB_Var* GetVarInterface();
  static const PPB_Var_Deprecated* GetVarDeprecatedInterface();
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PPB_VAR_IMPL_H_
