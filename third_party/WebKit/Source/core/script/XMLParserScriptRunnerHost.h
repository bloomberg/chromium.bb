// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef XMLParserScriptRunnerHost_h
#define XMLParserScriptRunnerHost_h

#include "core/CoreExport.h"
#include "platform/heap/GarbageCollected.h"

namespace blink {

class CORE_EXPORT XMLParserScriptRunnerHost : public GarbageCollectedMixin {
 public:
  virtual ~XMLParserScriptRunnerHost() {}
  virtual void Trace(Visitor*) {}

  virtual void NotifyScriptExecuted() = 0;
};

}  // namespace blink

#endif
