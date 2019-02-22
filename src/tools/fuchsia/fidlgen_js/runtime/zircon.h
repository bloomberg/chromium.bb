// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_FUCHSIA_FIDLGEN_JS_RUNTIME_ZIRCON_H_
#define TOOLS_FUCHSIA_FIDLGEN_JS_RUNTIME_ZIRCON_H_

#include "v8/include/v8.h"

namespace fidljs {

// Adds Zircon APIs bindings to |global|, for use by JavaScript callers.
void InjectZxBindings(v8::Isolate* isolate, v8::Local<v8::Object> global);

}  // namespace fidljs

#endif  // TOOLS_FUCHSIA_FIDLGEN_JS_RUNTIME_ZIRCON_H_
