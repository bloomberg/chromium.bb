// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_KEY_BUNDLE_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_KEY_BUNDLE_H_

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/optional.h"
#include "base/values.h"
#include "chromeos/services/device_sync/cryptauth_key.h"
#include "chromeos/services/device_sync/proto/cryptauth_directive.pb.h"

namespace chromeos {

namespace device_sync {

// A group of related CryptAuthKeys, uniquely identified by their handles.
//
// No more than one key in the bundle can be active at a time, and only the
// active key should be used for encryption, signing, etc. The inactive keys are
// retained in case CryptAuth decides to activate them in a future via a
// SyncSingleKeyResponse::KeyAction.
//
// All key bundles used in Chrome OS are enumerated in the Name enum class. The
// corresponding name string that will be sent to CryptAuth in the
// SyncSingleKeysRequest::key_name protobuf field can be retrieved via
// KeyBundleNameEnumToString().
class CryptAuthKeyBundle {
 public:
  // Names which uniquely define a CryptAuthKeyBundle.
  // TODO(nohle): Add name for DeviceSync keys.
  enum class Name { kUserKeyPair, kLegacyMasterKey };
  static const base::flat_set<CryptAuthKeyBundle::Name>& AllNames();
  static std::string KeyBundleNameEnumToString(CryptAuthKeyBundle::Name name);
  static base::Optional<CryptAuthKeyBundle::Name> KeyBundleNameStringToEnum(
      const std::string& name);

  static base::Optional<CryptAuthKeyBundle> FromDictionary(
      const base::Value& dict);

  CryptAuthKeyBundle(Name name);

  CryptAuthKeyBundle(const CryptAuthKeyBundle&);

  ~CryptAuthKeyBundle();

  Name name() const { return name_; }

  const base::flat_map<std::string, CryptAuthKey>& handle_to_key_map() const {
    return handle_to_key_map_;
  }

  const base::Optional<cryptauthv2::KeyDirective>& key_directive() const {
    return key_directive_;
  }

  void set_key_directive(const cryptauthv2::KeyDirective& key_directive) {
    key_directive_ = key_directive;
  }

  // Returns nullptr if there is no active key.
  const CryptAuthKey* GetActiveKey() const;

  // If the key being added is active, all other keys in the bundle will be
  // deactivated. If the handle of the input key matches one in the bundle, the
  // existing key will be overwritten.
  // Note: All keys added to the bundle kUserKeyPair must have the handle
  // kCryptAuthFixedUserKeyPairHandle.
  void AddKey(const CryptAuthKey& key);

  // Activates the key corresponding to |handle| in the bundle and deactivates
  // the other keys.
  void SetActiveKey(const std::string& handle);

  // Sets all key statuses to kInactive.
  void DeactivateKeys();

  // Remove the key corresponding to |handle| from the bundle.
  void DeleteKey(const std::string& handle);

  base::Value AsDictionary() const;

  bool operator==(const CryptAuthKeyBundle& other) const;
  bool operator!=(const CryptAuthKeyBundle& other) const;

 private:
  Name name_;
  base::flat_map<std::string, CryptAuthKey> handle_to_key_map_;
  base::Optional<cryptauthv2::KeyDirective> key_directive_;
};

}  // namespace device_sync

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_KEY_BUNDLE_H_
