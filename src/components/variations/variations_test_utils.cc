// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/variations_test_utils.h"

#include "base/base64.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "components/metrics/clean_exit_beacon.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/variations/pref_names.h"
#include "components/variations/proto/client_variations.pb.h"
#include "components/variations/service/variations_safe_mode_constants.h"
#include "components/variations/variations_associated_data.h"
#include "components/variations/variations_switches.h"
#include "third_party/zlib/google/compression_utils.h"

namespace variations {
namespace {

const char* kTestSeed_StudyNames[] = {"UMA-Uniformity-Trial-10-Percent"};

const char kTestSeed_Base64UncompressedData[] =
    "CigxZDI5NDY0ZmIzZDc4ZmYxNTU2ZTViNTUxYzY0NDdjYmM3NGU1ZmQwEr0BCh9VTUEtVW5pZm"
    "9ybWl0eS1UcmlhbC0xMC1QZXJjZW50GICckqUFOAFCB2RlZmF1bHRKCwoHZGVmYXVsdBABSgwK"
    "CGdyb3VwXzAxEAFKDAoIZ3JvdXBfMDIQAUoMCghncm91cF8wMxABSgwKCGdyb3VwXzA0EAFKDA"
    "oIZ3JvdXBfMDUQAUoMCghncm91cF8wNhABSgwKCGdyb3VwXzA3EAFKDAoIZ3JvdXBfMDgQAUoM"
    "Cghncm91cF8wORAB";

const char kTestSeed_Base64CompressedData[] =
    "H4sIAAAAAAAAAOPSMEwxsjQxM0lLMk4xt0hLMzQ1NUs1TTI1NUw2MzExT05KNjdJNU1LMRDay8"
    "glH+rrqBual5mWX5SbWVKpG1KUmZija2igG5BalJyaVyLRMGfSUlYLRif2lNS0xNKcEi9uLhhT"
    "gNGLh4sjvSi/tCDewBCFZ4TCM0bhmaDwTFF4Zig8cxSeBQrPUoARAEVeJPrqAAAA";

const char kTestSeed_Base64Signature[] =
    "MEQCIDD1IVxjzWYncun+9IGzqYjZvqxxujQEayJULTlbTGA/AiAr0oVmEgVUQZBYq5VLOSvy96"
    "JkMYgzTkHPwbv7K/CmgA==";

const char* kCrashingSeed_StudyNames[] = {"CrashingStudy"};

const char kCrashingSeed_Base64UncompressedData[] =
    "CigzNWVkMmQ5ZTM1NGI0MTRiZWZkZjkzMGE3MzQwOTQwMTljMDE2MmYxEp4CCg1DcmFzaGluZ1"
    "N0dWR5OAFKOAoNRW5hYmxlZExhdW5jaBBkYiUKI0ZvcmNlRmllbGRUcmlhbFNldHVwQ3Jhc2hG"
    "b3JUZXN0aW5nSlcKLEZvcmNlZE9uX0ZvcmNlRmllbGRUcmlhbFNldHVwQ3Jhc2hGb3JUZXN0aW"
    "5nEABiJRojRm9yY2VGaWVsZFRyaWFsU2V0dXBDcmFzaEZvclRlc3RpbmdKWAotRm9yY2VkT2Zm"
    "X0ZvcmNlRmllbGRUcmlhbFNldHVwQ3Jhc2hGb3JUZXN0aW5nEABiJSIjRm9yY2VGaWVsZFRyaW"
    "FsU2V0dXBDcmFzaEZvclRlc3RpbmdSHhIEOTEuKiAAIAEgAiADKAQoBSgGKAAoASgCKAMoCSIt"
    "aGFzaC80YWE1NmExZGMzMGRmYzc2NzYxNTI0OGQ2ZmVlMjk4MzAxOThiMjc2";

const char kCrashingSeed_Base64CompressedData[] =
    "H4sIAAAAAAAAAI3QwUvDMBTH8babwgKDsaMHKZNBEKdJk6bJWbbDEAQ30JskeS+2UKp07cF/"
    "Zn+rZfgH9Py+73P4ESpyhAwMilw6yaXDAMEIZgshmZGMG8+4ygJfnhMyf27tqayar0PXw6+"
    "O95rMt411NcKL7RtfLsCtyd3uu/W4q7CGY1vZ+oBd/"
    "3P5HA5HPHUDsH8nD5cMXpvPEf0icuubUfAH2fzDIYyVV2Pkt9vl1PDH+zRK4zRJJ3RKr+"
    "g1jWhMEzqhs9WmHPonaW2uLAcvGARfqELxPJMaVEDMjBbDotplhfoDs9NLbnoBAAA=";

const char kCrashingSeed_Base64Signature[] =
    "MEQCIEn1+VsBfNA93dxzpk+BLhdO91kMQnofxfTK5Uo8vDi8AiAnTCFCIPgEGWNOKzuKfNWn6"
    "emB6pnGWjSTbI/pvfxHnw==";

}  // namespace

const SignedSeedData kTestSeedData{
    kTestSeed_StudyNames, kTestSeed_Base64UncompressedData,
    kTestSeed_Base64CompressedData, kTestSeed_Base64Signature};

const SignedSeedData kCrashingSeedData{
    kCrashingSeed_StudyNames, kCrashingSeed_Base64UncompressedData,
    kCrashingSeed_Base64CompressedData, kCrashingSeed_Base64Signature};

const SignedSeedPrefKeys kSafeSeedPrefKeys{prefs::kVariationsSafeCompressedSeed,
                                           prefs::kVariationsSafeSeedSignature};

const SignedSeedPrefKeys kRegularSeedPrefKeys{prefs::kVariationsCompressedSeed,
                                              prefs::kVariationsSeedSignature};

SignedSeedData::SignedSeedData(base::span<const char*> in_study_names,
                               const char* in_base64_uncompressed_data,
                               const char* in_base64_compressed_data,
                               const char* in_base64_signature)
    : study_names(std::move(in_study_names)),
      base64_uncompressed_data(in_base64_uncompressed_data),
      base64_compressed_data(in_base64_compressed_data),
      base64_signature(in_base64_signature) {}

SignedSeedData::~SignedSeedData() = default;

SignedSeedData::SignedSeedData(const SignedSeedData&) = default;
SignedSeedData::SignedSeedData(SignedSeedData&&) = default;
SignedSeedData& SignedSeedData::operator=(const SignedSeedData&) = default;
SignedSeedData& SignedSeedData::operator=(SignedSeedData&&) = default;

void DisableTestingConfig() {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableFieldTrialTestingConfig);
}

