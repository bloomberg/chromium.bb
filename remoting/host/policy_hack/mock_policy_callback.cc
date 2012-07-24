// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/policy_hack/mock_policy_callback.h"

namespace remoting {
namespace policy_hack {

MockPolicyCallback::MockPolicyCallback() {
}

MockPolicyCallback::~MockPolicyCallback() {
}

void MockPolicyCallback::OnPolicyUpdate(
    scoped_ptr<base::DictionaryValue> policies) {
  OnPolicyUpdatePtr(policies.get());
}

}  // namespace policy_hack
}  // namespace remoting
