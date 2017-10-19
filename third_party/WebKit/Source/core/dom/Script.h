// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Script_h
#define Script_h

#include "core/CoreExport.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/loader/fetch/ScriptFetchOptions.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class Document;
class LocalFrame;
class SecurityOrigin;

enum class ScriptType { kClassic, kModule };

// https://html.spec.whatwg.org/#concept-script
class CORE_EXPORT Script : public GarbageCollectedFinalized<Script> {
 public:
  virtual void Trace(blink::Visitor* visitor) {}

  virtual ~Script() {}

  virtual ScriptType GetScriptType() const = 0;

  // Returns false if the script should not be run due to MIME type check.
  virtual bool CheckMIMETypeBeforeRunScript(Document* context_document,
                                            const SecurityOrigin*) const = 0;

  // https://html.spec.whatwg.org/#run-a-classic-script or
  // https://html.spec.whatwg.org/#run-a-module-script,
  // depending on the script type.
  virtual void RunScript(LocalFrame*, const SecurityOrigin*) const = 0;

  // For CSP check for inline scripts.
  virtual String InlineSourceTextForCSP() const = 0;

  const ScriptFetchOptions& FetchOptions() const { return fetch_options_; }

 protected:
  explicit Script(const ScriptFetchOptions& fetch_options)
      : fetch_options_(fetch_options) {}

 private:
  // https://html.spec.whatwg.org/#concept-script-script-fetch-options
  const ScriptFetchOptions fetch_options_;
};

}  // namespace blink

#endif
