// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/hid/hid_policy_allowed_devices.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/containers/contains.h"
#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "services/device/public/mojom/hid.mojom.h"
#include "url/gurl.h"

namespace {

constexpr char kPrefDevicesKey[] = "devices";
constexpr char kPrefProductIdKey[] = "product_id";
constexpr char kPrefUrlsKey[] = "urls";
constexpr char kPrefUsageKey[] = "usage";
constexpr char kPrefUsagePageKey[] = "usage_page";
constexpr char kPrefUsagesKey[] = "usages";
constexpr char kPrefVendorIdKey[] = "vendor_id";

}  // namespace

HidPolicyAllowedDevices::HidPolicyAllowedDevices(PrefService* pref_service) {
  pref_change_registrar_.Init(pref_service);
  // The lifetime of |pref_change_registrar_| is managed by this class so it is
  // safe to use base::Unretained here.
  pref_change_registrar_.Add(
      prefs::kManagedWebHidAllowAllDevicesForUrls,
      base::BindRepeating(
          &HidPolicyAllowedDevices::LoadAllowAllDevicesForUrlsPolicy,
          base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kManagedWebHidAllowDevicesForUrls,
      base::BindRepeating(
          &HidPolicyAllowedDevices::LoadAllowDevicesForUrlsPolicy,
          base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kManagedWebHidAllowDevicesWithHidUsagesForUrls,
      base::BindRepeating(
          &HidPolicyAllowedDevices::LoadAllowDevicesWithHidUsagesForUrlsPolicy,
          base::Unretained(this)));

  LoadAllowAllDevicesForUrlsPolicy();
  LoadAllowDevicesForUrlsPolicy();
  LoadAllowDevicesWithHidUsagesForUrlsPolicy();
}

HidPolicyAllowedDevices::~HidPolicyAllowedDevices() = default;

// static
void HidPolicyAllowedDevices::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterListPref(prefs::kManagedWebHidAllowAllDevicesForUrls);
  registry->RegisterListPref(prefs::kManagedWebHidAllowDevicesForUrls);
  registry->RegisterListPref(
      prefs::kManagedWebHidAllowDevicesWithHidUsagesForUrls);
}

bool HidPolicyAllowedDevices::HasDevicePermission(
    const url::Origin& origin,
    const device::mojom::HidDeviceInfo& device) {
  if (base::Contains(all_devices_policy_, origin))
    return true;

  auto vendor_it = vendor_policy_.find(device.vendor_id);
  if (vendor_it != vendor_policy_.end() &&
      base::Contains(vendor_it->second, origin)) {
    return true;
  }

  auto device_it = device_policy_.find({device.vendor_id, device.product_id});
  if (device_it != device_policy_.end() &&
      base::Contains(device_it->second, origin)) {
    return true;
  }

  for (const auto& collection : device.collections) {
    auto usage_page_it = usage_page_policy_.find(collection->usage->usage_page);
    if (usage_page_it != usage_page_policy_.end() &&
        base::Contains(usage_page_it->second, origin)) {
      return true;
    }

    auto usage_it = usage_policy_.find(
        {collection->usage->usage_page, collection->usage->usage});
    if (usage_it != usage_policy_.end() &&
        base::Contains(usage_it->second, origin)) {
      return true;
    }
  }

  return false;
}

void HidPolicyAllowedDevices::LoadAllowAllDevicesForUrlsPolicy() {
  all_devices_policy_.clear();

  const auto* pref_value = pref_change_registrar_.prefs()->Get(
      prefs::kManagedWebHidAllowAllDevicesForUrls);
  if (!pref_value)
    return;

  // The pref value has already been validated by the policy handler, so it is
  // safe to assume that |pref_value| follows the policy template.
  for (const auto& url_value : pref_value->GetListDeprecated()) {
    GURL url(url_value.GetString());
    if (url.is_valid())
      all_devices_policy_.insert(url::Origin::Create(url));
  }
}

void HidPolicyAllowedDevices::LoadAllowDevicesForUrlsPolicy() {
  device_policy_.clear();
  vendor_policy_.clear();

  const auto* pref_value = pref_change_registrar_.prefs()->Get(
      prefs::kManagedWebHidAllowDevicesForUrls);
  if (!pref_value)
    return;

  // The pref value has already been validated by the policy handler, so it is
  // safe to assume that |pref_value| follows the policy template.
  for (const auto& item : pref_value->GetListDeprecated()) {
    const base::Value* urls_value = item.FindKey(kPrefUrlsKey);
    DCHECK(urls_value);

    std::vector<url::Origin> urls;
    for (const auto& url_value : urls_value->GetListDeprecated()) {
      GURL url(url_value.GetString());
      if (url.is_valid())
        urls.push_back(url::Origin::Create(url));
    }

    if (urls.empty())
      continue;

    const auto* devices_value = item.FindKey(kPrefDevicesKey);
    DCHECK(devices_value);
    for (const auto& device_value : devices_value->GetListDeprecated()) {
      const auto* vendor_id_value = device_value.FindKey(kPrefVendorIdKey);
      DCHECK(vendor_id_value);

      const auto* product_id_value = device_value.FindKey(kPrefProductIdKey);
      // "product_id" is optional. If it is not specified, the policy matches
      // any device with the given vendor ID.
      if (product_id_value) {
        device_policy_[{vendor_id_value->GetInt(), product_id_value->GetInt()}]
            .insert(urls.begin(), urls.end());
      } else {
        vendor_policy_[vendor_id_value->GetInt()].insert(urls.begin(),
                                                         urls.end());
      }
    }
  }
}

void HidPolicyAllowedDevices::LoadAllowDevicesWithHidUsagesForUrlsPolicy() {
  usage_policy_.clear();
  usage_page_policy_.clear();

  const auto* pref_value = pref_change_registrar_.prefs()->Get(
      prefs::kManagedWebHidAllowDevicesWithHidUsagesForUrls);
  if (!pref_value)
    return;

  // The pref value has already been validated by the policy handler, so it is
  // safe to assume that |pref_value| follows the policy template.
  for (const auto& item : pref_value->GetListDeprecated()) {
    const base::Value* urls_value = item.FindKey(kPrefUrlsKey);
    DCHECK(urls_value);

    std::vector<url::Origin> urls;
    for (const auto& url_value : urls_value->GetListDeprecated()) {
      GURL url(url_value.GetString());
      if (!url.is_valid())
        continue;

      urls.push_back(url::Origin::Create(url));
    }

    if (urls.empty())
      continue;

    const auto* usages_value = item.FindKey(kPrefUsagesKey);
    DCHECK(usages_value);
    for (const auto& usage_and_page_value : usages_value->GetListDeprecated()) {
      const auto* usage_page_value =
          usage_and_page_value.FindKey(kPrefUsagePageKey);
      DCHECK(usage_page_value);

      const auto* usage_value = usage_and_page_value.FindKey(kPrefUsageKey);
      // "usage" is optional. If "usage" is not specified, the policy matches
      // any device containing a top-level collection with the given usage page.
      if (usage_value) {
        usage_policy_[{usage_page_value->GetInt(), usage_value->GetInt()}]
            .insert(urls.begin(), urls.end());
      } else {
        usage_page_policy_[usage_page_value->GetInt()].insert(urls.begin(),
                                                              urls.end());
      }
    }
  }
}
