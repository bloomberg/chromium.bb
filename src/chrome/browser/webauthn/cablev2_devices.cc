// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/cablev2_devices.h"

#include <algorithm>
#include <array>
#include <string>
#include <vector>

#include "base/base64.h"
#include "base/feature_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/device_info_sync_service_factory.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/device_info_sync_service.h"
#include "components/sync_device_info/device_info_tracker.h"
#include "device/fido/cable/cable_discovery_data.h"
#include "device/fido/cable/v2_handshake.h"
#include "device/fido/features.h"

using device::cablev2::Pairing;

namespace cablev2 {

namespace {

template <size_t N>
bool CopyBytestring(std::array<uint8_t, N>* out, const std::string* value) {
  if (!value) {
    return false;
  }

  std::string bytes;
  if (!base::Base64Decode(*value, &bytes) || bytes.size() != N) {
    return false;
  }

  std::copy(bytes.begin(), bytes.end(), out->begin());
  return true;
}

bool CopyBytestring(std::vector<uint8_t>* out, const std::string* value) {
  if (!value) {
    return false;
  }

  std::string bytes;
  if (!base::Base64Decode(*value, &bytes)) {
    return false;
  }

  out->clear();
  out->insert(out->begin(), bytes.begin(), bytes.end());
  return true;
}

bool CopyString(std::string* out, const std::string* value) {
  if (!value) {
    return false;
  }
  *out = *value;
  return true;
}

const char kWebAuthnCablePairingsPrefName[] = "webauthn.cablev2_pairings";

// The `kWebAuthnCablePairingsPrefName` preference contains a list of dicts,
// where each dict has these keys:
const char kPairingPrefName[] = "name";
const char kPairingPrefContactId[] = "contact_id";
const char kPairingPrefTunnelServer[] = "tunnel_server";
const char kPairingPrefId[] = "id";
const char kPairingPrefSecret[] = "secret";
const char kPairingPrefPublicKey[] = "pub_key";
const char kPairingPrefTime[] = "time";

// NameForDisplay removes line-breaking characters from `raw_name` to ensure
// that the transport-selection UI isn't too badly broken by nonsense names.
static std::string NameForDisplay(base::StringPiece raw_name) {
  std::u16string unicode_name = base::UTF8ToUTF16(raw_name);
  base::StringPiece16 trimmed_name =
      base::TrimWhitespace(unicode_name, base::TRIM_ALL);
  // These are all the Unicode mandatory line-breaking characters
  // (https://www.unicode.org/reports/tr14/tr14-32.html#Properties).
  constexpr char16_t kLineTerminators[] = {0x0a, 0x0b, 0x0c, 0x0d, 0x85, 0x2028,
                                           0x2029,
                                           // Array must be NUL terminated.
                                           0};
  std::u16string nonbreaking_name;
  base::RemoveChars(trimmed_name, kLineTerminators, &nonbreaking_name);
  return base::UTF16ToUTF8(nonbreaking_name);
}

// DeletePairingByPublicKey erases any pairing with the given public key
// from `list`.
void DeletePairingByPublicKey(base::Value* list,
                              const std::string& public_key_base64) {
  list->EraseListValueIf([&public_key_base64](const auto& value) {
    if (!value.is_dict()) {
      return false;
    }
    const base::Value* pref_public_key = value.FindKey(kPairingPrefPublicKey);
    return pref_public_key && pref_public_key->is_string() &&
           pref_public_key->GetString() == public_key_base64;
  });
}

// PairingFromSyncedDevice extracts the caBLEv2 information from Sync's
// DeviceInfo (if any) into a caBLEv2 pairing. It may return nullptr.
std::unique_ptr<Pairing> PairingFromSyncedDevice(syncer::DeviceInfo* device,
                                                 const base::Time& now) {
  if (device->last_updated_timestamp() < now) {
    const base::TimeDelta age = now - device->last_updated_timestamp();
    if (age.InHours() > 24 * 14) {
      // Entries older than 14 days are dropped. If changing this, consider
      // updating `cablev2::sync::IDIsValid` too so that the mobile-side is
      // aligned.
      return nullptr;
    }
  }

  const absl::optional<syncer::DeviceInfo::PhoneAsASecurityKeyInfo>&
      maybe_paask_info = device->paask_info();
  if (!maybe_paask_info) {
    return nullptr;
  }

  const syncer::DeviceInfo::PhoneAsASecurityKeyInfo& paask_info =
      *maybe_paask_info;
  auto pairing = std::make_unique<Pairing>();
  pairing->from_sync_deviceinfo = true;
  pairing->name = NameForDisplay(device->client_name());

  const absl::optional<device::cablev2::tunnelserver::KnownDomainID>
      tunnel_server_domain = device::cablev2::tunnelserver::ToKnownDomainID(
          paask_info.tunnel_server_domain);
  if (!tunnel_server_domain) {
    // It's possible that a phone is running a more modern version of Chrome
    // and uses an assigned tunnel server domain that is unknown to this code.
    return nullptr;
  }

  pairing->tunnel_server_domain =
      device::cablev2::tunnelserver::DecodeDomain(*tunnel_server_domain);
  pairing->contact_id = paask_info.contact_id;
  pairing->peer_public_key_x962 = paask_info.peer_public_key_x962;
  pairing->secret.assign(paask_info.secret.begin(), paask_info.secret.end());
  pairing->last_updated = device->last_updated_timestamp();

  // The pairing ID from sync is zero-padded to the standard length.
  pairing->id.assign(device::cablev2::kPairingIDSize, 0);
  static_assert(device::cablev2::kPairingIDSize >= sizeof(paask_info.id), "");
  memcpy(pairing->id.data(), &paask_info.id, sizeof(paask_info.id));

  // The channel priority is only approximate and exists to help testing and
  // development. I.e. we want the development or Canary install on a device to
  // shadow the stable channel so that it's possible to test things. This code
  // is matching the string generated by `FormatUserAgentForSync`.
  const std::string& user_agent = device->sync_user_agent();
  if (user_agent.find("-devel") != std::string::npos) {
    pairing->channel_priority = 5;
  } else if (user_agent.find("(canary)") != std::string::npos) {
    pairing->channel_priority = 4;
  } else if (user_agent.find("(dev)") != std::string::npos) {
    pairing->channel_priority = 3;
  } else if (user_agent.find("(beta)") != std::string::npos) {
    pairing->channel_priority = 2;
  } else if (user_agent.find("(stable)") != std::string::npos) {
    pairing->channel_priority = 1;
  } else {
    pairing->channel_priority = 0;
  }

  return pairing;
}

std::vector<std::unique_ptr<Pairing>> GetSyncedDevices(Profile* const profile) {
  std::vector<std::unique_ptr<Pairing>> ret;
  syncer::DeviceInfoSyncService* const sync_service =
      DeviceInfoSyncServiceFactory::GetForProfile(profile);
  if (!sync_service) {
    return ret;
  }

  syncer::DeviceInfoTracker* const tracker =
      sync_service->GetDeviceInfoTracker();
  std::vector<std::unique_ptr<syncer::DeviceInfo>> devices =
      tracker->GetAllDeviceInfo();

  const base::Time now = base::Time::Now();
  for (const auto& device : devices) {
    std::unique_ptr<Pairing> pairing =
        PairingFromSyncedDevice(device.get(), now);
    if (!pairing) {
      continue;
    }
    ret.emplace_back(std::move(pairing));
  }

  return ret;
}

std::vector<std::unique_ptr<Pairing>> GetLinkedDevices(Profile* const profile) {
  PrefService* const prefs = profile->GetPrefs();
  const base::Value* pref_pairings =
      prefs->GetList(kWebAuthnCablePairingsPrefName);

  std::vector<std::unique_ptr<Pairing>> ret;
  for (const auto& pairing : pref_pairings->GetListDeprecated()) {
    if (!pairing.is_dict()) {
      continue;
    }

    auto out_pairing = std::make_unique<Pairing>();
    if (!CopyString(&out_pairing->name,
                    pairing.FindStringKey(kPairingPrefName)) ||
        !CopyString(&out_pairing->tunnel_server_domain,
                    pairing.FindStringKey(kPairingPrefTunnelServer)) ||
        !CopyBytestring(&out_pairing->contact_id,
                        pairing.FindStringKey(kPairingPrefContactId)) ||
        !CopyBytestring(&out_pairing->id,
                        pairing.FindStringKey(kPairingPrefId)) ||
        !CopyBytestring(&out_pairing->secret,
                        pairing.FindStringKey(kPairingPrefSecret)) ||
        !CopyBytestring(&out_pairing->peer_public_key_x962,
                        pairing.FindStringKey(kPairingPrefPublicKey))) {
      continue;
    }

    out_pairing->name = NameForDisplay(out_pairing->name);
    ret.emplace_back(std::move(out_pairing));
  }

  return ret;
}

// FindUniqueName checks whether |name| is already contained in |existing_names|
// (after projecting with |NameForDisplay|). If so it appends a counter so that
// it isn't.
std::string FindUniqueName(const std::string& orig_name,
                           base::span<const base::StringPiece> existing_names) {
  std::string name = orig_name;
  std::string name_for_display = NameForDisplay(name);
  for (int i = 1;; i++) {
    if (std::none_of(existing_names.begin(), existing_names.end(),
                     [&name_for_display](base::StringPiece s) -> bool {
                       return NameForDisplay(s) == name_for_display;
                     })) {
      // The new name is unique.
      break;
    }

    // The new name collides with an existing one. Append a counter to the
    // original and try again. (If the original string is right-to-left then
    // the counter will appear on the left, not the right, of the string even
    // though the codepoints are appended here.)
    name = orig_name + " (" + base::NumberToString(i) + ")";
    name_for_display = NameForDisplay(name);
  }

  return name;
}

}  // namespace

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(kWebAuthnCablePairingsPrefName,
                             user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

KnownDevices::KnownDevices() = default;
KnownDevices::~KnownDevices() = default;

// static
std::unique_ptr<KnownDevices> KnownDevices::FromProfile(Profile* profile) {
  if (profile->IsOffTheRecord()) {
    // For Incognito windows we collect the devices from the parent profile.
    // The `AuthenticatorRequestDialogModel` will notice that it's an OTR
    // profile and display a confirmation interstitial for makeCredential calls.
    profile = profile->GetOriginalProfile();
  }

  auto ret = std::make_unique<KnownDevices>();
  ret->synced_devices = GetSyncedDevices(profile);
  ret->linked_devices = GetLinkedDevices(profile);
  return ret;
}

std::vector<std::unique_ptr<Pairing>> MergeDevices(
    std::unique_ptr<KnownDevices> known_devices,
    const icu::Locale* locale) {
  std::vector<std::unique_ptr<Pairing>> ret;
  ret.swap(known_devices->synced_devices);
  std::sort(ret.begin(), ret.end(), Pairing::CompareByMostRecentFirst);

  for (auto& pairing : known_devices->linked_devices) {
    ret.emplace_back(std::move(pairing));
  }

  // All the pairings from sync come first in `ret`, sorted by most recent
  // first, followed by pairings from prefs, which are known to have unique
  // public keys within themselves. A stable sort by public key will group
  // together any pairings for the same Chrome instance, preferring recent sync
  // records, then `std::unique` will delete all but the first.
  std::stable_sort(ret.begin(), ret.end(), Pairing::CompareByPublicKey);
  ret.erase(std::unique(ret.begin(), ret.end(), Pairing::EqualPublicKeys),
            ret.end());

  // ret now contains only a single entry per Chrome install. There can still be
  // multiple entries for a given name, however. Sort by most recent and then by
  // channel. That means that, considering all the entries for a given name,
  // Sync entries on unstable channels have top priority. Within a given
  // channel, the most recent entry has priority.

  std::sort(ret.begin(), ret.end(), Pairing::CompareByMostRecentFirst);
  std::stable_sort(ret.begin(), ret.end(),
                   Pairing::CompareByLeastStableChannelFirst);

  // Last, sort by name while preserving the order of entries with the same
  // name. Need to make a reference for Compare, because libstdc++ would
  // need copy constructor otherwise.
  Pairing::NameComparator comparator = Pairing::CompareByName(locale);
  std::stable_sort(ret.begin(), ret.end(), std::ref(comparator));

  return ret;
}

std::vector<base::StringPiece> KnownDevices::Names() const {
  std::vector<base::StringPiece> names;
  names.reserve(this->synced_devices.size() + this->linked_devices.size());
  for (const std::unique_ptr<device::cablev2::Pairing>& device :
       this->synced_devices) {
    names.push_back(device->name);
  }
  for (const std::unique_ptr<device::cablev2::Pairing>& device :
       this->linked_devices) {
    names.push_back(device->name);
  }
  return names;
}

void AddPairing(PrefService* pref_service,
                std::unique_ptr<Pairing> pairing,
                base::span<const base::StringPiece> existing_names) {
  // This is called when doing a QR-code pairing with a phone and the phone
  // sends long-term pairing information during the handshake. The pairing
  // information is saved in preferences for future operations.
  if (!base::FeatureList::IsEnabled(device::kWebAuthPhoneSupport)) {
    NOTREACHED();
    return;
  }

  const std::string name = FindUniqueName(pairing->name, existing_names);

  // For Incognito/Guest profiles, pairings will only last for the duration of
  // that session. While an argument could be made that it's safe to persist
  // such pairing for longer, this seems like the safe option initially.
  ListPrefUpdate update(pref_service, kWebAuthnCablePairingsPrefName);

  // Find any existing entries with the same public key and replace them. The
  // handshake protocol requires the phone to prove possession of the public
  // key so it's not possible for an evil phone to displace another's pairing.
  std::string public_key_base64 =
      base::Base64Encode(pairing->peer_public_key_x962);
  DeletePairingByPublicKey(update.Get(), public_key_base64);

  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetKey(kPairingPrefPublicKey, base::Value(std::move(public_key_base64)));
  dict.SetKey(kPairingPrefTunnelServer,
              base::Value(pairing->tunnel_server_domain));
  dict.SetKey(kPairingPrefName, base::Value(std::move(name)));
  dict.SetKey(kPairingPrefContactId,
              base::Value(base::Base64Encode(pairing->contact_id)));
  dict.SetKey(kPairingPrefId, base::Value(base::Base64Encode(pairing->id)));
  dict.SetKey(kPairingPrefSecret,
              base::Value(base::Base64Encode(pairing->secret)));

  base::Time::Exploded now;
  base::Time::Now().UTCExplode(&now);
  dict.SetKey(kPairingPrefTime,
              // RFC 3339 time format.
              base::Value(base::StringPrintf(
                  "%04d-%02d-%02dT%02d:%02d:%02dZ", now.year, now.month,
                  now.day_of_month, now.hour, now.minute, now.second)));

  update->Append(std::move(dict));
}

// DeletePairingByPublicKey erases any pairing with the given public key
// from `list`.
void DeletePairingByPublicKey(
    PrefService* pref_service,
    std::array<uint8_t, device::kP256X962Length> public_key) {
  ListPrefUpdate update(pref_service, kWebAuthnCablePairingsPrefName);
  DeletePairingByPublicKey(update.Get(), base::Base64Encode(public_key));
}

bool RenamePairing(
    PrefService* pref_service,
    const std::array<uint8_t, device::kP256X962Length>& public_key,
    const std::string& new_name,
    base::span<const base::StringPiece> existing_names) {
  const std::string name = FindUniqueName(new_name, existing_names);
  const std::string public_key_base64 = base::Base64Encode(public_key);

  ListPrefUpdate update(pref_service, kWebAuthnCablePairingsPrefName);
  base::Value::ListView list = update.Get()->GetListDeprecated();

  for (base::Value& value : list) {
    if (!value.is_dict()) {
      continue;
    }
    const std::string* pref_public_key =
        value.FindStringKey(kPairingPrefPublicKey);
    if (pref_public_key && *pref_public_key == public_key_base64) {
      value.SetKey(kPairingPrefName, base::Value(std::move(name)));
      return true;
    }
  }

  return false;
}

}  // namespace cablev2
