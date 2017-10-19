// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TrustedScriptURL_h
#define TrustedScriptURL_h

#include "core/CoreExport.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/weborigin/KURL.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class ScriptState;

class CORE_EXPORT TrustedScriptURL final
    : public GarbageCollectedFinalized<TrustedScriptURL>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static TrustedScriptURL* Create(const KURL& url) {
    return new TrustedScriptURL(url);
  }

  // TrustedScriptURL.idl
  String toString() const;
  static TrustedScriptURL* unsafelyCreate(ScriptState*, const String& url);

  virtual void Trace(blink::Visitor* visitor) {}

 private:
  TrustedScriptURL(const KURL&);

  KURL url_;
};

}  // namespace blink

#endif  // TrustedScriptURL_h
