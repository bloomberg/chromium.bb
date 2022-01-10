// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_CLEAN_EXIT_BEACON_H_
#define COMPONENTS_METRICS_CLEAN_EXIT_BEACON_H_

#include <string>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/version_info/channel.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class PrefRegistrySimple;
class PrefService;

namespace metrics {

// Captures all possible beacon value permutations for two distinct beacons.
// Exposed for testing.
enum class CleanExitBeaconConsistency {
  kCleanClean = 0,
  kCleanDirty = 1,
  kCleanMissing = 2,
  kDirtyClean = 3,
  kDirtyDirty = 4,
  kDirtyMissing = 5,
  kMissingClean = 6,
  kMissingDirty = 7,
  kMissingMissing = 8,
  kMaxValue = kMissingMissing,
};

// Reads and updates a beacon used to detect whether the previous browser
// process exited cleanly.
class CleanExitBeacon {
 public:
  // Instantiates a CleanExitBeacon whose value is stored in |local_state|'s
  // kStabilityExitedCleanly pref. |local_state| must be fully initialized.
  //
  // On Windows, |backup_registry_key| stores a backup of the beacon to verify
  // that the pref's value corresponds to the registry's. |backup_registry_key|
  // is ignored on other platforms, but iOS has a similar verification
  // mechanism embedded inside CleanExitBeacon.
  //
  // |user_data_dir| is the path to the client's user data directory. If empty,
  // a separate file will not be used for Variations Safe Mode prefs.
  //
  // TODO(crbug/1241702): Remove |channel| at the end of the Extended Variations
  // Safe Mode experiment. |channel| is used to enable the experiment on only
  // certain channels.
  CleanExitBeacon(const std::wstring& backup_registry_key,
                  const base::FilePath& user_data_dir,
                  PrefService* local_state,
                  version_info::Channel channel);

  virtual ~CleanExitBeacon() = default;

  // Not copyable or movable.
  CleanExitBeacon(const CleanExitBeacon&) = delete;
  CleanExitBeacon& operator=(const CleanExitBeacon&) = delete;

  // Initializes the CleanExitBeacon. This includes the following tasks:
  // 1. Determining if the last session exited cleanly,
  // 2. Incrementing the crash streak, if necessary, and
  // 3. Emitting some metrics.
  void Initialize();

  // Returns the original value of the beacon.
  bool exited_cleanly() const { return did_previous_session_exit_cleanly_; }

  // Returns the original value of the last live timestamp.
  base::Time browser_last_live_timestamp() const {
    return initial_browser_last_live_timestamp_;
  }

  // Sets the beacon value to |exited_cleanly| and updates the last live
  // timestamp. If |write_synchronously| is true, then the beacon value is
  // written to disk synchronously. If false, a write is scheduled, and for
  // clients in the Extended Variations Safe Mode experiment, a synchronous
  // write is done, too.
  //
  // Note: |write_synchronously| should be true only for some clients in the
  // Extended Variations Safe Mode experiment.
  void WriteBeaconValue(bool exited_cleanly, bool write_synchronously = false);

  // Updates the last live timestamp.
  void UpdateLastLiveTimestamp();

  // Registers local state prefs used by this class.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Updates both Local State and NSUserDefaults beacon values.
  static void SetStabilityExitedCleanlyForTesting(PrefService* local_state,
                                                  bool exited_cleanly);

  // Resets both Local State and NSUserDefaults beacon values.
  static void ResetStabilityExitedCleanlyForTesting(PrefService* local_state);

  // CHECKs that Chrome exited cleanly.
  static void EnsureCleanShutdown(PrefService* local_state);

#if defined(OS_IOS)
  // Sets the NSUserDefaults beacon value.
  static void SetUserDefaultsBeacon(bool exited_cleanly);

  // Checks user default value of kUseUserDefaultsForExitedCleanlyBeacon.
  // Because variations are not initialized early in startup, pair a user
  // defaults value with the variations config.
  static bool ShouldUseUserDefaultsBeacon();

  // Syncs feature kUseUserDefaultsForExitedCleanlyBeacon to NSUserDefaults
  // kUserDefaultsFeatureFlagForExitedCleanlyBeacon.
  static void SyncUseUserDefaultsBeacon();
#endif  // defined(OS_IOS)

  // Prevents a test browser from performing two clean shutdown steps. First, it
  // prevents the beacon value from being updated after this function is called.
  // This prevents the the test browser from signaling that Chrome is shutting
  // down cleanly. Second, it makes EnsureCleanShutdown() a no-op.
  static void SkipCleanShutdownStepsForTesting();

 private:
  // Returns true if the previous session exited cleanly. Either Local State
  // or |beacon_file_contents| is used to get this information. Which is used
  // depends on the client's Extended Variations Safe Mode experiment group in
  // the previous session.
  // TODO(crbug/1241702): Update this comment when experimentation is over.
  bool DidPreviousSessionExitCleanly(base::Value* beacon_file_contents);

  // Writes |exited_cleanly| and the crash streak to the file located at
  // |beacon_file_path_|.
  void WriteBeaconFile(bool exited_cleanly) const;

#if defined(OS_WIN) || defined(OS_IOS)
  // Returns whether Chrome exited cleanly in the previous session according to
  // the platform-specific beacon (the registry for Windows or NSUserDefaults
  // for iOS). Returns absl::nullopt if the platform-specific location does not
  // have beacon info.
  absl::optional<bool> ExitedCleanly();
#endif  // defined(OS_WIN) || defined(OS_IOS)

#if defined(OS_IOS)
  // Returns true if the NSUserDefaults beacon value is set.
  static bool HasUserDefaultsBeacon();

  // Returns the NSUserDefaults beacon value.
  static bool GetUserDefaultsBeacon();

  // Clears the NSUserDefaults beacon value.
  static void ResetUserDefaultsBeacon();
#endif  // defined(OS_IOS)

  // Indicates whether the CleanExitBeacon has been initialized.
  bool initialized_ = false;

  // Stores a backup of the beacon. Windows only.
  const std::wstring backup_registry_key_;

  // Path to the client's user data directory. May be empty.
  const base::FilePath user_data_dir_;

  const raw_ptr<PrefService> local_state_;

  // This is the value of the last live timestamp from local state at the time
  // of construction. It is a timestamp from the previous browser session when
  // the browser was known to be alive.
  const base::Time initial_browser_last_live_timestamp_;

  // The client's channel, e.g. Canary. Used to help determine whether the
  // client should participate in the Extended Variations Safe Mode experiment.
  // TODO(crbug/1241702): Remove at the end of the experiment.
  const version_info::Channel channel_;

  bool did_previous_session_exit_cleanly_ = false;

  // Where the clean exit beacon and the variations crash streak may be stored
  // for some clients in the Extended Variations Safe Mode experiment.
  base::FilePath beacon_file_path_;
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_CLEAN_EXIT_BEACON_H_
