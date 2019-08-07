// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"

#include "base/run_loop.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

OobeScreenWaiter::OobeScreenWaiter(OobeScreen target_screen)
    : target_screen_(target_screen) {}

OobeScreenWaiter::~OobeScreenWaiter() = default;

void OobeScreenWaiter::Wait() {
  DCHECK_EQ(State::IDLE, state_);

  if (GetOobeUI()->current_screen() == target_screen_) {
    state_ = State::DONE;
    return;
  }
  DCHECK(!run_loop_);

  oobe_ui_observer_.Add(GetOobeUI());

  state_ = State::WAITING_FOR_SCREEN;

  run_loop_ = std::make_unique<base::RunLoop>();
  run_loop_->Run();
  run_loop_.reset();

  DCHECK_EQ(State::DONE, state_);

  oobe_ui_observer_.RemoveAll();

  if (assert_last_screen_)
    EXPECT_EQ(target_screen_, GetOobeUI()->current_screen());
}

void OobeScreenWaiter::OnCurrentScreenChanged(OobeScreen current_screen,
                                              OobeScreen new_screen) {
  DCHECK_NE(state_, State::IDLE);

  if (state_ != State::WAITING_FOR_SCREEN) {
    if (assert_last_screen_ && new_screen != target_screen_) {
      ADD_FAILURE() << "Screen changed from the target screen "
                    << static_cast<int>(current_screen) << " -> "
                    << static_cast<int>(new_screen);
      EndWait();
    }
    return;
  }

  if (assert_next_screen_ && new_screen != target_screen_) {
    ADD_FAILURE() << "Untarget screen change to "
                  << static_cast<int>(new_screen) << " while waiting for "
                  << static_cast<int>(target_screen_);
    EndWait();
    return;
  }

  if (new_screen == target_screen_)
    EndWait();
}

void OobeScreenWaiter::OnDestroyingOobeUI() {
  oobe_ui_observer_.RemoveAll();

  EXPECT_EQ(State::DONE, state_);

  EndWait();
}

OobeUI* OobeScreenWaiter::GetOobeUI() {
  OobeUI* oobe_ui = LoginDisplayHost::default_host()->GetOobeUI();
  CHECK(oobe_ui);
  return oobe_ui;
}

void OobeScreenWaiter::EndWait() {
  if (state_ == State::DONE)
    return;

  state_ = State::DONE;
  run_loop_->Quit();
}

}  // namespace chromeos
