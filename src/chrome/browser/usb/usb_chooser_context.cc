// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/usb_chooser_context.h"

#include <utility>
#include <vector>

#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/usb/usb_blocklist.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/common/service_manager_connection.h"
#include "device/usb/public/mojom/device.mojom.h"
#include "device/usb/usb_ids.h"
#include "services/device/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

constexpr char kDeviceNameKey[] = "name";
constexpr char kGuidKey[] = "ephemeral-guid";
constexpr char kProductIdKey[] = "product-id";
constexpr char kSerialNumberKey[] = "serial-number";
constexpr char kVendorIdKey[] = "vendor-id";
constexpr char kSourcePolicy[] = "policy";
constexpr char kSourcePreference[] = "preference";

// Reasons a permission may be closed. These are used in histograms so do not
// remove/reorder entries. Only add at the end just before
// WEBUSB_PERMISSION_REVOKED_MAX. Also remember to update the enum listing in
// tools/metrics/histograms/histograms.xml.
enum WebUsbPermissionRevoked {
  // Permission to access a USB device was revoked by the user.
  WEBUSB_PERMISSION_REVOKED = 0,
  // Permission to access an ephemeral USB device was revoked by the user.
  WEBUSB_PERMISSION_REVOKED_EPHEMERAL,
  // Maximum value for the enum.
  WEBUSB_PERMISSION_REVOKED_MAX
};

void RecordPermissionRevocation(WebUsbPermissionRevoked kind) {
  UMA_HISTOGRAM_ENUMERATION("WebUsb.PermissionRevoked", kind,
                            WEBUSB_PERMISSION_REVOKED_MAX);
}

bool CanStorePersistentEntry(const device::mojom::UsbDeviceInfo& device_info) {
  return !device_info.serial_number->empty();
}

std::pair<int, int> GetDeviceIds(const base::DictionaryValue& object) {
  DCHECK(object.FindKeyOfType(kVendorIdKey, base::Value::Type::INTEGER));
  int vendor_id = object.FindKey(kVendorIdKey)->GetInt();

  DCHECK(object.FindKeyOfType(kProductIdKey, base::Value::Type::INTEGER));
  int product_id = object.FindKey(kProductIdKey)->GetInt();

  return std::make_pair(vendor_id, product_id);
}

std::unique_ptr<base::DictionaryValue> DeviceInfoToDictValue(
    const device::mojom::UsbDeviceInfo& device_info) {
  auto device_dict = std::make_unique<base::DictionaryValue>();
  device_dict->SetKey(kDeviceNameKey,
                      device_info.product_name
                          ? base::Value(*device_info.product_name)
                          : base::Value(""));
  device_dict->SetKey(kVendorIdKey, base::Value(device_info.vendor_id));
  device_dict->SetKey(kProductIdKey, base::Value(device_info.product_id));

  // CanStorePersistentEntry checks if |device_info.serial_number| is not empty.
  if (CanStorePersistentEntry(device_info)) {
    device_dict->SetKey(kSerialNumberKey,
                        base::Value(*device_info.serial_number));
  } else {
    device_dict->SetKey(kGuidKey, base::Value(device_info.guid));
  }

  return device_dict;
}

base::string16 GetDeviceNameFromIds(int vendor_id, int product_id) {
// This is currently using the UI strings used for the chooser prompt. This is
// fine for now since the policy allowed devices are not being displayed in
// Site Settings yet. However, policy allowed devices can contain wildcards for
// the IDs, so more specific UI string need to be defined.
// TODO(https://crbug.com/854329): Add UI strings that are more specific to
// the Site Settings UI.
#if !defined(OS_ANDROID)
  const char* product_name =
      device::UsbIds::GetProductName(vendor_id, product_id);
  if (product_name)
    return base::UTF8ToUTF16(product_name);

  const char* vendor_name = device::UsbIds::GetVendorName(vendor_id);
  if (vendor_name) {
    return l10n_util::GetStringFUTF16(
        IDS_DEVICE_CHOOSER_DEVICE_NAME_UNKNOWN_DEVICE_WITH_VENDOR_NAME,
        base::UTF8ToUTF16(vendor_name));
  }
#endif  // !defined(OS_ANDROID)
  return l10n_util::GetStringFUTF16(
      IDS_DEVICE_CHOOSER_DEVICE_NAME_UNKNOWN_DEVICE_WITH_VENDOR_ID_AND_PRODUCT_ID,
      base::ASCIIToUTF16(base::StringPrintf("%04x", vendor_id)),
      base::ASCIIToUTF16(base::StringPrintf("%04x", product_id)));
}

