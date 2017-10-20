// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DocumentModulatorImpl_h
#define DocumentModulatorImpl_h

#include "core/dom/ModulatorImplBase.h"

#include "platform/heap/Handle.h"

namespace blink {

class ModuleScriptFetcher;
class ResourceFetcher;
class ScriptState;

// DocumentModulatorImpl is the Modulator implementation used in main documents
// (that means, not worker nor worklets). Module operations depending on the
// Document context should be implemented in this class, not in
// ModulatorImplBase.
class DocumentModulatorImpl final : public ModulatorImplBase {
 public:
  static ModulatorImplBase* Create(scoped_refptr<ScriptState>,
                                   ResourceFetcher*);

  // Implements Modulator.
  ModuleScriptFetcher* CreateModuleScriptFetcher() override;

  void Trace(blink::Visitor*);

 private:
  DocumentModulatorImpl(scoped_refptr<ScriptState>, ResourceFetcher*);
  Member<ResourceFetcher> fetcher_;
};

}  // namespace blink

#endif  // DocumentModulatorImpl_h
