// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/fake_oauth_token_getter.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"

namespace remoting {

FakeOAuthTokenGetter::FakeOAuthTokenGetter(Status status,
                                           const std::string& user_email,
                                           const std::string& access_token)
    : status_(status), user_email_(user_email), access_token_(access_token) {}

FakeOAuthTokenGetter::~FakeOAuthTokenGetter() = default;

void FakeOAuthTokenGetter::CallWithToken(TokenCallback on_access_token) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(on_access_token), status_,
                                user_email_, access_token_));
}

void FakeOAuthTokenGetter::InvalidateCache() {
  NOTIMPLEMENTED();
}

}  // namespace remoting
