// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CRYPTOHOME_CRYPTOHOME_UTIL_H_
#define CHROMEOS_CRYPTOHOME_CRYPTOHOME_UTIL_H_

#include <string>

#include "base/component_export.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/cryptohome/key.pb.h"
#include "chromeos/dbus/cryptohome/rpc.pb.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace cryptohome {

// Converts the key metadata in a RepeatedPtrField<cryptohome::KeyData> into
// cryptohome::KeyDefinition format. Note that this is temporarily extracted
// from GetKeyDataReplyToKeyDefinitions() to facilitate the transition from
// cryptohome_util.cc to userdataauth_util.cc.
std::vector<KeyDefinition> RepeatedKeyDataToKeyDefinitions(
    const google::protobuf::RepeatedPtrField<KeyData>& key_data);

// Creates an AuthorizationRequest from the given secret and label.
COMPONENT_EXPORT(CHROMEOS_CRYPTOHOME)
AuthorizationRequest CreateAuthorizationRequest(const std::string& label,
                                                const std::string& secret);

// Creates an AuthorizationRequest from the given key definition.
COMPONENT_EXPORT(CHROMEOS_CRYPTOHOME)
AuthorizationRequest CreateAuthorizationRequestFromKeyDef(
    const KeyDefinition& key_def);

// Converts the given KeyDefinition to a Key.
COMPONENT_EXPORT(CHROMEOS_CRYPTOHOME)
void KeyDefinitionToKey(const KeyDefinition& key_def, Key* key);

}  // namespace cryptohome

#endif  // CHROMEOS_CRYPTOHOME_CRYPTOHOME_UTIL_H_
