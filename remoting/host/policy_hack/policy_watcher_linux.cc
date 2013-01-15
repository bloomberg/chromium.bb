// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Most of this code is copied from various classes in
// src/chrome/browser/policy. In particular, look at
//
//   file_based_policy_loader.{h,cc}
//   config_dir_policy_provider.{h,cc}
//
// This is a reduction of the functionality in those classes.

#include <set>

#include "remoting/host/policy_hack/policy_watcher.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/files/file_path_watcher.h"
#include "base/json/json_file_value_serializer.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/time.h"
#include "base/values.h"

namespace remoting {
namespace policy_hack {

namespace {

const FilePath::CharType kPolicyDir[] =
  // Always read the Chrome policies (even on Chromium) so that policy
  // enforcement can't be bypassed by running Chromium.
  FILE_PATH_LITERAL("/etc/opt/chrome/policies/managed");

// Amount of time we wait for the files on disk to settle before trying to load
// them. This alleviates the problem of reading partially written files and
// makes it possible to batch quasi-simultaneous changes.
const int kSettleIntervalSeconds = 5;

}  // namespace

class PolicyWatcherLinux : public PolicyWatcher {
 public:
  PolicyWatcherLinux(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                     const FilePath& config_dir)
      : PolicyWatcher(task_runner),
        config_dir_(config_dir),
        ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
    // Detach the factory because we ensure that only the policy thread ever
    // calls methods on this. Also, the API contract of having to call
    // StopWatching() (which signals completion) after StartWatching()
    // before this object can be destructed ensures there are no users of
    // this object before it is destructed.
    weak_factory_.DetachFromThread();
  }

  virtual ~PolicyWatcherLinux() {}

 protected:
  virtual void StartWatchingInternal() OVERRIDE {
    DCHECK(OnPolicyWatcherThread());
    watcher_.reset(new base::FilePathWatcher());

    if (!config_dir_.empty() &&
        !watcher_->Watch(
            config_dir_, false,
            base::Bind(&PolicyWatcherLinux::OnFilePathChanged,
                       weak_factory_.GetWeakPtr()))) {
      OnFilePathChanged(config_dir_, true);
    }

    // There might have been changes to the directory in the time between
    // construction of the loader and initialization of the watcher. Call reload
    // to detect if that is the case.
    Reload();

    ScheduleFallbackReloadTask();
  }

  virtual void StopWatchingInternal() OVERRIDE {
    DCHECK(OnPolicyWatcherThread());
    // Cancel any inflight requests.
    watcher_.reset();
  }

 private:
  void OnFilePathChanged(const FilePath& path, bool error) {
    DCHECK(OnPolicyWatcherThread());

    if (!error)
      Reload();
    else
      LOG(ERROR) << "PolicyWatcherLinux on " << path.value() << " failed.";
  }

  base::Time GetLastModification() {
    DCHECK(OnPolicyWatcherThread());
    base::Time last_modification = base::Time();
    base::PlatformFileInfo file_info;

    // If the path does not exist or points to a directory, it's safe to load.
    if (!file_util::GetFileInfo(config_dir_, &file_info) ||
        !file_info.is_directory) {
      return last_modification;
    }

    // Enumerate the files and find the most recent modification timestamp.
    file_util::FileEnumerator file_enumerator(config_dir_,
                                              false,
                                              file_util::FileEnumerator::FILES);
    for (FilePath config_file = file_enumerator.Next();
         !config_file.empty();
         config_file = file_enumerator.Next()) {
      if (file_util::GetFileInfo(config_file, &file_info) &&
          !file_info.is_directory) {
        last_modification = std::max(last_modification,
                                     file_info.last_modified);
      }
    }

    return last_modification;
  }

