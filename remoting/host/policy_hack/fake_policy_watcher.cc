// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/policy_hack/fake_policy_watcher.h"

#include "base/single_thread_task_runner.h"

namespace remoting {
namespace policy_hack {

FakePolicyWatcher::FakePolicyWatcher(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : PolicyWatcher(task_runner) {
}

FakePolicyWatcher::~FakePolicyWatcher() {
}

void FakePolicyWatcher::SetPolicies(const base::DictionaryValue* policies) {
  UpdatePolicies(policies);
}

void FakePolicyWatcher::StartWatchingInternal() {
}

void FakePolicyWatcher::StopWatchingInternal() {
}

void FakePolicyWatcher::Reload() {
}

}  // namespace policy_hack
}  // namespace remoting
