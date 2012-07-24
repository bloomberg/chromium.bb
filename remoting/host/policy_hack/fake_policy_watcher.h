// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_POLICY_HACK_FAKE_POLICY_WATCHER_H_
#define REMOTING_HOST_POLICY_HACK_FAKE_POLICY_WATCHER_H_

#include "remoting/host/policy_hack/policy_watcher.h"

namespace remoting {
namespace policy_hack {

class FakePolicyWatcher : public PolicyWatcher {
 public:
  explicit FakePolicyWatcher(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  virtual ~FakePolicyWatcher();

  void SetPolicies(const base::DictionaryValue* policies);

 protected:
  virtual void StartWatchingInternal() OVERRIDE;
  virtual void StopWatchingInternal() OVERRIDE;
  virtual void Reload() OVERRIDE;
};

}  // namespace policy_hack
}  // namespace remoting

#endif  // REMOTING_HOST_POLICY_HACK_FAKE_POLICY_WATCHER_H_