  // Returns NULL if the policy dictionary couldn't be read.
  scoped_ptr<DictionaryValue> Load() {
    DCHECK(OnPolicyWatcherThread());
    // Enumerate the files and sort them lexicographically.
    std::set<FilePath> files;
    file_util::FileEnumerator file_enumerator(config_dir_, false,
                                              file_util::FileEnumerator::FILES);
    for (FilePath config_file_path = file_enumerator.Next();
         !config_file_path.empty(); config_file_path = file_enumerator.Next())
      files.insert(config_file_path);

    // Start with an empty dictionary and merge the files' contents.
    scoped_ptr<DictionaryValue> policy(new DictionaryValue());
    for (std::set<FilePath>::iterator config_file_iter = files.begin();
         config_file_iter != files.end(); ++config_file_iter) {
      JSONFileValueSerializer deserializer(*config_file_iter);
      deserializer.set_allow_trailing_comma(true);
      int error_code = 0;
      std::string error_msg;
      scoped_ptr<Value> value(
          deserializer.Deserialize(&error_code, &error_msg));
      if (!value.get()) {
        LOG(WARNING) << "Failed to read configuration file "
                     << config_file_iter->value() << ": " << error_msg;
        return scoped_ptr<DictionaryValue>();
      }
      if (!value->IsType(Value::TYPE_DICTIONARY)) {
        LOG(WARNING) << "Expected JSON dictionary in configuration file "
                     << config_file_iter->value();
        return scoped_ptr<DictionaryValue>();
      }
      policy->MergeDictionary(static_cast<DictionaryValue*>(value.get()));
    }

    return policy.Pass();
  }

  void Reload() {
    DCHECK(OnPolicyWatcherThread());
    // Check the directory time in order to see whether a reload is required.
    base::TimeDelta delay;
    base::Time now = base::Time::Now();
    if (!IsSafeToReloadPolicy(now, &delay)) {
      ScheduleReloadTask(delay);
      return;
    }

    // Check again in case the directory has changed while reading it.
    if (!IsSafeToReloadPolicy(now, &delay)) {
      ScheduleReloadTask(delay);
      return;
    }

    // Load the policy definitions.
    scoped_ptr<DictionaryValue> new_policy = Load();
    if (new_policy.get()) {
      UpdatePolicies(new_policy.get());
      ScheduleFallbackReloadTask();
    } else {
      // A failure to load policy definitions is probably temporary, so try
      // again soon.
      ScheduleReloadTask(base::TimeDelta::FromSeconds(kSettleIntervalSeconds));
    }
  }

  bool IsSafeToReloadPolicy(const base::Time& now, base::TimeDelta* delay) {
    DCHECK(OnPolicyWatcherThread());
    DCHECK(delay);
    const base::TimeDelta kSettleInterval =
        base::TimeDelta::FromSeconds(kSettleIntervalSeconds);

    base::Time last_modification = GetLastModification();
    if (last_modification.is_null())
      return true;

    if (last_modification_file_.is_null())
      last_modification_file_ = last_modification;

    // If there was a change since the last recorded modification, wait some
    // more.
    if (last_modification != last_modification_file_) {
      last_modification_file_ = last_modification;
      last_modification_clock_ = now;
      *delay = kSettleInterval;
      return false;
    }

    // Check whether the settle interval has elapsed.
    base::TimeDelta age = now - last_modification_clock_;
    if (age < kSettleInterval) {
      *delay = kSettleInterval - age;
      return false;
    }

    return true;
  }

  // Managed with a scoped_ptr rather than being declared as an inline member to
  // decouple the watcher's life cycle from the PolicyWatcherLinux. This
  // decoupling makes it possible to destroy the watcher before the loader's
  // destructor is called (e.g. during Stop), since |watcher_| internally holds
  // a reference to the loader and keeps it alive.
  scoped_ptr<base::FilePathWatcher> watcher_;

  // Records last known modification timestamp of |config_dir_|.
  base::Time last_modification_file_;

  // The wall clock time at which the last modification timestamp was
  // recorded.  It's better to not assume the file notification time and the
  // wall clock times come from the same source, just in case there is some
  // non-local filesystem involved.
  base::Time last_modification_clock_;

  const FilePath config_dir_;

  // Allows us to cancel any inflight FileWatcher events or scheduled reloads.
  base::WeakPtrFactory<PolicyWatcherLinux> weak_factory_;
};

PolicyWatcher* PolicyWatcher::Create(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  FilePath policy_dir(kPolicyDir);
  return new PolicyWatcherLinux(task_runner, policy_dir);
}

}  // namespace policy_hack
}  // namespace remoting