std::unique_ptr<base::DictionaryValue> DeviceIdsToDictValue(int vendor_id,
                                                            int product_id) {
  auto device_dict = std::make_unique<base::DictionaryValue>();
  base::string16 device_name = GetDeviceNameFromIds(vendor_id, product_id);

  device_dict->SetKey(kDeviceNameKey, base::Value(device_name));
  device_dict->SetKey(kVendorIdKey, base::Value(vendor_id));
  device_dict->SetKey(kProductIdKey, base::Value(product_id));
  device_dict->SetKey(kSerialNumberKey, base::Value(std::string()));

  return device_dict;
}

}  // namespace

void UsbChooserContext::Observer::OnDeviceAdded(
    const device::mojom::UsbDeviceInfo& device_info) {}

void UsbChooserContext::Observer::OnDeviceRemoved(
    const device::mojom::UsbDeviceInfo& device_info) {}

void UsbChooserContext::Observer::OnPermissionRevoked(
    const GURL& requesting_origin,
    const GURL& embedding_origin) {}

void UsbChooserContext::Observer::OnDeviceManagerConnectionError() {}

UsbChooserContext::UsbChooserContext(Profile* profile)
    : ChooserContextBase(profile,
                         CONTENT_SETTINGS_TYPE_USB_GUARD,
                         CONTENT_SETTINGS_TYPE_USB_CHOOSER_DATA),
      is_incognito_(profile->IsOffTheRecord()),
      client_binding_(this),
      weak_factory_(this) {
  usb_policy_allowed_devices_.reset(
      new UsbPolicyAllowedDevices(profile->GetPrefs()));
}

void UsbChooserContext::InitDeviceList(
    std::vector<device::mojom::UsbDeviceInfoPtr> devices) {
  for (auto& device_info : devices) {
    DCHECK(device_info);
    devices_.insert(std::make_pair(device_info->guid, std::move(device_info)));
  }
  is_initialized_ = true;

  while (!pending_get_devices_requests_.empty()) {
    std::vector<device::mojom::UsbDeviceInfoPtr> device_list;
    for (const auto& entry : devices_) {
      device_list.push_back(entry.second->Clone());
    }
    std::move(pending_get_devices_requests_.front())
        .Run(std::move(device_list));
    pending_get_devices_requests_.pop();
  }
}

void UsbChooserContext::EnsureConnectionWithDeviceManager() {
  if (device_manager_)
    return;

  // Request UsbDeviceManagerPtr from DeviceService.
  content::ServiceManagerConnection::GetForProcess()
      ->GetConnector()
      ->BindInterface(device::mojom::kServiceName,
                      mojo::MakeRequest(&device_manager_));

  SetUpDeviceManagerConnection();
}

void UsbChooserContext::SetUpDeviceManagerConnection() {
  DCHECK(device_manager_);
  device_manager_.set_connection_error_handler(
      base::BindOnce(&UsbChooserContext::OnDeviceManagerConnectionError,
                     base::Unretained(this)));

  // Listen for added/removed device events.
  DCHECK(!client_binding_);
  device::mojom::UsbDeviceManagerClientAssociatedPtrInfo client;
  client_binding_.Bind(mojo::MakeRequest(&client));
  device_manager_->EnumerateDevicesAndSetClient(
      std::move(client), base::BindOnce(&UsbChooserContext::InitDeviceList,
                                        weak_factory_.GetWeakPtr()));
}

UsbChooserContext::~UsbChooserContext() {
  OnDeviceManagerConnectionError();
}

std::vector<std::unique_ptr<base::DictionaryValue>>
UsbChooserContext::GetGrantedObjects(const GURL& requesting_origin,
                                     const GURL& embedding_origin) {
  std::vector<std::unique_ptr<base::DictionaryValue>> objects =
      ChooserContextBase::GetGrantedObjects(requesting_origin,
                                            embedding_origin);

  if (CanRequestObjectPermission(requesting_origin, embedding_origin)) {
    auto it = ephemeral_devices_.find(
        std::make_pair(requesting_origin, embedding_origin));
    if (it != ephemeral_devices_.end()) {
      for (const std::string& guid : it->second) {
        // |devices_| should be initialized when |ephemeral_devices_| is filled.
        // Because |ephemeral_devices_| is filled by GrantDevicePermission()
        // which is called in UsbChooserController::Select(), this method will
        // always be called after device initialization in UsbChooserController
        // which always returns after the device list initialization in this
        // class.
        DCHECK(base::ContainsKey(devices_, guid));
        objects.push_back(DeviceInfoToDictValue(*devices_[guid]));
      }
    }
  }

  return objects;
}

