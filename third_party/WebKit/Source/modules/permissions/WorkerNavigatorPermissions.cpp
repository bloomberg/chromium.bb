// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/permissions/WorkerNavigatorPermissions.h"

#include "core/workers/WorkerNavigator.h"
#include "modules/permissions/Permissions.h"

namespace blink {

WorkerNavigatorPermissions::WorkerNavigatorPermissions() {}

// static
const char* WorkerNavigatorPermissions::SupplementName() {
  return "WorkerNavigatorPermissions";
}

// static
WorkerNavigatorPermissions& WorkerNavigatorPermissions::From(
    WorkerNavigator& worker_navigator) {
  WorkerNavigatorPermissions* supplement =
      static_cast<WorkerNavigatorPermissions*>(
          Supplement<WorkerNavigator>::From(worker_navigator,
                                            SupplementName()));
  if (!supplement) {
    supplement = new WorkerNavigatorPermissions();
    ProvideTo(worker_navigator, SupplementName(), supplement);
  }
  return *supplement;
}

// static
Permissions* WorkerNavigatorPermissions::permissions(
    WorkerNavigator& worker_navigator) {
  WorkerNavigatorPermissions& self =
      WorkerNavigatorPermissions::From(worker_navigator);
  if (!self.permissions_)
    self.permissions_ = new Permissions();
  return self.permissions_;
}

void WorkerNavigatorPermissions::Trace(blink::Visitor* visitor) {
  visitor->Trace(permissions_);
  Supplement<WorkerNavigator>::Trace(visitor);
}

}  // namespace blink
