// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_COMPOSITOR_VSYNC_MANAGER_H_
#define UI_COMPOSITOR_COMPOSITOR_VSYNC_MANAGER_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "ui/compositor/compositor_export.h"

namespace ui {

// This class manages vsync parameters for a compositor. It merges updates of
// the parameters from different sources and sends the merged updates to
// observers which register to it.
class COMPOSITOR_EXPORT CompositorVSyncManager
    : public base::RefCounted<CompositorVSyncManager> {
 public:
  class Observer {
   public:
    virtual ~Observer() = default;
    virtual void OnUpdateVSyncParameters(base::TimeTicks timebase,
                                         base::TimeDelta interval) = 0;
  };

  CompositorVSyncManager();

  // The vsync parameters consist of |timebase|, which is the platform timestamp
  // of the last vsync, and |interval|, which is the interval between vsyncs.
  void UpdateVSyncParameters(base::TimeTicks timebase,
                             base::TimeDelta interval);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  friend class base::RefCounted<CompositorVSyncManager>;

  ~CompositorVSyncManager();

  void NotifyObservers(base::TimeTicks timebase, base::TimeDelta interval);

  base::ObserverList<Observer>::Unchecked observer_list_;

  base::TimeTicks last_timebase_;
  base::TimeDelta last_interval_;

  DISALLOW_COPY_AND_ASSIGN(CompositorVSyncManager);
};

}  // namespace ui

#endif  // UI_COMPOSITOR_COMPOSITOR_VSYNC_MANAGER_H_
