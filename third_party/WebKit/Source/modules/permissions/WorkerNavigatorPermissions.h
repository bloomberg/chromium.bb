// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WorkerNavigatorPermissions_h
#define WorkerNavigatorPermissions_h

#include "core/workers/WorkerNavigator.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"

namespace blink {

class WorkerNavigator;
class Permissions;

class WorkerNavigatorPermissions final
    : public GarbageCollected<WorkerNavigatorPermissions>,
      public Supplement<WorkerNavigator> {
  USING_GARBAGE_COLLECTED_MIXIN(WorkerNavigatorPermissions);

 public:
  static WorkerNavigatorPermissions& From(WorkerNavigator&);
  static Permissions* permissions(WorkerNavigator&);

  virtual void Trace(blink::Visitor*);

 private:
  static const char* SupplementName();

  WorkerNavigatorPermissions();

  Member<Permissions> permissions_;
};

}  // namespace blink

#endif  // WorkerNavigatorPermissions_h
