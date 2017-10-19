// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DynamicModuleResolver_h
#define DynamicModuleResolver_h

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "platform/heap/Visitor.h"
#include "platform/weborigin/KURL.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class Modulator;
class ReferrerScriptInfo;
class ScriptPromiseResolver;

// DynamicModuleResolver implements "Runtime Semantics:
// HostImportModuleDynamically" per spec.
// https://tc39.github.io/proposal-dynamic-import/#sec-hostimportmoduledynamically
class CORE_EXPORT DynamicModuleResolver final
    : public GarbageCollected<DynamicModuleResolver> {
 public:
  void Trace(blink::Visitor*);

  static DynamicModuleResolver* Create(Modulator* modulator) {
    return new DynamicModuleResolver(modulator);
  }

  // Implements "HostImportModuleDynamically" semantics.
  // Should be called w/ a valid V8 context.
  void ResolveDynamically(const String& specifier,
                          const KURL& referrer_url,
                          const ReferrerScriptInfo& referrer_info,
                          ScriptPromiseResolver*);

 private:
  explicit DynamicModuleResolver(Modulator* modulator)
      : modulator_(modulator) {}

  Member<Modulator> modulator_;
};

}  // namespace blink

#endif  // DynamicModuleResolver_h
