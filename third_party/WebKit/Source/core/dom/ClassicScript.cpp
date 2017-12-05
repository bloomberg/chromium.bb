// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/ClassicScript.h"

#include "bindings/core/v8/ScriptController.h"
#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/UseCounter.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/loader/AllowedByNosniff.h"
#include "platform/network/mime/MIMETypeRegistry.h"

namespace blink {

void ClassicScript::Trace(blink::Visitor* visitor) {
  Script::Trace(visitor);
  visitor->Trace(script_source_code_);
}

bool ClassicScript::CheckMIMETypeBeforeRunScript(
    Document* context_document,
    const SecurityOrigin* security_origin) const {
  return AllowedByNosniff::MimeTypeAsScript(
      context_document, GetScriptSourceCode().GetResource()->GetResponse());
}

void ClassicScript::RunScript(LocalFrame* frame,
                              const SecurityOrigin* security_origin) const {
  frame->GetScriptController().ExecuteScriptInMainWorld(
      GetScriptSourceCode(), FetchOptions(), access_control_status_);
}

}  // namespace blink