bool ExtractVariationIds(const std::string& variations,
                         std::set<VariationID>* variation_ids,
                         std::set<VariationID>* trigger_ids) {
  std::string serialized_proto;
  if (!base::Base64Decode(variations, &serialized_proto))
    return false;
  ClientVariations proto;
  if (!proto.ParseFromString(serialized_proto))
    return false;
  for (int i = 0; i < proto.variation_id_size(); ++i)
    variation_ids->insert(proto.variation_id(i));
  for (int i = 0; i < proto.trigger_variation_id_size(); ++i)
    trigger_ids->insert(proto.trigger_variation_id(i));
  return true;
}

scoped_refptr<base::FieldTrial> CreateTrialAndAssociateId(
    const std::string& trial_name,
    const std::string& default_group_name,
    IDCollectionKey key,
    VariationID id) {
  AssociateGoogleVariationID(key, trial_name, default_group_name, id);
  scoped_refptr<base::FieldTrial> trial(
      base::FieldTrialList::CreateFieldTrial(trial_name, default_group_name));
  DCHECK(trial);

  if (trial) {
    // Ensure the trial is registered under the correct key so we can look it
    // up.
    trial->group();
  }

  return trial;
}

int SetUpExtendedSafeModeExperiment(const std::string& group_name) {
  int default_group;
  scoped_refptr<base::FieldTrial> trial(
      base::FieldTrialList::FactoryGetFieldTrial(
          kExtendedSafeModeTrial, 100, kDefaultGroup,
          base::FieldTrial::ONE_TIME_RANDOMIZED, &default_group));

  int active_group = default_group;
  if (group_name != kDefaultGroup)
    active_group = trial->AppendGroup(group_name, 100);
  trial->SetForced();
  return active_group;
}

void SimulateCrash(PrefService* local_state) {
  local_state->SetBoolean(metrics::prefs::kStabilityExitedCleanly, false);
  metrics::CleanExitBeacon::SkipCleanShutdownStepsForTesting();
}

void WriteSeedData(PrefService* local_state,
                   const SignedSeedData& seed_data,
                   const SignedSeedPrefKeys& pref_keys) {
  local_state->SetString(pref_keys.base64_compressed_data_key,
                         seed_data.base64_compressed_data);
  local_state->SetString(pref_keys.base64_signature_key,
                         seed_data.base64_signature);
  local_state->CommitPendingWrite();
}

bool FieldTrialListHasAllStudiesFrom(const SignedSeedData& seed_data) {
  return base::ranges::all_of(seed_data.study_names, [](const char* study) {
    return base::FieldTrialList::TrialExists(study);
  });
}

}  // namespace variations
