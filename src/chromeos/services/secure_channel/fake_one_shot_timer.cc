// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/fake_one_shot_timer.h"

#include "base/callback.h"

namespace chromeos {

namespace secure_channel {

FakeOneShotTimer::FakeOneShotTimer(
    base::OnceCallback<void(const base::UnguessableToken&)> destructor_callback)
    : base::MockOneShotTimer(),
      destructor_callback_(std::move(destructor_callback)),
      id_(base::UnguessableToken::Create()) {}

FakeOneShotTimer::~FakeOneShotTimer() {
  std::move(destructor_callback_).Run(id_);
}

}  // namespace secure_channel

}  // namespace chromeos
