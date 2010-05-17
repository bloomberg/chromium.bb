// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_PEPPER_VAR_H_
#define WEBKIT_GLUE_PLUGINS_PEPPER_VAR_H_

typedef struct _ppb_Var PPB_Var;

namespace pepper {

// There's no class implementing Var since it could represent a number of
// objects. Instead, we just expose a getter for the interface implemented in
// the .cc file here.
const PPB_Var* GetVarInterface();

}  // namespace pepper

#endif  // WEBKIT_GLUE_PLUGINS_PEPPER_VAR_H_
