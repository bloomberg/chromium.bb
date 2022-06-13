// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_VARIATIONS_TEST_UTILS_H_
#define COMPONENTS_VARIATIONS_VARIATIONS_TEST_UTILS_H_

#include <set>
#include <string>

#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/field_trial.h"
#include "components/variations/variations_associated_data.h"

class PrefService;

namespace variations {

// Packages signed variations seed data into a tuple for use with
// WriteSeedData(). This allows for encapsulated seed information to be created
// below for generic test seeds as well as seeds which cause crashes.
struct SignedSeedData {
  base::span<const char*> study_names;  // Names of all studies in the seed.
  const char* base64_uncompressed_data;
  const char* base64_compressed_data;
  const char* base64_signature;

  // Out-of-line constructor/destructor/copy/move required for 'complex'
  // classes.
  SignedSeedData(base::span<const char*> in_study_names,
                 const char* in_base64_uncompressed_data,
                 const char* in_base64_compressed_data,
                 const char* in_base64_signature);
  ~SignedSeedData();
  SignedSeedData(const SignedSeedData&);
  SignedSeedData(SignedSeedData&&);
  SignedSeedData& operator=(const SignedSeedData&);
  SignedSeedData& operator=(SignedSeedData&&);
};

// Packages variations seed pref keys into a tuple for use with StoreSeedInfo().
// This allow easily writing signed seed data into either the safe seed or
// regular seed locations in Local State.
struct SignedSeedPrefKeys {
  const char* base64_compressed_data_key;
  const char* base64_signature_key;
};

// The test seed data is associated with a VariationsSeed with one study,
// "UMA-Uniformity-Trial-10-Percent", and ten equally weighted groups: "default"
// and "group_01" through "group_09". The study is not associated with channels,
// platforms, or features.
extern const SignedSeedData kTestSeedData;

// The crashing seed data contains a CrashingStudy that enables the
// variations::kForceFieldTrialSetupCrashForTesting feature at 100% on all
// platforms and channels.
extern const SignedSeedData kCrashingSeedData;

// The pref keys used to store safe signed variations seed data.
extern const SignedSeedPrefKeys kSafeSeedPrefKeys;

// The pref keys used to store regular signed variations seed data.
extern const SignedSeedPrefKeys kRegularSeedPrefKeys;

// Disables the use of the field trial testing config to exercise
// VariationsFieldTrialCreator::CreateTrialsFromSeed().
void DisableTestingConfig();

// Decodes the variations header and extracts the variation ids.
bool ExtractVariationIds(const std::string& variations,
                         std::set<VariationID>* variation_ids,
                         std::set<VariationID>* trigger_ids);

// Creates FieldTrial from given |key| and |id|.
scoped_refptr<base::FieldTrial> CreateTrialAndAssociateId(
    const std::string& trial_name,
    const std::string& default_group_name,
    IDCollectionKey key,
    VariationID id);

// Sets up the extended safe mode experiment such that |group_name| is the
// active group. Returns the numeric value that denotes the active group.
int SetUpExtendedSafeModeExperiment(const std::string& group_name);

// Simulates a crash by setting the clean exit pref to false and disabling
// the steps to update the pref on clean shutdown.
void SimulateCrash(PrefService* local_state);

// Writes |seed_info| into |local_state| using the given seed |pref_keys|.
void WriteSeedData(PrefService* local_state,
                   const SignedSeedData& seed_data,
                   const SignedSeedPrefKeys& pref_keys);

// Returns true if all of the study_names listed in |seed_data| exist in the
// (global) field trial list.
bool FieldTrialListHasAllStudiesFrom(const SignedSeedData& seed_data);

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_VARIATIONS_TEST_UTILS_H_
