// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_NETWORK_ICON_PURGER_H_
#define ASH_SYSTEM_NETWORK_NETWORK_ICON_PURGER_H_

#include "base/macros.h"
#include "base/timer/timer.h"
#include "chromeos/network/network_state_handler_observer.h"

namespace ash {

// Purges network icon caches when the network list is updated.
// Introduces a small delay before purging the cache. This prevents
// us from excessively purging icon caches when receiving updates
// in quick succession.
class NetworkIconPurger : public chromeos::NetworkStateHandlerObserver {
 public:
  NetworkIconPurger();
  ~NetworkIconPurger() override;

  // chromeos::NetworkStateHandlerObserver
  void NetworkListChanged() override;

 private:
  base::OneShotTimer timer_;

  DISALLOW_COPY_AND_ASSIGN(NetworkIconPurger);
};

}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_NETWORK_ICON_PURGER_H_
