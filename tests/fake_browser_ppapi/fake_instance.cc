/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <stdio.h>
#include "native_client/tests/fake_browser_ppapi/fake_instance.h"

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/tests/fake_browser_ppapi/fake_window.h"
#include "ppapi/c/ppb_instance.h"

using ppapi_proxy::DebugPrintf;

namespace fake_browser_ppapi {

PP_Var Instance::GetWindowObject() {
  DebugPrintf("Instance::GetWindowObject: instance=%p\n",
              reinterpret_cast<void*>(this));
  return window_->FakeWindowObject();
}

PP_Var Instance::GetOwnerElementObject() {
  DebugPrintf("Instance::GetOwnerElementObject: instance=%p\n",
              reinterpret_cast<void*>(this));
  NACL_UNIMPLEMENTED();
  return PP_MakeUndefined();
}

bool Instance::BindGraphics(PP_Resource device) {
  DebugPrintf("Instance::BindGraphicsDeviceContext: instance=%p"
              ", device=%"NACL_PRIu64"\n",
              reinterpret_cast<void*>(this), device);
  NACL_UNIMPLEMENTED();
  return false;
}

bool Instance::IsFullFrame() {
  DebugPrintf("Instance::IsFullFrame: instance=%p\n",
              reinterpret_cast<void*>(this));
  NACL_UNIMPLEMENTED();
  return false;
}

PP_Var Instance::ExecuteScript(PP_Var script,
                               PP_Var* exception) {
  DebugPrintf("Instance::ExecuteScript: instance=%p\n",
              reinterpret_cast<void*>(this));
  NACL_UNIMPLEMENTED();
  UNREFERENCED_PARAMETER(script);
  UNREFERENCED_PARAMETER(exception);
  return PP_MakeUndefined();
}

}  // namespace fake_browser_ppapi
