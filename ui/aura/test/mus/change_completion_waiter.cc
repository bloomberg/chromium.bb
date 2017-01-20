// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/test/mus/change_completion_waiter.h"

#include "base/callback.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/mus/in_flight_change.h"
#include "ui/aura/mus/window_tree_client.h"

namespace aura {
namespace test {

ChangeCompletionWaiter::ChangeCompletionWaiter(WindowTreeClient* client,
                                               ChangeType type,
                                               bool success)
    : state_(WaitState::NOT_STARTED),
      client_(client),
      type_(type),
      success_(success) {
  client->AddTestObserver(this);
}

ChangeCompletionWaiter::~ChangeCompletionWaiter() {
  client_->RemoveTestObserver(this);
}

void ChangeCompletionWaiter::Wait() {
  if (state_ != WaitState::RECEIVED) {
    quit_closure_ = run_loop_.QuitClosure();
    run_loop_.Run();
  }
}

void ChangeCompletionWaiter::OnChangeStarted(uint32_t change_id,
                                             aura::ChangeType type) {
  if (state_ == WaitState::NOT_STARTED && type == type_) {
    state_ = WaitState::WAITING;
    change_id_ = change_id;
  }
}

void ChangeCompletionWaiter::OnChangeCompleted(uint32_t change_id,
                                               aura::ChangeType type,
                                               bool success) {
  if (state_ == WaitState::WAITING && change_id_ == change_id) {
    EXPECT_EQ(success_, success);
    state_ = WaitState::RECEIVED;
    if (quit_closure_)
      quit_closure_.Run();
  }
}

}  // namespace test
}  // namespace aura
