/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_TESTS_FAKE_BROWSER_PPAPI_TEST_SCRIPTABLE_H_
#define NATIVE_CLIENT_TESTS_FAKE_BROWSER_PPAPI_TEST_SCRIPTABLE_H_

#include "ppapi/c/ppb_core.h"
#include "ppapi/c/ppb_instance.h"
#include "ppapi/c/ppb_var.h"

void TestScriptableObject(PP_Var receiver,
                          const PPB_Instance* browser_instance_interface,
                          const PPB_Var* var_interface,
                          PP_Instance instance_id);

#endif  // NATIVE_CLIENT_TESTS_FAKE_BROWSER_PPAPI_TEST_SCRIPTABLE_H_
