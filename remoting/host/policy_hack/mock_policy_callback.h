// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_POLICY_HACK_MOCK_POLICY_CALLBACK_H_
#define REMOTING_HOST_POLICY_HACK_MOCK_POLICY_CALLBACK_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace remoting {
namespace policy_hack {

class MockPolicyCallback {
 public:
  MockPolicyCallback();
  virtual ~MockPolicyCallback();

  MOCK_METHOD1(OnPolicyUpdatePtr, void(const base::DictionaryValue* policies));
  void OnPolicyUpdate(scoped_ptr<base::DictionaryValue> policies);

 private:
  DISALLOW_COPY_AND_ASSIGN(MockPolicyCallback);
};

}  // namespace policy_hack
}  // namespace remoting

#endif  // REMOTING_HOST_POLICY_HACK_MOCK_POLICY_CALLBACK_H_
