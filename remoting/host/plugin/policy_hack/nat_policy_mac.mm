// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/plugin/policy_hack/nat_policy.h"

#include "base/compiler_specific.h"
#include "base/message_loop_proxy.h"
#include "base/scoped_ptr.h"
#include "base/values.h"

namespace remoting {
namespace policy_hack {

class NatPolicyMac : public NatPolicy {
 public:
  explicit NatPolicyMac(base::MessageLoopProxy* message_loop_proxy)
     : NatPolicy(message_loop_proxy) {
  }

  virtual ~NatPolicyMac() {
  }

  virtual void StartWatchingInternal() OVERRIDE {
    scoped_ptr<base::DictionaryValue> new_policy(new base::DictionaryValue());
    UpdateNatPolicy(new_policy.get());
  }

  virtual void StopWatchingInternal() OVERRIDE {
  }

  virtual void Reload() OVERRIDE {
  }
};

NatPolicy* NatPolicy::Create(base::MessageLoopProxy* message_loop_proxy) {
  return new NatPolicyMac(message_loop_proxy);
}

}  // namespace policy_hack
}  // namespace remoting
