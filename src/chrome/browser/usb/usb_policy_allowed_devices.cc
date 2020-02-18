// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/usb_policy_allowed_devices.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/strings/string_split.h"
#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "services/device/public/mojom/usb_device.mojom.h"
#include "services/device/public/mojom/usb_manager.mojom.h"
#include "url/gurl.h"

namespace {

constexpr char kPrefDevicesKey[] = "devices";
constexpr char kPrefUrlsKey[] = "urls";
constexpr char kPrefVendorIdKey[] = "vendor_id";
constexpr char kPrefProductIdKey[] = "product_id";

// Find the origin match by checking if a origin pair in |origin_set| matches
// the given origin pair. A nullopt embedding origin signifies a wildcard, so
// ignore the embedding origin check for this case.
bool FindMatchInSet(
    const std::set<std::pair<url::Origin, base::Optional<url::Origin>>>&
        origin_set,
    const url::Origin& requesting_origin,
    const url::Origin& embedding_origin) {
  for (const auto& origin_pair : origin_set) {
    if (origin_pair.first == requesting_origin) {
      if (!origin_pair.second || *origin_pair.second == embedding_origin)
        return true;
    }
  }
  return false;
}

}  // namespace

UsbPolicyAllowedDevices::UsbPolicyAllowedDevices(
    PrefService* profile_prefs,
    PrefService* local_state_prefs) {
  // Add observers for the prefs managed by user and device policy to call
  // CreateOrUpdateMap when the value is changed. The lifetimes of the
  // PrefChangeRegistrars are managed by this class, therefore it is safe to
  // use base::Unretained here.
  const base::Closure change_callback = base::BindRepeating(
      &UsbPolicyAllowedDevices::CreateOrUpdateMap, base::Unretained(this));

  profile_pref_change_registrar_.Init(profile_prefs);
  profile_pref_change_registrar_.Add(prefs::kManagedWebUsbAllowDevicesForUrls,
                                     change_callback);
#if defined(OS_CHROMEOS)
  if (local_state_prefs) {  // Can be nullptr in tests.
    local_state_pref_change_registrar_.Init(local_state_prefs);
    local_state_pref_change_registrar_.Add(
        prefs::kDeviceWebUsbAllowDevicesForUrls, change_callback);
  }
#endif  // defined(OS_CHROMEOS)

  CreateOrUpdateMap();
}

UsbPolicyAllowedDevices::~UsbPolicyAllowedDevices() {}

bool UsbPolicyAllowedDevices::IsDeviceAllowed(
    const url::Origin& requesting_origin,
    const url::Origin& embedding_origin,
    const device::mojom::UsbDeviceInfo& device_info) {
  return IsDeviceAllowed(
      requesting_origin, embedding_origin,
      std::make_pair(device_info.vendor_id, device_info.product_id));
}

bool UsbPolicyAllowedDevices::IsDeviceAllowed(
    const url::Origin& requesting_origin,
    const url::Origin& embedding_origin,
    const std::pair<int, int>& device_ids) {
  // Search through each set of URL pair that match the given device. The
  // keys correspond to the following URL pair sets:
  //  * (vendor_id, product_id): A set corresponding to the exact device.
  //  * (vendor_id, -1): A set corresponding to any device with |vendor_id|.
  //  * (-1, -1): A set corresponding to any device.
  const std::pair<int, int> set_keys[] = {
      std::make_pair(device_ids.first, device_ids.second),
      std::make_pair(device_ids.first, -1), std::make_pair(-1, -1)};

  for (const auto& key : set_keys) {
    const auto entry = usb_device_ids_to_urls_.find(key);
    if (entry == usb_device_ids_to_urls_.cend())
      continue;

    if (FindMatchInSet(entry->second, requesting_origin, embedding_origin))
      return true;
  }
  return false;
}

void UsbPolicyAllowedDevices::CreateOrUpdateMap() {
  usb_device_ids_to_urls_.clear();

#if defined(OS_CHROMEOS)
  if (local_state_pref_change_registrar_.prefs()) {  // Can be nullptr in tests.
    const base::Value* local_state_pref_value =
        local_state_pref_change_registrar_.prefs()->Get(
            prefs::kDeviceWebUsbAllowDevicesForUrls);
    CreateOrUpdateMapFromPrefValue(local_state_pref_value);
  }
#endif  // defined(OS_CHROMEOS)
  const base::Value* profile_pref_value =
      profile_pref_change_registrar_.prefs()->Get(
          prefs::kManagedWebUsbAllowDevicesForUrls);
  CreateOrUpdateMapFromPrefValue(profile_pref_value);
}

void UsbPolicyAllowedDevices::CreateOrUpdateMapFromPrefValue(
    const base::Value* pref_value) {
  // A policy has not been assigned.
  if (!pref_value) {
    return;
  }

  // The pref value has already been validated by the policy handler, so it is
  // safe to assume that |pref_value| follows the policy template.
  for (const auto& item : pref_value->GetList()) {
    const base::Value* urls_list = item.FindKey(kPrefUrlsKey);
    std::set<std::pair<url::Origin, base::Optional<url::Origin>>> parsed_set;

    // A urls item can contain a pair of URLs that are delimited by a comma. If
    // it does not contain a second URL, set the embedding URL to an empty GURL
    // to signify a wildcard embedded URL.
    for (const auto& urls_value : urls_list->GetList()) {
      std::vector<std::string> urls =
          base::SplitString(urls_value.GetString(), ",", base::TRIM_WHITESPACE,
                            base::SPLIT_WANT_ALL);

      // Skip invalid URL entries.
      if (urls.empty())
        continue;

      auto requesting_origin = url::Origin::Create(GURL(urls[0]));
      base::Optional<url::Origin> embedding_origin;
      if (urls.size() == 2 && !urls[1].empty())
        embedding_origin = url::Origin::Create(GURL(urls[1]));
      auto origin_pair = std::make_pair(std::move(requesting_origin),
                                        std::move(embedding_origin));

      parsed_set.insert(std::move(origin_pair));
    }

    // Ignore items with empty parsed URLs.
    if (parsed_set.empty())
      continue;

    // For each device entry in the map, create or update its respective URL
    // set.
    const base::Value* devices = item.FindKey(kPrefDevicesKey);
    for (const auto& device : devices->GetList()) {
      // A missing ID signifies a wildcard for that ID, so a sentinel value of
      // -1 is assigned.
      const base::Value* vendor_id_value = device.FindKey(kPrefVendorIdKey);
      const base::Value* product_id_value = device.FindKey(kPrefProductIdKey);
      int vendor_id = vendor_id_value ? vendor_id_value->GetInt() : -1;
      int product_id = product_id_value ? product_id_value->GetInt() : -1;
      DCHECK(vendor_id != -1 || product_id == -1);

      auto key = std::make_pair(vendor_id, product_id);
      usb_device_ids_to_urls_[key].insert(parsed_set.begin(), parsed_set.end());
    }
  }
}
