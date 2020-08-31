// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_TIMER_FACTORY_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_TIMER_FACTORY_H_

#include <memory>

#include "base/macros.h"

namespace base {
class OneShotTimer;
}  // namespace base

namespace chromeos {

namespace secure_channel {

// Creates timers. This class is needed so that tests can inject test doubles
// for timers.
class TimerFactory {
 public:
  virtual ~TimerFactory() = default;
  virtual std::unique_ptr<base::OneShotTimer> CreateOneShotTimer() = 0;

 protected:
  TimerFactory() = default;

  DISALLOW_COPY_AND_ASSIGN(TimerFactory);
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_TIMER_FACTORY_H_
