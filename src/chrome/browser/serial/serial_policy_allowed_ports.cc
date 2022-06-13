// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/serial/serial_policy_allowed_ports.h"

#include <vector>

#include "base/bind.h"
#include "base/containers/contains.h"
#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "services/device/public/mojom/serial.mojom.h"
#include "url/gurl.h"

namespace {

constexpr char kPrefDevicesKey[] = "devices";
constexpr char kPrefUrlsKey[] = "urls";
constexpr char kPrefVendorIdKey[] = "vendor_id";
constexpr char kPrefProductIdKey[] = "product_id";

}  // namespace

SerialPolicyAllowedPorts::SerialPolicyAllowedPorts(PrefService* pref_service) {
  pref_change_registrar_.Init(pref_service);

  // The lifetime of |pref_change_registrar_| is managed by this class so it is
  // safe to use base::Unretained() here.
  pref_change_registrar_.Add(
      prefs::kManagedSerialAllowAllPortsForUrls,
      base::BindRepeating(
          &SerialPolicyAllowedPorts::LoadAllowAllPortsForUrlsPolicy,
          base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kManagedSerialAllowUsbDevicesForUrls,
      base::BindRepeating(
          &SerialPolicyAllowedPorts::LoadAllowUsbDevicesForUrlsPolicy,
          base::Unretained(this)));

  LoadAllowAllPortsForUrlsPolicy();
  LoadAllowUsbDevicesForUrlsPolicy();
}

SerialPolicyAllowedPorts::~SerialPolicyAllowedPorts() = default;

// static
void SerialPolicyAllowedPorts::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(prefs::kManagedSerialAllowAllPortsForUrls);
  registry->RegisterListPref(prefs::kManagedSerialAllowUsbDevicesForUrls);
}

bool SerialPolicyAllowedPorts::HasPortPermission(
    const url::Origin& origin,
    const device::mojom::SerialPortInfo& port_info) {
  if (base::Contains(all_ports_policy_, origin)) {
    return true;
  }

  if (port_info.has_vendor_id) {
    auto it = usb_vendor_policy_.find(port_info.vendor_id);
    if (it != usb_vendor_policy_.end() && base::Contains(it->second, origin)) {
      return true;
    }
  }

  if (port_info.has_vendor_id && port_info.has_product_id) {
    auto it = usb_device_policy_.find(
        std::make_pair(port_info.vendor_id, port_info.product_id));
    if (it != usb_device_policy_.end() && base::Contains(it->second, origin)) {
      return true;
    }
  }

  return false;
}

void SerialPolicyAllowedPorts::LoadAllowAllPortsForUrlsPolicy() {
  all_ports_policy_.clear();

  const base::Value* pref_value = pref_change_registrar_.prefs()->Get(
      prefs::kManagedSerialAllowAllPortsForUrls);
  if (!pref_value) {
    return;
  }

  // The pref value has already been validated by the policy handler, so it is
  // safe to assume that |pref_value| follows the policy template.
  std::vector<url::Origin> urls;
  for (const auto& url_value : pref_value->GetList()) {
    GURL url(url_value.GetString());
    if (!url.is_valid()) {
      continue;
    }

    urls.push_back(url::Origin::Create(url));
  }

  all_ports_policy_.insert(urls.begin(), urls.end());
}

void SerialPolicyAllowedPorts::LoadAllowUsbDevicesForUrlsPolicy() {
  usb_device_policy_.clear();
  usb_vendor_policy_.clear();

  const base::Value* pref_value = pref_change_registrar_.prefs()->Get(
      prefs::kManagedSerialAllowUsbDevicesForUrls);
  if (!pref_value) {
    return;
  }

  // The pref value has already been validated by the policy handler, so it is
  // safe to assume that |pref_value| follows the policy template.
  for (const auto& item : pref_value->GetList()) {
    const base::Value* urls_value = item.FindKey(kPrefUrlsKey);
    DCHECK(urls_value);

    std::vector<url::Origin> urls;
    for (const auto& url_value : urls_value->GetList()) {
      GURL url(url_value.GetString());
      if (!url.is_valid()) {
        continue;
      }

      urls.push_back(url::Origin::Create(url));
    }

    if (urls.empty()) {
      continue;
    }

    const base::Value* devices_value = item.FindKey(kPrefDevicesKey);
    DCHECK(devices_value);
    for (const auto& port_value : devices_value->GetList()) {
      const base::Value* vendor_id_value = port_value.FindKey(kPrefVendorIdKey);
      DCHECK(vendor_id_value);

      const base::Value* product_id_value =
          port_value.FindKey(kPrefProductIdKey);
      // "product_id" is optional and the policy matches all devices with the
      // given vendor ID if it is not specified.
      if (product_id_value) {
        usb_device_policy_[{vendor_id_value->GetInt(),
                            product_id_value->GetInt()}]
            .insert(urls.begin(), urls.end());
      } else {
        usb_vendor_policy_[vendor_id_value->GetInt()].insert(urls.begin(),
                                                             urls.end());
      }
    }
  }
}
