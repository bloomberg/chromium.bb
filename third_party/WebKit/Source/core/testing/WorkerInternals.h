// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WorkerInternals_h
#define WorkerInternals_h

#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/GarbageCollected.h"

namespace blink {

class ExceptionState;
class OriginTrialsTest;
class ScriptState;

class WorkerInternals final : public GarbageCollectedFinalized<WorkerInternals>,
                              public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static WorkerInternals* Create() { return new WorkerInternals(); }
  virtual ~WorkerInternals();

  OriginTrialsTest* originTrialsTest() const;
  void countFeature(ScriptState*, uint32_t feature, ExceptionState&);
  void countDeprecation(ScriptState*, uint32_t feature, ExceptionState&);

  void collectGarbage(ScriptState*);

  void Trace(blink::Visitor* visitor) {}

 private:
  explicit WorkerInternals();
};

}  // namespace blink

#endif  // WorkerInternals_h
