// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScriptModuleResolver_h
#define ScriptModuleResolver_h

#include "bindings/core/v8/ExceptionState.h"
#include "platform/heap/Handle.h"
#include "wtf/text/WTFString.h"

namespace blink {

class ScriptModule;
class ModuleScript;

// The ScriptModuleResolver interface is used from V8 module bindings
// when it need the ScriptModule's descendants.
//
// When a module writes import 'x', the module is called the referrer, 'x' is
// the specifier, and the module identified by 'x' is the descendant.
// ScriptModuleResolver, given a referrer and specifier, can look up the
// descendant.
class ScriptModuleResolver : public GarbageCollected<ScriptModuleResolver> {
 public:
  DEFINE_INLINE_VIRTUAL_TRACE() {}

  // Notify the ScriptModuleResolver that a ModuleScript exists.
  // This hook gives a chance for the resolver impl to populate module record
  // identifier -> ModuleScript mapping entry.
  virtual void registerModuleScript(ModuleScript*) = 0;

  // Implements "Runtime Semantics: HostResolveImportedModule"
  // https://tc39.github.io/ecma262/#sec-hostresolveimportedmodule
  // This returns a null ScriptModule when an exception is thrown.
  virtual ScriptModule resolve(const String& specifier,
                               const ScriptModule& referrer,
                               ExceptionState&) = 0;
};

}  // namespace blink

#endif  // ScriptModuleResolver_h
