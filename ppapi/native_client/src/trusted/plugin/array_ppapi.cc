// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <assert.h>

#include "native_client/src/include/nacl_string.h"
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/trusted/plugin/utility.h"
#include "native_client/src/trusted/plugin/array_ppapi.h"


namespace plugin {

ArrayPpapi::ArrayPpapi(Plugin* instance) {
  PLUGIN_PRINTF(("ArrayPpapi::ArrayPpapi (this=%p, instance=%p)\n",
                 static_cast<void*>(this), static_cast<void*>(instance)));
  pp::VarPrivate window = instance->GetWindowObject();
  PLUGIN_PRINTF(("ArrayPpapi::ArrayPpapi (window=%d)\n",
                !window.is_undefined()));
  js_array_ = window.Call(pp::Var("eval"), pp::Var("new Array;"));
  PLUGIN_PRINTF(("ArrayPpapi::ArrayPpapi (js_array_=%d)\n",
                 !js_array_.is_undefined()));
  assert(!js_array_.is_undefined());
}

}  // namespace plugin