std::vector<std::unique_ptr<ChooserContextBase::Object>>
UsbChooserContext::GetAllGrantedObjects() {
  std::vector<std::unique_ptr<ChooserContextBase::Object>> objects =
      ChooserContextBase::GetAllGrantedObjects();

  for (const auto& map_entry : ephemeral_devices_) {
    const GURL& requesting_origin = map_entry.first.first;
    const GURL& embedding_origin = map_entry.first.second;

    if (!CanRequestObjectPermission(requesting_origin, embedding_origin))
      continue;

    for (const std::string& guid : map_entry.second) {
      DCHECK(base::ContainsKey(devices_, guid));
      // ChooserContextBase::Object constructor will swap the object.
      auto object = DeviceInfoToDictValue(*devices_[guid]);
      objects.push_back(std::make_unique<ChooserContextBase::Object>(
          requesting_origin, embedding_origin, object.get(), kSourcePreference,
          is_incognito_));
    }
  }

  // Iterate through the user granted objects to create a mapping of device IDs
  // to device object for the policy granted objects to use, and remove
  // objects that have already been granted permission by the policy.
  std::map<std::pair<int, int>, base::Value> device_ids_to_object_map;
  for (auto it = objects.begin(); it != objects.end();) {
    const Object& object = **it;
    auto device_ids = GetDeviceIds(object.object);
    const GURL& requesting_origin = object.requesting_origin;
    const GURL& embedding_origin = object.embedding_origin;

    device_ids_to_object_map[device_ids] = object.object.Clone();

    if (usb_policy_allowed_devices_->IsDeviceAllowed(
            requesting_origin, embedding_origin, device_ids)) {
      it = objects.erase(it);
    } else {
      ++it;
    }
  }

  for (const auto& allowed_devices_entry : usb_policy_allowed_devices_->map()) {
    // The map key is a tuple of (vendor_id, product_id).
    const int vendor_id = allowed_devices_entry.first.first;
    const int product_id = allowed_devices_entry.first.second;

    for (const auto& url_pair : allowed_devices_entry.second) {
      std::unique_ptr<base::DictionaryValue> object;
      auto it =
          device_ids_to_object_map.find(std::make_pair(vendor_id, product_id));
      if (it != device_ids_to_object_map.end()) {
        object = base::DictionaryValue::From(
            base::Value::ToUniquePtrValue(it->second.Clone()));
      } else {
        object = DeviceIdsToDictValue(vendor_id, product_id);
      }

      objects.push_back(std::make_unique<ChooserContextBase::Object>(
          url_pair.first, url_pair.second, object.get(), kSourcePolicy,
          is_incognito_));
    }
  }

  return objects;
}

void UsbChooserContext::RevokeObjectPermission(
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    const base::DictionaryValue& object) {
  std::string guid;
  if (object.GetString(kGuidKey, &guid)) {
    auto it = ephemeral_devices_.find(
        std::make_pair(requesting_origin, embedding_origin));
    if (it != ephemeral_devices_.end()) {
      it->second.erase(guid);
      if (it->second.empty())
        ephemeral_devices_.erase(it);
    }
    RecordPermissionRevocation(WEBUSB_PERMISSION_REVOKED_EPHEMERAL);
  } else {
    ChooserContextBase::RevokeObjectPermission(requesting_origin,
                                               embedding_origin, object);
    RecordPermissionRevocation(WEBUSB_PERMISSION_REVOKED);
  }

  // Notify observers about the permission revocation.
  for (auto& observer : observer_list_)
    observer.OnPermissionRevoked(requesting_origin, embedding_origin);
}

void UsbChooserContext::GrantDevicePermission(
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    const device::mojom::UsbDeviceInfo& device_info) {
  if (CanStorePersistentEntry(device_info)) {
    GrantObjectPermission(requesting_origin, embedding_origin,
                          DeviceInfoToDictValue(device_info));
  } else {
    ephemeral_devices_[std::make_pair(requesting_origin, embedding_origin)]
        .insert(device_info.guid);
  }
}

