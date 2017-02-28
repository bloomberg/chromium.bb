// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScriptModule_h
#define ScriptModule_h

#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/SharedPersistent.h"
#include "core/CoreExport.h"
#include "v8/include/v8.h"
#include "wtf/Allocator.h"
#include "wtf/text/WTFString.h"

namespace blink {

class CORE_EXPORT ScriptModule final {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

 public:
  static ScriptModule compile(v8::Isolate*,
                              const String& source,
                              const String& fileName);

  ScriptModule() {}
  ScriptModule(const ScriptModule& module) : m_module(module.m_module) {}
  ~ScriptModule();

  bool instantiate(ScriptState*);
  void evaluate(ScriptState*);

  bool isNull() const { return m_module->isEmpty(); }

 private:
  ScriptModule(v8::Isolate*, v8::Local<v8::Module>);

  RefPtr<SharedPersistent<v8::Module>> m_module;
};

}  // namespace blink

#endif  // ScriptModule_h
