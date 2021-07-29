// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BREADCRUMBS_CORE_BREADCRUMB_MANAGER_H_
#define COMPONENTS_BREADCRUMBS_CORE_BREADCRUMB_MANAGER_H_

#include <list>
#include <string>
#include <utility>

#include "base/observer_list.h"
#include "base/time/time.h"

namespace breadcrumbs {

class BreadcrumbManagerObserver;

// Stores events logged with |AddEvent| in memory which can later be retrieved
// with |GetEvents|. Events will be silently dropped after a certain amount of
// time has passed unless no more recent events are available. The internal
// management of events aims to keep relevant events available while clearing
// stale data.
class BreadcrumbManager {
 public:
  BreadcrumbManager();
  BreadcrumbManager(const BreadcrumbManager&) = delete;
  BreadcrumbManager& operator=(const BreadcrumbManager&) = delete;
  ~BreadcrumbManager();

  // Returns the number of collected breadcrumb events which are still relevant.
  // Note: This method may drop old events so the value can change even when no
  // new events have been added, but time has passed.
  size_t GetEventCount();

  // Returns a list of the collected breadcrumb events which are still relevant
  // up to |event_count_limit|. Passing zero for |event_count_limit| signifies
  // no limit. Events returned will have a timestamp prepended to the original
  // |event| string representing when |AddEvent| was called.
  // Note: This method may drop old events so the returned events can change
  // even if no new events have been added, but time has passed.
  const std::list<std::string> GetEvents(size_t event_count_limit);

  // Logs a breadcrumb event with message data |event|.
  // NOTE: |event| must not include newline characters as newlines are used by
  // BreadcrumbPersistentStore as a deliminator.
  void AddEvent(const std::string& event);

  // Adds and removes observers.
  void AddObserver(BreadcrumbManagerObserver* observer);
  void RemoveObserver(BreadcrumbManagerObserver* observer);

 private:
  // Drops events which are considered stale. Note that stale events are not
  // guaranteed to be removed. Explicitly, stale events will be retained while
  // newer events are limited.
  void DropOldEvents();

  // Creation time of the BreadcrumbManager.
  const base::Time start_time_;

  // List of events, paired with the time they were logged to minute resolution.
  // Newer events are at the end of the list.
  struct EventBucket {
    base::Time time;
    std::list<std::string> events;

    explicit EventBucket(base::Time bucket_time);
    EventBucket(const EventBucket&);
    ~EventBucket();
  };
  std::list<EventBucket> event_buckets_;

  base::ObserverList<BreadcrumbManagerObserver, /*check_empty=*/true>
      observers_;
};

}  // namespace breadcrumbs

#endif  // COMPONENTS_BREADCRUMBS_CORE_BREADCRUMB_MANAGER_H_