bool UsbChooserContext::HasDevicePermission(
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    const device::mojom::UsbDeviceInfo& device_info) {
  if (UsbBlocklist::Get().IsExcluded(device_info))
    return false;

  if (usb_policy_allowed_devices_->IsDeviceAllowed(
          requesting_origin, embedding_origin, device_info)) {
    return true;
  }

  if (!CanRequestObjectPermission(requesting_origin, embedding_origin))
    return false;

  auto it = ephemeral_devices_.find(
      std::make_pair(requesting_origin, embedding_origin));
  if (it != ephemeral_devices_.end() &&
      base::ContainsKey(it->second, device_info.guid)) {
    return true;
  }

  std::vector<std::unique_ptr<base::DictionaryValue>> device_list =
      GetGrantedObjects(requesting_origin, embedding_origin);
  for (const std::unique_ptr<base::DictionaryValue>& device_dict :
       device_list) {
    int vendor_id;
    int product_id;
    base::string16 serial_number;
    if (device_dict->GetInteger(kVendorIdKey, &vendor_id) &&
        device_info.vendor_id == vendor_id &&
        device_dict->GetInteger(kProductIdKey, &product_id) &&
        device_info.product_id == product_id &&
        device_dict->GetString(kSerialNumberKey, &serial_number) &&
        device_info.serial_number == serial_number) {
      return true;
    }
  }

  return false;
}

void UsbChooserContext::GetDevices(
    device::mojom::UsbDeviceManager::GetDevicesCallback callback) {
  if (!is_initialized_) {
    pending_get_devices_requests_.push(std::move(callback));
    return;
  }

  std::vector<device::mojom::UsbDeviceInfoPtr> device_list;
  for (const auto& pair : devices_)
    device_list.push_back(pair.second->Clone());
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(device_list)));
}

void UsbChooserContext::GetDevice(
    const std::string& guid,
    device::mojom::UsbDeviceRequest device_request,
    device::mojom::UsbDeviceClientPtr device_client) {
  EnsureConnectionWithDeviceManager();
  device_manager_->GetDevice(guid, std::move(device_request),
                             std::move(device_client));
}

const device::mojom::UsbDeviceInfo* UsbChooserContext::GetDeviceInfo(
    const std::string& guid) {
  DCHECK(is_initialized_);
  auto it = devices_.find(guid);
  return it == devices_.end() ? nullptr : it->second.get();
}

void UsbChooserContext::AddObserver(Observer* observer) {
  EnsureConnectionWithDeviceManager();
  observer_list_.AddObserver(observer);
}

void UsbChooserContext::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

base::WeakPtr<UsbChooserContext> UsbChooserContext::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

bool UsbChooserContext::IsValidObject(const base::DictionaryValue& object) {
  return object.size() == 4 && object.HasKey(kDeviceNameKey) &&
         object.HasKey(kVendorIdKey) && object.HasKey(kProductIdKey) &&
         (object.HasKey(kSerialNumberKey) || object.HasKey(kGuidKey));
}

std::string UsbChooserContext::GetObjectName(
    const base::DictionaryValue& object) {
  DCHECK(IsValidObject(object));
  std::string name;
  bool found = object.GetString(kDeviceNameKey, &name);
  DCHECK(found);
  return name;
}

void UsbChooserContext::OnDeviceAdded(
    device::mojom::UsbDeviceInfoPtr device_info) {
  DCHECK(device_info);
  // Update the device list.
  DCHECK(!base::ContainsKey(devices_, device_info->guid));
  devices_.insert(std::make_pair(device_info->guid, device_info->Clone()));

  // Notify all observers.
  for (auto& observer : observer_list_)
    observer.OnDeviceAdded(*device_info);
}

void UsbChooserContext::OnDeviceRemoved(
    device::mojom::UsbDeviceInfoPtr device_info) {
  DCHECK(device_info);
  // Update the device list.
  DCHECK(base::ContainsKey(devices_, device_info->guid));
  devices_.erase(device_info->guid);

  // Notify all observers.
  for (auto& observer : observer_list_)
    observer.OnDeviceRemoved(*device_info);

  for (auto& map_entry : ephemeral_devices_)
    map_entry.second.erase(device_info->guid);
}

void UsbChooserContext::OnDeviceManagerConnectionError() {
  device_manager_.reset();
  client_binding_.Close();
  devices_.clear();
  is_initialized_ = false;
  ephemeral_devices_.clear();

  // Notify all observers.
  for (auto& observer : observer_list_)
    observer.OnDeviceManagerConnectionError();
}

void UsbChooserContext::SetDeviceManagerForTesting(
    device::mojom::UsbDeviceManagerPtr fake_device_manager) {
  DCHECK(!device_manager_);
  DCHECK(fake_device_manager);
  device_manager_ = std::move(fake_device_manager);
  SetUpDeviceManagerConnection();
}
