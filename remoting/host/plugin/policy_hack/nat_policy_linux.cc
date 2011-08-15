// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Most of this code is copied from various classes in
// src/chrome/browser/policy. In partiuclar, look at
//
//   asynchronous_policy_loader.{h,cc}
//   file_based_policy_loader.{h,cc}
//   config_dir_policy_provider.{h,cc}
//
// This is a reduction of the functionality in those classes.

#include <set>

#include "remoting/host/plugin/policy_hack/nat_policy.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/files/file_path_watcher.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop_proxy.h"
#include "base/scoped_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/time.h"
#include "base/values.h"
#include "content/common/json_value_serializer.h"

namespace remoting {
namespace policy_hack {

namespace {

#if defined(GOOGLE_CHROME_BUILD)
const FilePath::CharType kPolicyDir[] =
    FILE_PATH_LITERAL("/etc/opt/chrome/policies/managed");
#else
const FilePath::CharType kPolicyDir[] =
    FILE_PATH_LITERAL("/etc/chromium/policies/managed");
#endif

// The time interval for rechecking policy. This is our fallback in case the
// delegate never reports a change to the ReloadObserver.
const int kFallbackReloadDelayMinutes = 15;

// Amount of time we wait for the files on disk to settle before trying to load
// them. This alleviates the problem of reading partially written files and
// makes it possible to batch quasi-simultaneous changes.
const int kSettleIntervalSeconds = 5;

}  // namespace

class NatPolicyLinux : public NatPolicy {
 public:
  NatPolicyLinux(base::MessageLoopProxy* message_loop_proxy,
                 const FilePath& config_dir)
      : message_loop_proxy_(message_loop_proxy),
        config_dir_(config_dir),
        current_nat_enabled_state_(false),
        first_state_published_(false),
        ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
    // Detach the factory because we ensure that only the |message_loop_proxy_|
    // thread ever calls methods on this. Also, the API contract of having
    // to call StopWatching() (which signals completion) after StopWatching()
    // before this object can be destructed ensures there are no users of
    // this object before it is destructed.
    weak_factory_.DetachFromThread();
  }

  virtual ~NatPolicyLinux() {}

  virtual void StartWatching(const NatEnabledCallback& nat_enabled_cb)
      OVERRIDE {
    if (!message_loop_proxy_->BelongsToCurrentThread()) {
      message_loop_proxy_->PostTask(FROM_HERE,
                                    base::Bind(&NatPolicyLinux::StartWatching,
                                               base::Unretained(this),
                                               nat_enabled_cb));
      return;
    }

    nat_enabled_cb_ = nat_enabled_cb;
    watcher_.reset(new base::files::FilePathWatcher());

    if (!config_dir_.empty() &&
        !watcher_->Watch(
            config_dir_,
            new FilePathWatcherDelegate(weak_factory_.GetWeakPtr()))) {
      OnFilePathError(config_dir_);
    }

    // There might have been changes to the directory in the time between
    // construction of the loader and initialization of the watcher. Call reload
    // to detect if that is the case.
    Reload();

    ScheduleFallbackReloadTask();
  }

  virtual void StopWatching(base::WaitableEvent* done) OVERRIDE {
    if (!message_loop_proxy_->BelongsToCurrentThread()) {
      message_loop_proxy_->PostTask(FROM_HERE,
                                    base::Bind(&NatPolicyLinux::StopWatching,
                                               base::Unretained(this), done));
      return;
    }

    // Cancel any inflight requests.
    weak_factory_.InvalidateWeakPtrs();
    watcher_.reset();
    nat_enabled_cb_.Reset();

    done->Signal();
  }

  // Called by FilePathWatcherDelegate.
  virtual void OnFilePathError(const FilePath& path) {
    LOG(ERROR) << "NatPolicyLinux on " << path.value()
               << " failed.";
  }

  // Called by FilePathWatcherDelegate.
  virtual void OnFilePathChanged(const FilePath& path) {
    DCHECK(message_loop_proxy_->BelongsToCurrentThread());

    Reload();
  }

 private:
  // Needed to avoid refcounting NatPolicyLinux.
  class FilePathWatcherDelegate :
    public base::files::FilePathWatcher::Delegate {
   public:
    FilePathWatcherDelegate(base::WeakPtr<NatPolicyLinux> policy_watcher)
        : policy_watcher_(policy_watcher) {
    }

    virtual void OnFilePathError(const FilePath& path) {
      if (policy_watcher_) {
        policy_watcher_->OnFilePathError(path);
      }
    }

    virtual void OnFilePathChanged(const FilePath& path) {
      if (policy_watcher_) {
        policy_watcher_->OnFilePathChanged(path);
      }
    }

   private:
    base::WeakPtr<NatPolicyLinux> policy_watcher_;
  };

  void ScheduleFallbackReloadTask() {
    DCHECK(message_loop_proxy_->BelongsToCurrentThread());
    ScheduleReloadTask(
        base::TimeDelta::FromMinutes(kFallbackReloadDelayMinutes));
  }

  void ScheduleReloadTask(const base::TimeDelta& delay) {
    DCHECK(message_loop_proxy_->BelongsToCurrentThread());
    message_loop_proxy_->PostDelayedTask(
        FROM_HERE,
        base::Bind(&NatPolicyLinux::Reload, weak_factory_.GetWeakPtr()),
        delay.InMilliseconds());
  }

