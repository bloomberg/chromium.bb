// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INSTANCE_THROTTLE_ARC_INSTANCE_THROTTLE_H_
#define CHROME_BROWSER_ASH_ARC_INSTANCE_THROTTLE_ARC_INSTANCE_THROTTLE_H_

#include <memory>
#include <string>
#include <utility>

#include "ash/components/arc/arc_util.h"
#include "ash/components/arc/session/connection_observer.h"
#include "chrome/browser/ash/throttle_observer.h"
#include "chrome/browser/ash/throttle_service.h"
#include "components/keyed_service/core/keyed_service.h"

namespace base {
class TimeDelta;
}

namespace content {
class BrowserContext;
}

namespace arc {
class ArcBridgeService;

namespace mojom {
class PowerInstance;
}

// This class holds a number of observers which watch for several conditions
// (window activation, mojom instance connection, etc) and adjusts the
// throttling state of the ARC container on a change in conditions.
class ArcInstanceThrottle : public KeyedService,
                            public ash::ThrottleService,
                            public ConnectionObserver<mojom::PowerInstance> {
 public:
  class Delegate {
   public:
    Delegate() = default;
    virtual ~Delegate() = default;

    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;

    virtual void SetCpuRestriction(
        CpuRestrictionState cpu_restriction_state) = 0;
    virtual void RecordCpuRestrictionDisabledUMA(
        const std::string& observer_name,
        base::TimeDelta delta) = 0;
  };

  // Returns singleton instance for the given BrowserContext, or nullptr if
  // the browser |context| is not allowed to use ARC.
  static ArcInstanceThrottle* GetForBrowserContext(
      content::BrowserContext* context);
  static ArcInstanceThrottle* GetForBrowserContextForTesting(
      content::BrowserContext* context);

  ArcInstanceThrottle(content::BrowserContext* context,
                      ArcBridgeService* bridge);
  ~ArcInstanceThrottle() override;

  ArcInstanceThrottle(const ArcInstanceThrottle&) = delete;
  ArcInstanceThrottle& operator=(const ArcInstanceThrottle&) = delete;

  // KeyedService:
  void Shutdown() override;

  // ConnectionObserver<mojom::PowerInstance>:
  void OnConnectionReady() override;

  void set_delegate_for_testing(std::unique_ptr<Delegate> delegate) {
    delegate_ = std::move(delegate);
  }

 private:
  // ash::ThrottleService:
  void ThrottleInstance(ash::ThrottleObserver::PriorityLevel level) override;
  void RecordCpuRestrictionDisabledUMA(const std::string& observer_name,
                                       base::TimeDelta delta) override;

  // Notifies CPU resetriction state to power mojom.
  void NotifyCpuRestriction(CpuRestrictionState cpu_restriction_state);

  std::unique_ptr<Delegate> delegate_;
  // Owned by ArcServiceManager.
  ArcBridgeService* const bridge_;
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_INSTANCE_THROTTLE_ARC_INSTANCE_THROTTLE_H_
