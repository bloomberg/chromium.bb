// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_ONC_ONC_MERGER_H_
#define CHROMEOS_NETWORK_ONC_ONC_MERGER_H_

#include <memory>

#include "base/component_export.h"

namespace base {
class DictionaryValue;
}

namespace chromeos {
namespace onc {

struct OncValueSignature;

// Merges the given |user_settings| and |shared_settings| settings with the
// given |user_policy| and |device_policy| settings. Each can be omitted by
// providing a NULL pointer. Each dictionary has to be part of a valid ONC
// dictionary. They don't have to describe top-level ONC but should refer to the
// same section in ONC. |user_settings| and |shared_settings| should not contain
// kRecommended fields. The resulting dictionary is valid ONC but may contain
// dispensable fields (e.g. in a network with type: "WiFi", the field "VPN" is
// dispensable) that can be removed by the caller using the ONC normalizer. ONC
// conformance of the arguments is not checked. Use ONC validator for that.
COMPONENT_EXPORT(CHROMEOS_NETWORK)
std::unique_ptr<base::DictionaryValue> MergeSettingsAndPoliciesToEffective(
    const base::DictionaryValue* user_policy,
    const base::DictionaryValue* device_policy,
    const base::DictionaryValue* user_settings,
    const base::DictionaryValue* shared_settings);

// Like MergeSettingsWithPoliciesToEffective but creates one dictionary in place
// of each field that exists in any of the argument dictionaries. Each of these
// dictionaries contains the onc::kAugmentations* fields (see onc_constants.h)
// for which a value is available. The onc::kAugmentationEffectiveSetting field
// contains the field name of the field containing the effective field that
// overrides all other values. Credentials from policies are not written to the
// result.
COMPONENT_EXPORT(CHROMEOS_NETWORK)
std::unique_ptr<base::DictionaryValue> MergeSettingsAndPoliciesToAugmented(
    const OncValueSignature& signature,
    const base::DictionaryValue* user_policy,
    const base::DictionaryValue* device_policy,
    const base::DictionaryValue* user_settings,
    const base::DictionaryValue* shared_settings,
    const base::DictionaryValue* active_settings);

}  // namespace onc
}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_ONC_ONC_MERGER_H_
