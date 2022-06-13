// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/contexts/bluetooth_chooser_context.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permissions_client.h"
#include "content/public/browser/browser_context.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"
#include "third_party/blink/public/mojom/bluetooth/web_bluetooth.mojom.h"
#include "url/origin.h"

using blink::WebBluetoothDeviceId;
using device::BluetoothUUID;

namespace permissions {

namespace {

// The Bluetooth device permission objects are dictionary type base::Values. The
// object contains keys for the device address, device name, services that can
// be accessed, and the generated web bluetooth device ID. Since base::Value
// does not have a set type, the services key contains another dictionary type
// base::Value object where each key is a UUID for a service and the value is a
// boolean that is never used. This allows for service permissions to be queried
// quickly and for new service permissions to added without duplicating existing
// service permissions. The following is an example of how a device permission
// is formatted using JSON notation:
// {
//   "device-address": "00:00:00:00:00:00",
//   "name": "Wireless Device",
//   "services": {
//     "0xabcd": "true",
//     "0x1234": "true",
//   },
//   "web-bluetooth-device-id": "4ik7W0WVaGFY6zXxJqdAKw==",
// }
constexpr char kDeviceAddressKey[] = "device-address";
constexpr char kDeviceNameKey[] = "name";
constexpr char kManufacturerDataKey[] = "manufacturer-data";
constexpr char kServicesKey[] = "services";
constexpr char kWebBluetoothDeviceIdKey[] = "web-bluetooth-device-id";

// The Web Bluetooth API spec states that when the user selects a device to
// pair with the origin, the origin is allowed to access any service listed in
// |options->filters| and |options->optional_services|.
// https://webbluetoothcg.github.io/web-bluetooth/#bluetooth
void AddUnionOfServicesTo(
    const blink::mojom::WebBluetoothRequestDeviceOptions* options,
    base::Value* permission_object) {
  if (!options)
    return;

  DCHECK(!!permission_object->FindDictKey(kServicesKey));
  auto& services_dict = *permission_object->FindDictKey(kServicesKey);
  if (options->filters) {
    for (const blink::mojom::WebBluetoothLeScanFilterPtr& filter :
         options->filters.value()) {
      if (!filter->services)
        continue;

      for (const BluetoothUUID& uuid : filter->services.value())
        services_dict.SetBoolKey(uuid.canonical_value(), /*val=*/true);
    }
  }

  for (const BluetoothUUID& uuid : options->optional_services)
    services_dict.SetBoolKey(uuid.canonical_value(), /*val=*/true);
}

void AddManufacturerDataTo(
    const blink::mojom::WebBluetoothRequestDeviceOptions* options,
    base::Value* permission_object) {
  if (!options || options->optional_manufacturer_data.empty())
    return;

  base::flat_set<uint16_t> manufacturer_data_set(
      options->optional_manufacturer_data);

  if (!permission_object->FindListKey(kManufacturerDataKey)) {
    base::Value manufacturer_data_value(base::Value::Type::LIST);
    permission_object->SetKey(kManufacturerDataKey,
                              std::move(manufacturer_data_value));
  }

  auto& manufacturer_data_list =
      *permission_object->FindListKey(kManufacturerDataKey);
  for (const auto& manufacturer_data_permission :
       manufacturer_data_list.GetList()) {
    manufacturer_data_set.insert(
        static_cast<uint16_t>(manufacturer_data_permission.GetInt()));
  }

  manufacturer_data_list.ClearList();
  for (const uint16_t manufacturer_code : manufacturer_data_set)
    manufacturer_data_list.Append(manufacturer_code);
}

base::Value DeviceInfoToDeviceObject(
    const device::BluetoothDevice* device,
    const blink::mojom::WebBluetoothRequestDeviceOptions* options,
    const WebBluetoothDeviceId& device_id) {
  base::Value device_value(base::Value::Type::DICTIONARY);
  device_value.SetStringKey(kDeviceAddressKey, device->GetAddress());
  device_value.SetStringKey(kWebBluetoothDeviceIdKey, device_id.str());
  device_value.SetStringKey(kDeviceNameKey, device->GetNameForDisplay());

  base::Value services_value(base::Value::Type::DICTIONARY);
  device_value.SetKey(kServicesKey, std::move(services_value));
  AddUnionOfServicesTo(options, &device_value);

  base::Value manufacturer_data_value(base::Value::Type::LIST);
  device_value.SetKey(kManufacturerDataKey, std::move(manufacturer_data_value));
  AddManufacturerDataTo(options, &device_value);

  return device_value;
}

}  // namespace

BluetoothChooserContext::BluetoothChooserContext(
    content::BrowserContext* browser_context)
    : ObjectPermissionContextBase(
          ContentSettingsType::BLUETOOTH_GUARD,
          ContentSettingsType::BLUETOOTH_CHOOSER_DATA,
          PermissionsClient::Get()->GetSettingsMap(browser_context)) {}

BluetoothChooserContext::~BluetoothChooserContext() = default;

WebBluetoothDeviceId BluetoothChooserContext::GetWebBluetoothDeviceId(
    const url::Origin& origin,
    const std::string& device_address) {
  const std::vector<std::unique_ptr<Object>> object_list =
      GetGrantedObjects(origin);
  for (const auto& object : object_list) {
    const base::Value& device = object->value;
    DCHECK(IsValidObject(device));

    if (device_address == *device.FindStringKey(kDeviceAddressKey)) {
      return WebBluetoothDeviceId(
          *device.FindStringKey(kWebBluetoothDeviceIdKey));
    }
  }

  // Check if the device has been assigned an ID through an LE scan.
  auto scanned_devices_it = scanned_devices_.find(origin);
  if (scanned_devices_it == scanned_devices_.end())
    return {};

  auto address_to_id_it = scanned_devices_it->second.find(device_address);
  if (address_to_id_it != scanned_devices_it->second.end())
    return address_to_id_it->second;
  return {};
}

std::string BluetoothChooserContext::GetDeviceAddress(
    const url::Origin& origin,
    const WebBluetoothDeviceId& device_id) {
  base::Value device = FindDeviceObject(origin, device_id);
  if (!device.is_none())
    return *device.FindStringKey(kDeviceAddressKey);

  // Check if the device ID corresponds to a device detected via an LE scan.
  auto scanned_devices_it = scanned_devices_.find(origin);
  if (scanned_devices_it == scanned_devices_.end())
    return std::string();

  for (const auto& entry : scanned_devices_it->second) {
    if (entry.second == device_id)
      return entry.first;
  }
  return std::string();
}

WebBluetoothDeviceId BluetoothChooserContext::AddScannedDevice(
    const url::Origin& origin,
    const std::string& device_address) {
  // Check if a WebBluetoothDeviceId already exists for the device with
  // |device_address| for the current origin.
  const auto granted_id = GetWebBluetoothDeviceId(origin, device_address);
  if (granted_id.IsValid())
    return granted_id;

  DeviceAddressToIdMap& address_to_id_map = scanned_devices_[origin];
  auto scanned_id = WebBluetoothDeviceId::Create();
  address_to_id_map.emplace(device_address, scanned_id);
  return scanned_id;
}

WebBluetoothDeviceId BluetoothChooserContext::GrantServiceAccessPermission(
    const url::Origin& origin,
    const device::BluetoothDevice* device,
    const blink::mojom::WebBluetoothRequestDeviceOptions* options) {
  // If |origin| already has permission to access the device with
  // |device_address|, update the allowed GATT services by performing a union of
  // |services|.
  const std::vector<std::unique_ptr<Object>> object_list =
      GetGrantedObjects(origin);
  const std::string& device_address = device->GetAddress();
  for (const auto& object : object_list) {
    base::Value& device_object = object->value;
    DCHECK(IsValidObject(device_object));
    if (device_address == *device_object.FindStringKey(kDeviceAddressKey)) {
      auto new_device_object = device_object.Clone();
      WebBluetoothDeviceId device_id(
          *new_device_object.FindStringKey(kWebBluetoothDeviceIdKey));

      AddUnionOfServicesTo(options, &new_device_object);
      AddManufacturerDataTo(options, &new_device_object);
      UpdateObjectPermission(origin, device_object,
                             std::move(new_device_object));
      return device_id;
    }
  }

  // If the device has been detected through the Web Bluetooth Scanning API,
  // grant permission using the WebBluetoothDeviceId generated through that API.
  // Remove the ID from the temporary |scanned_devices_| map to avoid
  // duplication, since the ID will now be stored in HostContentSettingsMap.
  WebBluetoothDeviceId device_id;
  auto scanned_devices_it = scanned_devices_.find(origin);
  if (scanned_devices_it != scanned_devices_.end()) {
    auto& address_to_id_map = scanned_devices_it->second;
    auto address_to_id_it = address_to_id_map.find(device_address);

    if (address_to_id_it != address_to_id_map.end()) {
      device_id = address_to_id_it->second;
      address_to_id_map.erase(address_to_id_it);

      if (scanned_devices_it->second.empty())
        scanned_devices_.erase(scanned_devices_it);
    }
  }

  if (!device_id.IsValid())
    device_id = WebBluetoothDeviceId::Create();

  base::Value permission_object =
      DeviceInfoToDeviceObject(device, options, device_id);
  GrantObjectPermission(origin, std::move(permission_object));
  return device_id;
}

bool BluetoothChooserContext::HasDevicePermission(
    const url::Origin& origin,
    const WebBluetoothDeviceId& device_id) {
  base::Value device = FindDeviceObject(origin, device_id);
  return !device.is_none();
}

bool BluetoothChooserContext::IsAllowedToAccessAtLeastOneService(
    const url::Origin& origin,
    const WebBluetoothDeviceId& device_id) {
  base::Value device = FindDeviceObject(origin, device_id);
  if (device.is_none())
    return false;
  return !device.FindDictKey(kServicesKey)->DictEmpty();
}

bool BluetoothChooserContext::IsAllowedToAccessService(
    const url::Origin& origin,
    const WebBluetoothDeviceId& device_id,
    const BluetoothUUID& service) {
  base::Value device = FindDeviceObject(origin, device_id);
  if (device.is_none())
    return false;

  const auto& services_dict = *device.FindDictKey(kServicesKey);
  return !!services_dict.FindKey(service.canonical_value());
}

bool BluetoothChooserContext::IsAllowedToAccessManufacturerData(
    const url::Origin& origin,
    const WebBluetoothDeviceId& device_id,
    uint16_t manufacturer_code) {
  base::Value device = FindDeviceObject(origin, device_id);
  if (device.is_none())
    return false;

  const auto* manufacturer_data_list = device.FindListKey(kManufacturerDataKey);
  if (!manufacturer_data_list)
    return false;

  for (const auto& manufacturer_data : manufacturer_data_list->GetList()) {
    if (manufacturer_code == manufacturer_data.GetInt())
      return true;
  }
  return false;
}

// static
WebBluetoothDeviceId BluetoothChooserContext::GetObjectDeviceId(
    const base::Value& object) {
  std::string device_id_str = *object.FindStringKey(kWebBluetoothDeviceIdKey);
  return WebBluetoothDeviceId(device_id_str);
}

std::string BluetoothChooserContext::GetKeyForObject(
    const base::Value& object) {
  if (!IsValidObject(object))
    return std::string();
  return *(object.FindStringKey(kWebBluetoothDeviceIdKey));
}

bool BluetoothChooserContext::IsValidObject(const base::Value& object) {
  return object.FindStringKey(kDeviceAddressKey) &&
         object.FindStringKey(kDeviceNameKey) &&
         object.FindStringKey(kWebBluetoothDeviceIdKey) &&
         WebBluetoothDeviceId::IsValid(
             *object.FindStringKey(kWebBluetoothDeviceIdKey)) &&
         object.FindDictKey(kServicesKey);
}

std::u16string BluetoothChooserContext::GetObjectDisplayName(
    const base::Value& object) {
  return base::UTF8ToUTF16(*object.FindStringKey(kDeviceNameKey));
}

base::Value BluetoothChooserContext::FindDeviceObject(
    const url::Origin& origin,
    const blink::WebBluetoothDeviceId& device_id) {
  const std::vector<std::unique_ptr<Object>> object_list =
      GetGrantedObjects(origin);
  for (const auto& object : object_list) {
    base::Value device = std::move(object->value);
    DCHECK(IsValidObject(device));

    const WebBluetoothDeviceId web_bluetooth_device_id(
        *device.FindStringKey(kWebBluetoothDeviceIdKey));
    if (device_id == web_bluetooth_device_id)
      return device;
  }
  return base::Value(base::Value::Type::NONE);
}

}  // namespace permissions