  base::Time GetLastModification() {
    DCHECK(message_loop_proxy_->BelongsToCurrentThread());
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

  // Caller owns the value.
  DictionaryValue* Load() {
    DCHECK(message_loop_proxy_->BelongsToCurrentThread());
    // Enumerate the files and sort them lexicographically.
    std::set<FilePath> files;
    file_util::FileEnumerator file_enumerator(config_dir_, false,
                                              file_util::FileEnumerator::FILES);
    for (FilePath config_file_path = file_enumerator.Next();
         !config_file_path.empty(); config_file_path = file_enumerator.Next())
      files.insert(config_file_path);

    // Start with an empty dictionary and merge the files' contents.
    DictionaryValue* policy = new DictionaryValue;
    for (std::set<FilePath>::iterator config_file_iter = files.begin();
         config_file_iter != files.end(); ++config_file_iter) {
      JSONFileValueSerializer deserializer(*config_file_iter);
      int error_code = 0;
      std::string error_msg;
      scoped_ptr<Value> value(
          deserializer.Deserialize(&error_code, &error_msg));
      if (!value.get()) {
        LOG(WARNING) << "Failed to read configuration file "
                     << config_file_iter->value() << ": " << error_msg;
        continue;
      }
      if (!value->IsType(Value::TYPE_DICTIONARY)) {
        LOG(WARNING) << "Expected JSON dictionary in configuration file "
                     << config_file_iter->value();
        continue;
      }
      policy->MergeDictionary(static_cast<DictionaryValue*>(value.get()));
    }

    return policy;
  }

  void Reload() {
    DCHECK(message_loop_proxy_->BelongsToCurrentThread());
    // Check the directory time in order to see whether a reload is required.
    base::TimeDelta delay;
    base::Time now = base::Time::Now();
    if (!IsSafeToReloadPolicy(now, &delay)) {
      ScheduleReloadTask(delay);
      return;
    }

    // Load the policy definitions.
    scoped_ptr<DictionaryValue> new_policy(Load());

    // Check again in case the directory has changed while reading it.
    if (!IsSafeToReloadPolicy(now, &delay)) {
      ScheduleReloadTask(delay);
      return;
    }

    // Read out just the host firewall traversal policy.  Name of policy taken
    // from the generated policy/policy_constants.h file.
    bool new_nat_enabled_state = false;
    if (!new_policy->HasKey("RemoteAccessHostFirewallTraversal")) {
      // If unspecified, the default value of this policy is true.
      //
      // TODO(ajwong): Current default to false until we have policy
      // implemented and verified in all 3 platforms.
      new_nat_enabled_state = false;
    } else {
      // Otherwise, try to parse the value and only change from false if we get
      // a successful read.
      base::Value* value;
      if (new_policy->Get("RemoteAccessHostFirewallTraversal", &value) &&
          value->IsType(base::Value::TYPE_BOOLEAN)) {
        CHECK(value->GetAsBoolean(&new_nat_enabled_state));
      }
    }

    if (!first_state_published_ ||
        (new_nat_enabled_state != current_nat_enabled_state_)) {
      first_state_published_ = true;
      current_nat_enabled_state_ = new_nat_enabled_state;
      nat_enabled_cb_.Run(current_nat_enabled_state_);
    }

    ScheduleFallbackReloadTask();
  }

  bool IsSafeToReloadPolicy(const base::Time& now, base::TimeDelta* delay) {
    DCHECK(message_loop_proxy_->BelongsToCurrentThread());
    DCHECK(delay);
    const base::TimeDelta kSettleInterval =
        base::TimeDelta::FromSeconds(kSettleIntervalSeconds);

    base::Time last_modification = GetLastModification();
    if (last_modification.is_null())
      return true;

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
  // decouple the watcher's life cycle from the NatPolicyLinux. This decoupling
  // makes it possible to destroy the watcher before the loader's destructor is
  // called (e.g. during Stop), since |watcher_| internally holds a reference to
  // the loader and keeps it alive.
  scoped_ptr<base::files::FilePathWatcher> watcher_;

  // Records last known modification timestamp of |config_dir_|.
  base::Time last_modification_file_;

  // The wall clock time at which the last modification timestamp was
  // recorded.  It's better to not assume the file notification time and the
  // wall clock times come from the same source, just in case there is some
  // non-local filesystem involved.
  base::Time last_modification_clock_;

  scoped_refptr<base::MessageLoopProxy> message_loop_proxy_;
  const FilePath config_dir_;
  NatEnabledCallback nat_enabled_cb_;

  bool current_nat_enabled_state_;
  bool first_state_published_;

  // Allows us to cancel any inflight FileWatcher events or scheduled reloads.
  base::WeakPtrFactory<NatPolicyLinux> weak_factory_;
};

NatPolicy* NatPolicy::Create(base::MessageLoopProxy* message_loop_proxy) {
  FilePath policy_dir(kPolicyDir);
  return new NatPolicyLinux(message_loop_proxy, policy_dir);
}

}  // namespace policy_hack
}  // namespace remoting
