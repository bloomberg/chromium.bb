// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_ASYNC_POLICY_LOADER_H_
#define COMPONENTS_POLICY_CORE_COMMON_ASYNC_POLICY_LOADER_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/policy/core/common/schema_map.h"
#include "components/policy/policy_export.h"

namespace base {
class SequencedTaskRunner;
}

namespace policy {

class PolicyBundle;

// Base implementation for platform-specific policy loaders. Together with the
// AsyncPolicyProvider, this base implementation takes care of the initial load,
// periodic reloads, watching file changes, refreshing policies and object
// lifetime.
//
// All methods are invoked on the background |task_runner_|, including the
// destructor. The only exceptions are the constructor (which may be called on
// any thread), InitialLoad() which is called on the thread that owns the
// provider and the calls of Load() and LastModificationTime() during the
// initial load.
// Also, during tests the destructor may be called on the main thread.
class POLICY_EXPORT AsyncPolicyLoader {
 public:
  explicit AsyncPolicyLoader(
      const scoped_refptr<base::SequencedTaskRunner>& task_runner);
  virtual ~AsyncPolicyLoader();

  // Gets a SequencedTaskRunner backed by the background thread.
  base::SequencedTaskRunner* task_runner() const { return task_runner_.get(); }

  // Returns the currently configured policies. Load() is always invoked on
  // the background thread, except for the initial Load() at startup which is
  // invoked from the thread that owns the provider.
  virtual std::unique_ptr<PolicyBundle> Load() = 0;

  // Allows implementations to finalize their initialization on the background
  // thread (e.g. setup file watchers).
  virtual void InitOnBackgroundThread() = 0;

  // Implementations should return the time of the last modification detected,
  // or base::Time() if it doesn't apply, which is the default.
  virtual base::Time LastModificationTime();

  // Used by the AsyncPolicyProvider to do the initial Load(). The first load
  // is also used to initialize |last_modification_time_| and
  // |schema_map_|.
  std::unique_ptr<PolicyBundle> InitialLoad(
      const scoped_refptr<SchemaMap>& schemas);

  // Implementations should invoke Reload() when a change is detected. This
  // must be invoked from the background thread and will trigger a Load(),
  // and pass the returned bundle to the provider.
  // The load is immediate when |force| is true. Otherwise, the loader
  // reschedules the reload until the LastModificationTime() is a couple of
  // seconds in the past. This mitigates the problem of reading files that are
  // currently being written to, and whose contents are incomplete.
  // A reload is posted periodically, if it hasn't been triggered recently. This
  // makes sure the policies are reloaded if the update events aren't triggered.
  void Reload(bool force);

  const scoped_refptr<SchemaMap>& schema_map() const { return schema_map_; }

 private:
  // Allow AsyncPolicyProvider to call Init().
  friend class AsyncPolicyProvider;

  typedef base::RepeatingCallback<void(std::unique_ptr<PolicyBundle>)>
      UpdateCallback;

  // Used by the AsyncPolicyProvider to install the |update_callback_|.
  // Invoked on the background thread.
  void Init(const UpdateCallback& update_callback);

  // Used by the AsyncPolicyProvider to reload with an updated SchemaMap.
  void RefreshPolicies(scoped_refptr<SchemaMap> schema_map);

  // Cancels any pending periodic reload and posts one |delay| time units from
  // now.
  void ScheduleNextReload(base::TimeDelta delay);

  // Checks if the underlying files haven't changed recently, by checking the
  // LastModificationTime(). |delay| is updated with a suggested time to wait
  // before retrying when this returns false.
  bool IsSafeToReload(const base::Time& now, base::TimeDelta* delay);

  // Task runner for running background jobs.
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Callback for updates, passed in Init().
  UpdateCallback update_callback_;

  // Records last known modification timestamp.
  base::Time last_modification_time_;

  // The wall clock time at which the last modification timestamp was
  // recorded.  It's better to not assume the file notification time and the
  // wall clock times come from the same source, just in case there is some
  // non-local filesystem involved.
  base::Time last_modification_clock_;

  // The current policy schemas that this provider should load.
  scoped_refptr<SchemaMap> schema_map_;

  // Used to get WeakPtrs for the periodic reload task.
  base::WeakPtrFactory<AsyncPolicyLoader> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AsyncPolicyLoader);
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_ASYNC_POLICY_LOADER_H_
