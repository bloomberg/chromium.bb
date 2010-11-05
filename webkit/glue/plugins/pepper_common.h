// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_PEPPER_COMMON_H_
#define WEBKIT_GLUE_PLUGINS_PEPPER_COMMON_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_var.h"

namespace pepper {

inline PP_Bool BoolToPPBool(bool value) {
  return value ? PP_TRUE : PP_FALSE;
}

inline bool PPBoolToBool(PP_Bool value) {
  return (PP_TRUE == value);
}

}  // namespace pepper

#endif  // WEBKIT_GLUE_PLUGINS_PEPPER_COMMON_H_

