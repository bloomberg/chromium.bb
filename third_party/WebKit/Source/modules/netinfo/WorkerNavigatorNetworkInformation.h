// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WorkerNavigatorNetworkInformation_h
#define WorkerNavigatorNetworkInformation_h

#include "core/workers/WorkerNavigator.h"
#include "platform/Supplementable.h"

namespace blink {

class ExecutionContext;
class NetworkInformation;
class ScriptState;
class WorkerNavigator;

class WorkerNavigatorNetworkInformation final
    : public GarbageCollected<WorkerNavigatorNetworkInformation>,
      public Supplement<WorkerNavigator> {
  USING_GARBAGE_COLLECTED_MIXIN(WorkerNavigatorNetworkInformation);

 public:
  static WorkerNavigatorNetworkInformation& From(WorkerNavigator&,
                                                 ExecutionContext*);
  static WorkerNavigatorNetworkInformation* ToWorkerNavigatorNetworkInformation(
      WorkerNavigator&,
      ExecutionContext*);
  static const char* SupplementName();

  static NetworkInformation* connection(ScriptState*, WorkerNavigator&);

  virtual void Trace(blink::Visitor*);

 private:
  WorkerNavigatorNetworkInformation(WorkerNavigator&, ExecutionContext*);
  NetworkInformation* connection(ExecutionContext*);

  Member<NetworkInformation> connection_;
};

}  // namespace blink

#endif  // WorkerNavigatorNetworkInformation_h
