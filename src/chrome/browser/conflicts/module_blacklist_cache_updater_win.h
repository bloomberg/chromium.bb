// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONFLICTS_MODULE_BLACKLIST_CACHE_UPDATER_WIN_H_
#define CHROME_BROWSER_CONFLICTS_MODULE_BLACKLIST_CACHE_UPDATER_WIN_H_

#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/md5.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/conflicts/module_database_observer_win.h"
#include "chrome/browser/conflicts/proto/module_list.pb.h"
#include "chrome_elf/third_party_dlls/packed_list_format.h"

class ModuleListFilter;
struct CertificateInfo;

namespace base {
class SequencedTaskRunner;
}

// This class is responsible for maintaining the module blacklist cache, which
// is used by chrome_elf.dll to determine which module to block from loading
// into the process.
//
// Two things can happen that requires an update to the cache:
//   1. The Module Database becomes idle and this class identified new
//      blacklisted modules. They must be added to the cache.
//   2. The module load attempt log was drained and contained blocked entries.
//      Their timestamp in the cache must be updated.
//
// To coalesce these events and reduce the number of updates, a timer is started
// when the load attempt log is drained. Once expired, an update is triggered
// unless one was already done because of newly blacklisted modules.
//
//
// Additional implementation details about the module blacklist cache file:
//
// Because the file is written under the User Data directory, it is not possible
// for chrome_elf to find it by itself (see https://crbug.com/748949). So the
// path to the file is written into a registry key, which solves the problem by
// making the already expanded path available.
//
// A consequence of this solution is that in some circumstances, multiple
// browser process will race to write the path to their blacklist file into a
// single registry key because the same registry key is shared between all User
// Data directories.
//
// This means that a browser instance launch using one User Data directory can
// potentially use the cache from another User Data directory instead of their
// own.
//
// This is acceptable given that all the caches contains more or less the same
// information and are interchangeable. Also, updates to the cache file and to
// the registry are atomic, guaranteeing that chrome_elf always reads a valid
// blacklist.
class ModuleBlacklistCacheUpdater : public ModuleDatabaseObserver {
 public:
  // A decision that explains whether or not a module is to be blocked. This
  // decision is made with respect to the logic in the currently running
  // executable and the current version of the module list. It may not
  // correspond with the blocking decisions that were enforced by the blocking
  // logic in chrome_elf during early startup. Those decisions are cached from
  // the previous browser launch which may be with respect to (a) an entirely
  // different version of the browser and/or (b) an entirely different version
  // of the module list.
  //
  // Note that this enum is very similar to the ModuleWarningDecision in
  // IncompatibleApplicationsUpdater. This is done so that it is easier to keep
  // the 2 features separate, as they can be independently enabled/disabled.
  enum class ModuleBlockingDecision {
    // Explicitly defined as zero so it is the default value when a
    // ModuleBlockingDecision variable is value-initialized
    // (std::vector::resize()).
    kUnknown = 0,

    // Detailed reasons why modules will be allowed to load in subsequent
    // startups.

    // Input method editors are allowed.
    kAllowedIME,
    // Allowed because the certificate's subject of the module matches the
    // certificate's subject of the executable. The certificate is not
    // validated.
    kAllowedSameCertificate,
    // Allowed because the path of the executable is the parent of the path of
    // the module. Only used in non-official builds.
    kAllowedSameDirectory,
    // Allowed because it is signed by Microsoft. The certificate is not
    // validated.
    kAllowedMicrosoft,
    // Explicitly whitelisted by the Module List component.
    kAllowedWhitelisted,
    // New "allowed" reasons should be added here!

    // Unwanted, but allowed to load by the Module List component. This is
    // usually because blocking the module would cause more stability issues
    // than allowing it. If the IncompatibleApplicationsWarning feature is
    // enabled, this module will cause a warning if it can be tied back to an
    // installed application.
    kTolerated,

    // Detailed reasons why modules will not be allowed to load in subsequent
    // startups.

    // The module will be blocked because it is explicitly listed in the module
    // blacklist.
    kDisallowedExplicit,
    // The module will be implicitly blocked because it is not otherwise
    // whitelisted.
    kDisallowedImplicit,
    // New "disallowed" reasons should be added here!
  };

  struct ModuleBlockingState {
    // Whether or not the module was in the blacklist cache that existed at
    // startup.
    bool was_in_blacklist_cache;

    // Whether or not the module was ever actively blocked from loading during
    // this session.
    bool was_blocked;

    // Whether or not the module ever loaded during this session. Usually this
    // means that the module is currently loaded, but it's possible for DLLs to
    // subsequently be unloaded at runtime.
    bool was_loaded;

