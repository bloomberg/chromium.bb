// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/netinfo/WorkerNavigatorNetworkInformation.h"

#include "core/dom/ExecutionContext.h"
#include "core/workers/WorkerNavigator.h"
#include "modules/netinfo/NetworkInformation.h"
#include "platform/bindings/ScriptState.h"

namespace blink {

WorkerNavigatorNetworkInformation::WorkerNavigatorNetworkInformation(
    WorkerNavigator& navigator,
    ExecutionContext* context)
    : Supplement<WorkerNavigator>(navigator) {}

WorkerNavigatorNetworkInformation& WorkerNavigatorNetworkInformation::From(
    WorkerNavigator& navigator,
    ExecutionContext* context) {
  WorkerNavigatorNetworkInformation* supplement =
      ToWorkerNavigatorNetworkInformation(navigator, context);
  if (!supplement) {
    supplement = new WorkerNavigatorNetworkInformation(navigator, context);
    ProvideTo(navigator, SupplementName(), supplement);
  }
  return *supplement;
}

WorkerNavigatorNetworkInformation*
WorkerNavigatorNetworkInformation::ToWorkerNavigatorNetworkInformation(
    WorkerNavigator& navigator,
    ExecutionContext* context) {
  return static_cast<WorkerNavigatorNetworkInformation*>(
      Supplement<WorkerNavigator>::From(navigator, SupplementName()));
}

const char* WorkerNavigatorNetworkInformation::SupplementName() {
  return "WorkerNavigatorNetworkInformation";
}

NetworkInformation* WorkerNavigatorNetworkInformation::connection(
    ScriptState* script_state,
    WorkerNavigator& navigator) {
  ExecutionContext* context = ExecutionContext::From(script_state);
  return WorkerNavigatorNetworkInformation::From(navigator, context)
      .connection(context);
}

void WorkerNavigatorNetworkInformation::Trace(blink::Visitor* visitor) {
  visitor->Trace(connection_);
  Supplement<WorkerNavigator>::Trace(visitor);
}

NetworkInformation* WorkerNavigatorNetworkInformation::connection(
    ExecutionContext* context) {
  DCHECK(context);
  if (!connection_)
    connection_ = NetworkInformation::Create(context);
  return connection_.Get();
}

}  // namespace blink
