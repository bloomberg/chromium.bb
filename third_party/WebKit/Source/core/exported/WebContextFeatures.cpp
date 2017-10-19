// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/web/WebContextFeatures.h"

#include "core/context_features/ContextFeatureSettings.h"
#include "platform/bindings/DOMWrapperWorld.h"
#include "platform/bindings/ScriptState.h"

namespace blink {

// static
void WebContextFeatures::EnableMojoJS(v8::Local<v8::Context> context,
                                      bool enable) {
  ScriptState* script_state = ScriptState::From(context);
  DCHECK(script_state->World().IsMainWorld());
  ContextFeatureSettings::From(
      ExecutionContext::From(script_state),
      ContextFeatureSettings::CreationMode::kCreateIfNotExists)
      ->enableMojoJS(enable);
}

}  // namespace blink
