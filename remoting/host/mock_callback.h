// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_MOCK_CALLBACK_H_
#define REMOTING_HOST_MOCK_CALLBACK_H_

// TODO(lukasza): If possible, move this header file to base/mock_callback.h

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"

#include "testing/gmock/include/gmock/gmock.h"

namespace remoting {

template <typename Sig>
class MockCallback;

template <typename R>
class MockCallback<R()> {
 public:
  MOCK_CONST_METHOD0_T(Run, R());

  MockCallback() {
  }

  // Caller of GetCallback has to guarantee that the returned callback
  // will not be run after |this| is destroyed.
  base::Callback<R()> GetCallback() {
    return base::Bind(&MockCallback<R()>::Run, base::Unretained(this));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCallback);
};

typedef MockCallback<void(void)> MockClosure;

}  // namespace remoting

#endif  // REMOTING_HOST_MOCK_CALLBACK_H_
