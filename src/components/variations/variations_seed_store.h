// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_VARIATIONS_SEED_STORE_H_
#define COMPONENTS_VARIATIONS_VARIATIONS_SEED_STORE_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/variations/metrics.h"
#include "components/variations/proto/variations_seed.pb.h"
#include "components/variations/seed_response.h"

class PrefService;
class PrefRegistrySimple;

namespace variations {

struct ClientFilterableState;
class VariationsSeed;

// A seed that has passed validation.
struct ValidatedSeed {
  // The serialized VariationsSeed bytes.
  std::string bytes;
  // A cryptographic signature on the seed_data.
  std::string base64_seed_signature;
  // The seed data parsed as a proto.
  VariationsSeed parsed;
};

// VariationsSeedStore is a helper class for reading and writing the variations
// seed from Local State.
class COMPONENT_EXPORT(VARIATIONS) VariationsSeedStore {
 public:
  // Standard constructor. Enables signature verification.
  explicit VariationsSeedStore(PrefService* local_state);
  // |initial_seed| may be null. If not null, then it will be stored in this
  // seed store. This is used by Android Chrome to supply the first run seed,
  // and by Android WebView to supply the seed on every run.
  // |signature_verification_enabled| can be used in unit tests to disable
  // signature checks on the seed. If |use_first_run_prefs| is true (default),
  // then this VariationsSeedStore may modify the Java SharedPreferences ("first
  // run prefs") which are set during first run; otherwise this will not access
  // SharedPreferences at all.
  VariationsSeedStore(PrefService* local_state,
                      std::unique_ptr<SeedResponse> initial_seed,
                      bool signature_verification_enabled,
                      bool use_first_run_prefs = true);

  VariationsSeedStore(const VariationsSeedStore&) = delete;
  VariationsSeedStore& operator=(const VariationsSeedStore&) = delete;

  virtual ~VariationsSeedStore();

  // Loads the variations seed data from local state into |seed|, as well as the
  // raw pref values into |seed_data| and |base64_signature|. If there is a
  // problem with loading, clears the seed pref value and returns false. If
  // successful, fills the the outparams with the loaded data and returns true.
  // Virtual for testing.
  virtual bool LoadSeed(VariationsSeed* seed,
                        std::string* seed_data,
                        std::string* base64_seed_signature) WARN_UNUSED_RESULT;

  // Stores the given seed |data| (serialized protobuf) to local state, along
  // with a base64-encoded digital signature for seed and the date when it was
  // fetched. If |is_gzip_compressed| is true, treats |data| as being gzip
  // compressed and decompresses it before any other processing.
  // If |is_delta_compressed| is true, treats |data| as being delta
  // compressed and attempts to decode it first using the store's seed data.
  // The actual seed data will be base64 encoded for storage. If the string
  // is invalid, the existing prefs are untouched and false is returned.
  // Additionally, stores the |country_code| that was received with the seed in
  // a separate pref. On success and if |parsed_seed| is not NULL, |parsed_seed|
  // will be filled with the de-serialized decoded protobuf.
  bool StoreSeedData(const std::string& data,
                     const std::string& base64_seed_signature,
                     const std::string& country_code,
                     const base::Time& date_fetched,
                     bool is_delta_compressed,
                     bool is_gzip_compressed,
                     VariationsSeed* parsed_seed) WARN_UNUSED_RESULT;

  // Loads the safe variations seed data from local state into |seed| and
  // updates any relevant fields in |client_state|. Returns
  // LoadSeedResult::kSuccess iff the safe seed was read successfully from
  // prefs. If the safe seed could not be loaded, it is guaranteed that no
  // fields in |client_state| are modified.
  //
  // Side effect: Upon failing to read or validate the safe seed, clears all
  // of the safe seed pref values.
  //
  // Virtual for testing.
  virtual LoadSeedResult LoadSafeSeed(VariationsSeed* seed,
                                      ClientFilterableState* client_state)
      WARN_UNUSED_RESULT;

  // Stores the given |seed_data| (a serialized protobuf) to local state as a
  // safe seed, along with a base64-encoded digital signature for seed and any
  // additional client metadata relevant to the safe seed. Returns true on
  // success or false on failure; no prefs are updated in case of failure.
  // Virtual for testing.
  virtual bool StoreSafeSeed(const std::string& seed_data,
                             const std::string& base64_seed_signature,
                             int seed_milestone,
                             const ClientFilterableState& client_state,
                             base::Time seed_fetch_time);

  // Loads the last fetch time (for the latest seed) that was persisted to the
  // store.
  base::Time GetLastFetchTime() const;

  // Returns the time at which the safe seed was fetched.
  base::Time GetSafeSeedFetchTime() const;

  // Records |fetch_time| as the last time at which a seed was fetched
  // successfully. Also updates the safe seed's fetch time if the latest and
  // safe seeds are identical.
  void RecordLastFetchTime(base::Time fetch_time);

  // Updates |kVariationsSeedDate| and logs when previous date was from a
  // different day.
  void UpdateSeedDateAndLogDayChange(const base::Time& server_date_fetched);

  // Returns the serial number of the most recently received seed, or an empty
  // string if there is no seed (or if it could not be read).
  // Side-effect: If there is a failure while attempting to read the latest seed
  // from prefs, clears the prefs associated with the seed.
  // Efficiency note: If code will eventually need to load the latest seed, it's
  // more efficient to call LoadSeed() prior to calling this method.
  const std::string& GetLatestSerialNumber();

