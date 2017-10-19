// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TrustedHTML_h
#define TrustedHTML_h

#include "core/CoreExport.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class ScriptState;

class CORE_EXPORT TrustedHTML final
    : public GarbageCollectedFinalized<TrustedHTML>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static TrustedHTML* Create(const String& html) {
    return new TrustedHTML(html);
  }

  // CredentialsContainer.idl
  String toString() const;
  static TrustedHTML* escape(ScriptState*, const String& html);
  static TrustedHTML* unsafelyCreate(ScriptState*, const String& html);

  virtual void Trace(blink::Visitor* visitor) {}

 private:
  TrustedHTML(const String& html);

  const String html_;
};

}  // namespace blink

#endif  // TrustedHTML_h
