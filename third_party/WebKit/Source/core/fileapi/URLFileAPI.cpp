// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/fileapi/URLFileAPI.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/ExecutionContext.h"
#include "core/fileapi/Blob.h"
#include "core/frame/UseCounter.h"
#include "core/html/PublicURLManager.h"
#include "core/url/DOMURL.h"
#include "platform/bindings/ScriptState.h"

namespace blink {

// static
String URLFileAPI::createObjectURL(ScriptState* script_state,
                                   Blob* blob,
                                   ExceptionState& exception_state) {
  DCHECK(blob);
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  DCHECK(execution_context);

  if (blob->isClosed()) {
    // TODO(jsbell): The spec doesn't throw, but rather returns a blob: URL
    // without adding it to the store.
    exception_state.ThrowDOMException(
        kInvalidStateError,
        String(blob->IsFile() ? "File" : "Blob") + " has been closed.");
    return String();
  }

  UseCounter::Count(execution_context, WebFeature::kCreateObjectURLBlob);
  return DOMURL::CreatePublicURL(execution_context, blob, blob->Uuid());
}

// static
void URLFileAPI::revokeObjectURL(ScriptState* script_state,
                                 const String& url_string) {
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  DCHECK(execution_context);

  KURL url(NullURL(), url_string);
  execution_context->RemoveURLFromMemoryCache(url);
  execution_context->GetPublicURLManager().Revoke(url);
}

}  // namespace blink
