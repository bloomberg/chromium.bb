// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/smb_client/smb_persisted_share_registry.h"

#include <utility>

#include "base/base64.h"
#include "base/check.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace chromeos {
namespace smb_client {
namespace {

constexpr char kShareUrlKey[] = "share_url";
constexpr char kDisplayNameKey[] = "display_name";
constexpr char kUsernameKey[] = "username";
constexpr char kWorkgroupKey[] = "workgroup";
constexpr char kUseKerberosKey[] = "use_kerberos";
constexpr char kPasswordSaltKey[] = "password_salt";

base::Value ShareToDict(const SmbShareInfo& share) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringKey(kShareUrlKey, share.share_url().ToString());
  dict.SetStringKey(kDisplayNameKey, share.display_name());
  dict.SetBoolKey(kUseKerberosKey, share.use_kerberos());
  if (!share.username().empty()) {
    dict.SetStringKey(kUsernameKey, share.username());
  }
  if (!share.workgroup().empty()) {
    dict.SetStringKey(kWorkgroupKey, share.workgroup());
  }
  if (!share.password_salt().empty()) {
    // Blob base::Values can't be stored in prefs, so store as a base64 encoded
    // string.
    dict.SetStringKey(kPasswordSaltKey,
                      base::Base64Encode(share.password_salt()));
  }
  return dict;
}

std::string GetStringValue(const base::Value& dict, const std::string& key) {
  const std::string* value = dict.FindStringKey(key);
  if (!value) {
    return {};
  }
  return *value;
}

std::vector<uint8_t> GetEncodedBinaryValue(const base::Value& dict,
                                           const std::string& key) {
  const std::string* encoded_value = dict.FindStringKey(key);
  if (!encoded_value) {
    return {};
  }
  std::string decoded_value;
  if (!base::Base64Decode(*encoded_value, &decoded_value)) {
    LOG(ERROR) << "Unable to decode base64-encoded binary pref from key: "
               << key;
    return {};
  }
  return {decoded_value.begin(), decoded_value.end()};
}

base::Optional<SmbShareInfo> DictToShare(const base::Value& dict) {
  std::string share_url = GetStringValue(dict, kShareUrlKey);
  if (share_url.empty()) {
    return {};
  }

  SmbUrl url(share_url);
  DCHECK(url.IsValid());
  SmbShareInfo info(url, GetStringValue(dict, kDisplayNameKey),
                    GetStringValue(dict, kUsernameKey),
                    GetStringValue(dict, kWorkgroupKey),
                    dict.FindBoolKey(kUseKerberosKey).value_or(false),
                    GetEncodedBinaryValue(dict, kPasswordSaltKey));
  return base::make_optional(std::move(info));
}

}  // namespace

// static
void SmbPersistedShareRegistry::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(prefs::kNetworkFileSharesSavedShares);
}

SmbPersistedShareRegistry::SmbPersistedShareRegistry(Profile* profile)
    : profile_(profile) {}

void SmbPersistedShareRegistry::Save(const SmbShareInfo& share) {
  ListPrefUpdate pref(profile_->GetPrefs(),
                      prefs::kNetworkFileSharesSavedShares);

  base::Value::ListView share_list = pref->GetList();
  for (auto it = share_list.begin(); it != share_list.end(); ++it) {
    if (GetStringValue(*it, kShareUrlKey) == share.share_url().ToString()) {
      *it = ShareToDict(share);
      return;
    }
  }

  pref->Append(ShareToDict(share));
  return;
}

void SmbPersistedShareRegistry::Delete(const SmbUrl& share_url) {
  ListPrefUpdate pref(profile_->GetPrefs(),
                      prefs::kNetworkFileSharesSavedShares);

  base::Value::ListView share_list = pref->GetList();
  for (auto it = share_list.begin(); it != share_list.end(); ++it) {
    if (GetStringValue(*it, kShareUrlKey) == share_url.ToString()) {
      bool result = pref->EraseListIter(it);
      DCHECK(result);
      return;
    }
  }
}

base::Optional<SmbShareInfo> SmbPersistedShareRegistry::Get(
    const SmbUrl& share_url) const {
  const base::Value* pref =
      profile_->GetPrefs()->Get(prefs::kNetworkFileSharesSavedShares);
  if (!pref) {
    return {};
  }

  base::Value::ConstListView share_list = pref->GetList();
  for (auto it = share_list.begin(); it != share_list.end(); ++it) {
    if (GetStringValue(*it, kShareUrlKey) == share_url.ToString()) {
      return DictToShare(*it);
    }
  }
  return {};
}

std::vector<SmbShareInfo> SmbPersistedShareRegistry::GetAll() const {
  const base::Value* pref =
      profile_->GetPrefs()->Get(prefs::kNetworkFileSharesSavedShares);
  if (!pref) {
    return {};
  }

  std::vector<SmbShareInfo> shares;
  base::Value::ConstListView share_list = pref->GetList();
  for (auto it = share_list.begin(); it != share_list.end(); ++it) {
    base::Optional<SmbShareInfo> info = DictToShare(*it);
    if (!info) {
      continue;
    }
    shares.push_back(std::move(*info));
  }
  return shares;
}

}  // namespace smb_client
}  // namespace chromeos
