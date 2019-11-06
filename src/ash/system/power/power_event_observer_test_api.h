// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_POWER_POWER_EVENT_OBSERVER_TEST_API_H_
#define ASH_SYSTEM_POWER_POWER_EVENT_OBSERVER_TEST_API_H_

#include "base/macros.h"

namespace ui {
class Compositor;
}

namespace ash {

class PowerEventObserver;

class PowerEventObserverTestApi {
 public:
  explicit PowerEventObserverTestApi(PowerEventObserver* power_event_observer);
  ~PowerEventObserverTestApi();

  void CompositingDidCommit(ui::Compositor* compositor);
  void CompositingStarted(ui::Compositor* compositor);
  void CompositingEnded(ui::Compositor* compositor);

  // Same as calling CompositingDidCommit, CompositingStarted and
  // CompositingEnded in sequence.
  void CompositeFrame(ui::Compositor* compositor);

  bool SimulateCompositorsReadyForSuspend();

 private:
  PowerEventObserver* power_event_observer_;

  DISALLOW_COPY_AND_ASSIGN(PowerEventObserverTestApi);
};

}  // namespace ash

#endif  // ASH_SYSTEM_POWER_POWER_EVENT_OBSERVER_TEST_API_H_