  // Registers Local State prefs used by this class.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  PrefService* local_state() { return local_state_; }
  const PrefService* local_state() const { return local_state_; }

  static VerifySignatureResult VerifySeedSignatureForTesting(
      const std::string& seed_bytes,
      const std::string& base64_seed_signature);

 private:
  FRIEND_TEST_ALL_PREFIXES(VariationsSeedStoreTest, VerifySeedSignature);
  FRIEND_TEST_ALL_PREFIXES(VariationsSeedStoreTest, ApplyDeltaPatch);

  // The seed store contains two distinct seeds:
  //   (1) The most recently fetched, or "latest", seed; and
  //   (2) A "safe" seed, which has been observed to keep Chrome in a basically
  //       functional state. In particular, a safe seed is one that allows
  //       Chrome to receive new seed updates from the server.
  // Note that it's possible for both seeds to be empty, and it's possible for
  // the two seeds to be identical in their contents.
  enum class SeedType {
    LATEST,
    SAFE,
  };

  // Clears all prefs related to variations seed storage for the specified seed
  // type.
  void ClearPrefs(SeedType seed_type);

#if defined(OS_ANDROID)
  // Imports the variations seed from the Java side. Logs UMA on failure.
  // Android Chrome uses this on first run; WebView uses this on every startup.
  // In Chrome's case, it's important to set the first run seed as soon as
  // possible, because some clients query the seed store prefs directly rather
  // than accessing them via the seed store API: https://crbug.com/829527
  void ImportInitialSeed(std::unique_ptr<SeedResponse> initial_seed);
#endif  // OS_ANDROID

  // Loads the variations seed data from local state into |seed|, as well as the
  // raw pref values into |seed_data| and |base64_signature|. Loads either the
  // safe seed or the latest seed, according to the |seed_type|. Returns whether
  // loading the seed was successful.
  // Side-effect: Upon any failure to read or validate the safe seed, clears all
  // of the pref values for the seed. This occurs iff the method returns false.
  LoadSeedResult LoadSeedImpl(SeedType seed_type,
                              VariationsSeed* seed,
                              std::string* seed_data,
                              std::string* base64_seed_signature)
      WARN_UNUSED_RESULT;

  // Reads the variations seed data from prefs into |seed_data|, and returns the
  // result of the load. The value stored into |seed_data| should only be used
  // if the result is SUCCESS. Reads either the latest or the safe seed,
  // according to the specified |seed_type|.
  // Side-effect: If the read fails, clears the prefs associated with the seed.
  LoadSeedResult ReadSeedData(SeedType seed_type,
                              std::string* seed_data) WARN_UNUSED_RESULT;

  // Resolves a |delta_bytes| against the latest seed.
  // Returns success or an error, populating |seed_bytes| on success.
  StoreSeedResult ResolveDelta(const std::string& delta_bytes,
                               std::string* seed_bytes) WARN_UNUSED_RESULT;

  // Resolves instance manipulations applied to received data.
  // Returns success or an error, populating |seed_bytes| on success.
  StoreSeedResult ResolveInstanceManipulations(const std::string& data,
                                               const InstanceManipulations& im,
                                               std::string* seed_bytes)
      WARN_UNUSED_RESULT;

  // Validates that |seed_bytes| parses and matches |base64_seed_signature|.
  // Signature checking may be disabled via |signature_verification_enabled_|.
  // |seed_type| indicates the source of the seed for logging purposes.
  // |result| must be non-null, and will be populated on success.
  // Returns success or some error value.
  StoreSeedResult ValidateSeedBytes(const std::string& seed_bytes,
                                    const std::string& base64_seed_signature,
                                    SeedType seed_type,
                                    ValidatedSeed* result) WARN_UNUSED_RESULT;

  // Gzip compresses and base64 encodes a validated seed.
  // Returns success or error and populates base64_seed_data on success.
  StoreSeedResult CompressSeedBytes(const ValidatedSeed& validated,
                                    std::string* base64_seed_data)
      WARN_UNUSED_RESULT;

  // Updates the latest seed with validated data.
  StoreSeedResult StoreValidatedSeed(const ValidatedSeed& seed,
                                     const std::string& country_code,
                                     const base::Time& date_fetched)
      WARN_UNUSED_RESULT;

  // Updates the safe seed with validated data.
  StoreSeedResult StoreValidatedSafeSeed(
      const ValidatedSeed& seed,
      int seed_milestone,
      const ClientFilterableState& client_state,
      base::Time seed_fetch_time) WARN_UNUSED_RESULT;

  // Applies a delta-compressed |patch| to |existing_data|, producing the result
  // in |output|. Returns whether the operation was successful.
  static bool ApplyDeltaPatch(const std::string& existing_data,
                              const std::string& patch,
                              std::string* output) WARN_UNUSED_RESULT;

  // The pref service used to persist the variations seed.
  raw_ptr<PrefService> local_state_;

  // Cached serial number from the most recently fetched variations seed.
  std::string latest_serial_number_;

  // Whether to validate signatures on the seed. Always on except in unit tests.
  const bool signature_verification_enabled_;

  // Whether this may read or write to Java "first run" SharedPreferences.
  const bool use_first_run_prefs_;
};

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_VARIATIONS_SEED_STORE_H_
