// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/script/ClassicScript.h"

#include "bindings/core/v8/ScriptController.h"
#include "core/frame/LocalFrame.h"

namespace blink {

void ClassicScript::Trace(blink::Visitor* visitor) {
  Script::Trace(visitor);
  visitor->Trace(script_source_code_);
}

void ClassicScript::RunScript(LocalFrame* frame,
                              const SecurityOrigin* security_origin) const {
  frame->GetScriptController().ExecuteScriptInMainWorld(
      GetScriptSourceCode(), BaseURL(), FetchOptions(), access_control_status_);
}

}  // namespace blink
