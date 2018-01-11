// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ModuleScriptLoaderClient_h
#define ModuleScriptLoaderClient_h

#include "platform/heap/Handle.h"

namespace blink {

class ModuleScript;

// A ModuleScriptLoaderClient is notified when a single module script load is
// complete.
// Note: Its corresponding module map entry is typically not yet created at the
// time of callback.
class ModuleScriptLoaderClient : public GarbageCollectedMixin {
 public:
  virtual ~ModuleScriptLoaderClient() = default;
  ;

 private:
  friend class ModuleScriptLoader;
  friend class ModuleMapTestModulator;

  virtual void NotifyNewSingleModuleFinished(ModuleScript*) = 0;
};

}  // namespace blink

#endif