    // The current blocking decision. This is synced to the cache and will be
    // applied at the next startup.
    ModuleBlockingDecision blocking_decision;
  };

  struct CacheUpdateResult {
    base::MD5Digest old_md5_digest;
    base::MD5Digest new_md5_digest;
  };
  using OnCacheUpdatedCallback =
      base::RepeatingCallback<void(const CacheUpdateResult&)>;

  // The amount of time the timer will run before triggering an update of the
  // cache.
  static constexpr base::TimeDelta kUpdateTimerDuration =
      base::TimeDelta::FromMinutes(2);

  // Creates an instance of the updater. The callback will be invoked every time
  // the cache is updated.
  // The parameters must outlive the lifetime of this class.
  // The ModuleListFilter is taken by scoped_refptr since it will be used in a
  // background sequence.
  ModuleBlacklistCacheUpdater(
      ModuleDatabaseEventSource* module_database_event_source,
      const CertificateInfo& exe_certificate_info,
      scoped_refptr<ModuleListFilter> module_list_filter,
      const std::vector<third_party_dlls::PackedListModule>&
          initial_blacklisted_modules,
      OnCacheUpdatedCallback on_cache_updated_callback);
  ~ModuleBlacklistCacheUpdater() override;

  // Returns true if the blocking of third-party modules is enabled. The return
  // value will not change throughout the lifetime of the process.
  static bool IsThirdPartyModuleBlockingEnabled();

  // Returns the path to the module blacklist cache.
  static base::FilePath GetModuleBlacklistCachePath();

  // Deletes the module blacklist cache. This disables the blocking of third-
  // party modules for the next browser launch.
  static void DeleteModuleBlacklistCache();

  // ModuleDatabaseObserver:
  void OnNewModuleFound(const ModuleInfoKey& module_key,
                        const ModuleInfoData& module_data) override;
  void OnKnownModuleLoaded(const ModuleInfoKey& module_key,
                           const ModuleInfoData& module_data) override;
  void OnModuleDatabaseIdle() override;

  // Returns the blocking decision for a module.
  const ModuleBlockingState& GetModuleBlockingState(
      ModuleInfoKey module_key) const;

 private:
  // The state of the module with respect to the ModuleList.
  enum class ModuleListState {
    // The module is not in the module list at all.
    kUnlisted,
    // The module is in the module list and is explicitly whitelisted.
    kWhitelisted,
    // The module is in the module list and "blacklisted", but loading is
    // tolerated.
    kTolerated,
    // The module is explicitly blacklisted.
    kBlacklisted,
  };

  void OnTimerExpired();

  // Gets the state of a module with respect to the module list.
  ModuleListState DetermineModuleListState(const ModuleInfoKey& module_key,
                                           const ModuleInfoData& module_data);

  // Determines whether or not a module *should* be whitelisted or blacklisted
  // on the next startup. Returns a ModuleBlockingDecision.
  ModuleBlockingDecision DetermineModuleBlockingDecision(
      const ModuleInfoKey& module_key,
      const ModuleInfoData& module_data);

  // Posts the task to update the cache on |background_sequence_|.
  void StartModuleBlacklistCacheUpdate();

  // Invoked on the sequence that owns this instance when the cache is updated.
  void OnModuleBlacklistCacheUpdated(const CacheUpdateResult& result);

  ModuleDatabaseEventSource* const module_database_event_source_;

  const CertificateInfo& exe_certificate_info_;
  scoped_refptr<ModuleListFilter> module_list_filter_;
  const std::vector<third_party_dlls::PackedListModule>&
      initial_blacklisted_modules_;

  OnCacheUpdatedCallback on_cache_updated_callback_;

  // The sequence on which the module blacklist cache file is updated.
  scoped_refptr<base::SequencedTaskRunner> background_sequence_;

  // Temporarily holds newly blacklisted modules before they are added to the
  // module blacklist cache.
  std::vector<third_party_dlls::PackedListModule> newly_blacklisted_modules_;

  // Temporarily holds modules that were blocked from loading into the browser
  // until they are used to update the cache.
  std::vector<third_party_dlls::PackedListModule> blocked_modules_;

  // Ensures that the cache is updated when new blocked modules arrives even if
  // OnModuleDatabaseIdle() is never called again.
  base::OneShotTimer timer_;

  // Holds the blocking state for all known modules. The index is the module
  // id.
  std::vector<ModuleBlockingState> module_blocking_state_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<ModuleBlacklistCacheUpdater> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ModuleBlacklistCacheUpdater);
};

#endif  // CHROME_BROWSER_CONFLICTS_MODULE_BLACKLIST_CACHE_UPDATER_WIN_H_
