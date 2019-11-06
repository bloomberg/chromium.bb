// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/id_generator_custom_bindings.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/callback.h"

namespace extensions {

IdGeneratorCustomBindings::IdGeneratorCustomBindings(ScriptContext* context)
    : ObjectBackedNativeHandler(context) {}

void IdGeneratorCustomBindings::AddRoutes() {
  RouteHandlerFunction(
      "GetNextId", base::BindRepeating(&IdGeneratorCustomBindings::GetNextId,
                                       base::Unretained(this)));
}

void IdGeneratorCustomBindings::GetNextId(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  static int32_t next_id = 0;
  ++next_id;
  // Make sure 0 is never returned because some APIs (particularly WebRequest)
  // have special meaning for 0 IDs.
  if (next_id == 0)
    next_id = 1;
  args.GetReturnValue().Set(next_id);
}

}  // namespace extensions
