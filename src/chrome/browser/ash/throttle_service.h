// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_THROTTLE_SERVICE_H_
#define CHROME_BROWSER_ASH_THROTTLE_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "chrome/browser/ash/throttle_observer.h"

namespace content {
class BrowserContext;
}

namespace ash {

// This class is the base for different throttle services on ChromeOS.
// The class holds a number of ThrottleObservers which watch for several
// conditions. When the observers change from active to inactive or vice-versa,
// OnObserverStateChanged finds the active observer with the highest
// PriorityLevel, and calls ThrottleInstance with that level.
class ThrottleService {
 public:
  class ServiceObserver : public base::CheckedObserver {
   public:
    // Notifies that throttling has been changed.
    virtual void OnThrottle(ThrottleObserver::PriorityLevel level) = 0;
  };

  explicit ThrottleService(content::BrowserContext* context);

  ThrottleService(const ThrottleService&) = delete;
  ThrottleService& operator=(const ThrottleService&) = delete;

  virtual ~ThrottleService();

  void AddServiceObserver(ServiceObserver* observer);
  void RemoveServiceObserver(ServiceObserver* observer);

  // Functions for testing
  void NotifyObserverStateChangedForTesting();
  void SetObserversForTesting(
      std::vector<std::unique_ptr<ThrottleObserver>> observers);

  // Sets enforced mode when level is fixed regardless of other observers.
  // Setting this to ThrottleObserver::PriorityLevel::UNKNOWN effectifly
  // switches to auto mode.
  void SetEnforced(ThrottleObserver::PriorityLevel level);

  ThrottleObserver::PriorityLevel level() const { return level_; }
  ThrottleObserver::PriorityLevel enforced_level() const {
    return enforced_level_;
  }
  void set_level_for_testing(ThrottleObserver::PriorityLevel level);

 protected:
  void AddObserver(std::unique_ptr<ThrottleObserver> observer);
  void StartObservers();
  void StopObservers();
  void OnObserverStateChanged();
  void SetLevel(ThrottleObserver::PriorityLevel level);

  // This function is called whenever there is a new level to which the
  // cgroup should be throttled. Derived classes should implement
  // ThrottleInstance to adjust the throttling state of their relevant cgroup.
  virtual void ThrottleInstance(ThrottleObserver::PriorityLevel level) = 0;

  // Whenever there is a change in the effective observer (active observer with
  // the highest PriorityLevel), this function is called with the name of the
  // previously effective observer and the duration it was effective. Derived
  // classes can implement this function to record UMA metrics.
  virtual void RecordCpuRestrictionDisabledUMA(const std::string& observer_name,
                                               base::TimeDelta delta) = 0;

  const std::vector<std::unique_ptr<ThrottleObserver>>& observers() const {
    return observers_;
  }
  content::BrowserContext* context() const { return context_; }

 private:
  content::BrowserContext* const context_;
  std::vector<std::unique_ptr<ThrottleObserver>> observers_;
  ThrottleObserver::PriorityLevel level_{
      ThrottleObserver::PriorityLevel::UNKNOWN};
  ThrottleObserver::PriorityLevel enforced_level_ = {
      ThrottleObserver::PriorityLevel::UNKNOWN};
  ThrottleObserver* last_effective_observer_{nullptr};
  base::TimeTicks last_throttle_transition_;
  base::ObserverList<ServiceObserver> service_observers_;
  base::WeakPtrFactory<ThrottleService> weak_ptr_factory_{this};
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove after the migration is finished.
namespace chromeos {
using ::ash::ThrottleService;
}

#endif  // CHROME_BROWSER_ASH_THROTTLE_SERVICE_H_
