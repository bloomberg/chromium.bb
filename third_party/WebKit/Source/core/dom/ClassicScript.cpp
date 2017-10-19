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
#include "platform/loader/fetch/AccessControlStatus.h"
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
  const bool is_external_script = GetScriptSourceCode().GetResource();

  AccessControlStatus access_control_status = kNotSharableCrossOrigin;
  if (!is_external_script) {
    access_control_status = kSharableCrossOrigin;
  } else {
    CHECK(GetScriptSourceCode().GetResource());
    access_control_status =
        GetScriptSourceCode().GetResource()->CalculateAccessControlStatus();
  }

  frame->GetScriptController().ExecuteScriptInMainWorld(
      GetScriptSourceCode(), FetchOptions(), access_control_status);
}

}  // namespace blink
