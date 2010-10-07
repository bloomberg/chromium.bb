/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_TESTS_FAKE_BROWSER_PPAPI_TEST_SCRIPTABLE_H_
#define NATIVE_CLIENT_TESTS_FAKE_BROWSER_PPAPI_TEST_SCRIPTABLE_H_

#include "ppapi/c/dev/ppb_var_deprecated.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/c/ppb_instance.h"

void TestScriptableObject(PP_Var receiver,
                          const PPB_Instance* browser_instance_interface,
                          const PPB_Var_Deprecated* var_interface,
                          PP_Instance instance_id,
                          PP_Module browser_module_id);

#endif  // NATIVE_CLIENT_TESTS_FAKE_BROWSER_PPAPI_TEST_SCRIPTABLE_H_
