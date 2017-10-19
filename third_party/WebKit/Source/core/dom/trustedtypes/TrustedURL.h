// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TrustedURL_h
#define TrustedURL_h

#include "core/CoreExport.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/weborigin/KURL.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class ScriptState;

class CORE_EXPORT TrustedURL final
    : public GarbageCollectedFinalized<TrustedURL>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static TrustedURL* Create(const KURL& url) { return new TrustedURL(url); }

  // TrustedURL.idl
  String toString() const;
  static TrustedURL* create(ScriptState*, const String& url);
  static TrustedURL* unsafelyCreate(ScriptState*, const String& url);

  virtual void Trace(blink::Visitor* visitor) {}

 private:
  TrustedURL(const KURL&);

  KURL url_;
};

}  // namespace blink

#endif  // TrustedURL_h
