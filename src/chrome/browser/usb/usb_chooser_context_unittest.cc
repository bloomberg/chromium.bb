// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/bind_helpers.h"
#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/usb/usb_chooser_context.h"
#include "chrome/browser/usb/usb_chooser_context_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "device/usb/public/cpp/fake_usb_device_manager.h"
#include "device/usb/public/mojom/device.mojom.h"
#include "device/usb/public/mojom/device_manager.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

using device::mojom::UsbDeviceInfoPtr;

namespace {

constexpr char kDeviceNameKey[] = "name";
constexpr char kGuidKey[] = "ephemeral-guid";
constexpr char kProductIdKey[] = "product-id";
constexpr char kSerialNumberKey[] = "serial-number";
constexpr char kVendorIdKey[] = "vendor-id";
constexpr int kDeviceIdWildcard = -1;

class UsbChooserContextTest : public testing::Test {
 public:
  UsbChooserContextTest() {}
  ~UsbChooserContextTest() override {}

 protected:
  Profile* profile() { return &profile_; }

  UsbChooserContext* GetChooserContext(Profile* profile) {
    auto* chooser_context = UsbChooserContextFactory::GetForProfile(profile);
    device::mojom::UsbDeviceManagerPtr device_manager_ptr;
    device_manager_.AddBinding(mojo::MakeRequest(&device_manager_ptr));
    chooser_context->SetDeviceManagerForTesting(std::move(device_manager_ptr));

    // Call GetDevices once to make sure the connection with DeviceManager has
    // been set up, so that it can be notified when device is removed.
    chooser_context->GetDevices(
        base::DoNothing::Once<std::vector<UsbDeviceInfoPtr>>());
    base::RunLoop().RunUntilIdle();
    return chooser_context;
  }

  device::FakeUsbDeviceManager device_manager_;

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;
};

}  // namespace

TEST_F(UsbChooserContextTest, CheckGrantAndRevokePermission) {
  GURL origin("https://www.google.com");
  UsbDeviceInfoPtr device_info =
      device_manager_.CreateAndAddDevice(0, 0, "Google", "Gizmo", "123ABC");
  UsbChooserContext* store = GetChooserContext(profile());

  base::DictionaryValue object_dict;
  object_dict.SetString(kDeviceNameKey, "Gizmo");
  object_dict.SetInteger(kVendorIdKey, 0);
  object_dict.SetInteger(kProductIdKey, 0);
  object_dict.SetString(kSerialNumberKey, "123ABC");

  EXPECT_FALSE(store->HasDevicePermission(origin, origin, *device_info));
  store->GrantDevicePermission(origin, origin, *device_info);
  EXPECT_TRUE(store->HasDevicePermission(origin, origin, *device_info));
  std::vector<std::unique_ptr<base::DictionaryValue>> objects =
      store->GetGrantedObjects(origin, origin);
  ASSERT_EQ(1u, objects.size());
  EXPECT_TRUE(object_dict.Equals(objects[0].get()));
  std::vector<std::unique_ptr<ChooserContextBase::Object>> all_origin_objects =
      store->GetAllGrantedObjects();
  ASSERT_EQ(1u, all_origin_objects.size());
  EXPECT_EQ(origin, all_origin_objects[0]->requesting_origin);
  EXPECT_EQ(origin, all_origin_objects[0]->embedding_origin);
  EXPECT_TRUE(object_dict.Equals(&all_origin_objects[0]->object));
  EXPECT_FALSE(all_origin_objects[0]->incognito);

  store->RevokeObjectPermission(origin, origin, *objects[0]);
  EXPECT_FALSE(store->HasDevicePermission(origin, origin, *device_info));
  objects = store->GetGrantedObjects(origin, origin);
  EXPECT_EQ(0u, objects.size());
  all_origin_objects = store->GetAllGrantedObjects();
  EXPECT_EQ(0u, all_origin_objects.size());
}

TEST_F(UsbChooserContextTest, CheckGrantAndRevokeEphemeralPermission) {
  GURL origin("https://www.google.com");
  UsbDeviceInfoPtr device_info =
      device_manager_.CreateAndAddDevice(0, 0, "Google", "Gizmo", "");
  UsbDeviceInfoPtr other_device_info =
      device_manager_.CreateAndAddDevice(0, 0, "Google", "Gizmo", "");

  UsbChooserContext* store = GetChooserContext(profile());

  base::DictionaryValue object_dict;
  object_dict.SetString(kDeviceNameKey, "Gizmo");
  object_dict.SetString(kGuidKey, device_info->guid);
  object_dict.SetInteger(kVendorIdKey, device_info->vendor_id);
  object_dict.SetInteger(kProductIdKey, device_info->product_id);

  EXPECT_FALSE(store->HasDevicePermission(origin, origin, *device_info));
  store->GrantDevicePermission(origin, origin, *device_info);
  EXPECT_TRUE(store->HasDevicePermission(origin, origin, *device_info));
  EXPECT_FALSE(store->HasDevicePermission(origin, origin, *other_device_info));
  std::vector<std::unique_ptr<base::DictionaryValue>> objects =
      store->GetGrantedObjects(origin, origin);
  EXPECT_EQ(1u, objects.size());
  EXPECT_TRUE(object_dict.Equals(objects[0].get()));
  std::vector<std::unique_ptr<ChooserContextBase::Object>> all_origin_objects =
      store->GetAllGrantedObjects();
  EXPECT_EQ(1u, all_origin_objects.size());
  EXPECT_EQ(origin, all_origin_objects[0]->requesting_origin);
  EXPECT_EQ(origin, all_origin_objects[0]->embedding_origin);
  EXPECT_TRUE(object_dict.Equals(&all_origin_objects[0]->object));
  EXPECT_FALSE(all_origin_objects[0]->incognito);

  store->RevokeObjectPermission(origin, origin, *objects[0]);
  EXPECT_FALSE(store->HasDevicePermission(origin, origin, *device_info));
  objects = store->GetGrantedObjects(origin, origin);
  EXPECT_EQ(0u, objects.size());
  all_origin_objects = store->GetAllGrantedObjects();
  EXPECT_EQ(0u, all_origin_objects.size());
}

TEST_F(UsbChooserContextTest, DisconnectDeviceWithPermission) {
  GURL origin("https://www.google.com");
  UsbDeviceInfoPtr device_info =
      device_manager_.CreateAndAddDevice(0, 0, "Google", "Gizmo", "123ABC");

  UsbChooserContext* store = GetChooserContext(profile());

  EXPECT_FALSE(store->HasDevicePermission(origin, origin, *device_info));
  store->GrantDevicePermission(origin, origin, *device_info);
  EXPECT_TRUE(store->HasDevicePermission(origin, origin, *device_info));
  std::vector<std::unique_ptr<base::DictionaryValue>> objects =
      store->GetGrantedObjects(origin, origin);
  EXPECT_EQ(1u, objects.size());
  std::vector<std::unique_ptr<ChooserContextBase::Object>> all_origin_objects =
      store->GetAllGrantedObjects();
  EXPECT_EQ(1u, all_origin_objects.size());

  device_manager_.RemoveDevice(device_info->guid);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(store->HasDevicePermission(origin, origin, *device_info));
  objects = store->GetGrantedObjects(origin, origin);
  EXPECT_EQ(1u, objects.size());
  all_origin_objects = store->GetAllGrantedObjects();
  EXPECT_EQ(1u, all_origin_objects.size());

  UsbDeviceInfoPtr reconnected_device_info =
      device_manager_.CreateAndAddDevice(0, 0, "Google", "Gizmo", "123ABC");

  EXPECT_TRUE(
      store->HasDevicePermission(origin, origin, *reconnected_device_info));
  objects = store->GetGrantedObjects(origin, origin);
  EXPECT_EQ(1u, objects.size());
  all_origin_objects = store->GetAllGrantedObjects();
  EXPECT_EQ(1u, all_origin_objects.size());
}

TEST_F(UsbChooserContextTest, DisconnectDeviceWithEphemeralPermission) {
  GURL origin("https://www.google.com");
  UsbDeviceInfoPtr device_info =
      device_manager_.CreateAndAddDevice(0, 0, "Google", "Gizmo", "");

  UsbChooserContext* store = GetChooserContext(profile());

  EXPECT_FALSE(store->HasDevicePermission(origin, origin, *device_info));
  store->GrantDevicePermission(origin, origin, *device_info);
  EXPECT_TRUE(store->HasDevicePermission(origin, origin, *device_info));
  std::vector<std::unique_ptr<base::DictionaryValue>> objects =
      store->GetGrantedObjects(origin, origin);
  EXPECT_EQ(1u, objects.size());
  std::vector<std::unique_ptr<ChooserContextBase::Object>> all_origin_objects =
      store->GetAllGrantedObjects();
  EXPECT_EQ(1u, all_origin_objects.size());

  device_manager_.RemoveDevice(device_info->guid);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(store->HasDevicePermission(origin, origin, *device_info));
  objects = store->GetGrantedObjects(origin, origin);
  EXPECT_EQ(0u, objects.size());
  all_origin_objects = store->GetAllGrantedObjects();
  EXPECT_EQ(0u, all_origin_objects.size());

  UsbDeviceInfoPtr reconnected_device_info =
      device_manager_.CreateAndAddDevice(0, 0, "Google", "Gizmo", "");

  EXPECT_FALSE(
      store->HasDevicePermission(origin, origin, *reconnected_device_info));
  objects = store->GetGrantedObjects(origin, origin);
  EXPECT_EQ(0u, objects.size());
  all_origin_objects = store->GetAllGrantedObjects();
  EXPECT_EQ(0u, all_origin_objects.size());
}

TEST_F(UsbChooserContextTest, GrantPermissionInIncognito) {
  GURL origin("https://www.google.com");
  UsbDeviceInfoPtr device_info_1 =
      device_manager_.CreateAndAddDevice(0, 0, "Google", "Gizmo", "");
  UsbDeviceInfoPtr device_info_2 =
      device_manager_.CreateAndAddDevice(0, 0, "Google", "Gizmo", "");
  UsbChooserContext* store = GetChooserContext(profile());
  UsbChooserContext* incognito_store =
      GetChooserContext(profile()->GetOffTheRecordProfile());

  store->GrantDevicePermission(origin, origin, *device_info_1);
  EXPECT_TRUE(store->HasDevicePermission(origin, origin, *device_info_1));
  EXPECT_FALSE(
      incognito_store->HasDevicePermission(origin, origin, *device_info_1));

  incognito_store->GrantDevicePermission(origin, origin, *device_info_2);
  EXPECT_TRUE(store->HasDevicePermission(origin, origin, *device_info_1));
  EXPECT_FALSE(store->HasDevicePermission(origin, origin, *device_info_2));
  EXPECT_FALSE(
      incognito_store->HasDevicePermission(origin, origin, *device_info_1));
  EXPECT_TRUE(
      incognito_store->HasDevicePermission(origin, origin, *device_info_2));

  {
    std::vector<std::unique_ptr<base::DictionaryValue>> objects =
        store->GetGrantedObjects(origin, origin);
    EXPECT_EQ(1u, objects.size());
    std::vector<std::unique_ptr<ChooserContextBase::Object>>
        all_origin_objects = store->GetAllGrantedObjects();
    ASSERT_EQ(1u, all_origin_objects.size());
    EXPECT_FALSE(all_origin_objects[0]->incognito);
  }
  {
    std::vector<std::unique_ptr<base::DictionaryValue>> objects =
        incognito_store->GetGrantedObjects(origin, origin);
    EXPECT_EQ(1u, objects.size());
    std::vector<std::unique_ptr<ChooserContextBase::Object>>
        all_origin_objects = incognito_store->GetAllGrantedObjects();
    ASSERT_EQ(1u, all_origin_objects.size());
    EXPECT_TRUE(all_origin_objects[0]->incognito);
  }
}

TEST_F(UsbChooserContextTest, UsbGuardPermission) {
  const GURL kFooOrigin("https://foo.com");
  const GURL kBarOrigin("https://bar.com");
  UsbDeviceInfoPtr device_info =
      device_manager_.CreateAndAddDevice(0, 0, "Google", "Gizmo", "ABC123");
  UsbDeviceInfoPtr ephemeral_device_info =
      device_manager_.CreateAndAddDevice(0, 0, "Google", "Gizmo", "");

  auto* map = HostContentSettingsMapFactory::GetForProfile(profile());
  map->SetContentSettingDefaultScope(kFooOrigin, kFooOrigin,
                                     CONTENT_SETTINGS_TYPE_USB_GUARD,
                                     std::string(), CONTENT_SETTING_BLOCK);

  auto* store = GetChooserContext(profile());
  store->GrantDevicePermission(kFooOrigin, kFooOrigin, *device_info);
  store->GrantDevicePermission(kFooOrigin, kFooOrigin, *ephemeral_device_info);
  store->GrantDevicePermission(kBarOrigin, kBarOrigin, *device_info);
  store->GrantDevicePermission(kBarOrigin, kBarOrigin, *ephemeral_device_info);

  std::vector<std::unique_ptr<base::DictionaryValue>> objects =
      store->GetGrantedObjects(kFooOrigin, kFooOrigin);
  EXPECT_EQ(0u, objects.size());

  objects = store->GetGrantedObjects(kBarOrigin, kBarOrigin);
  EXPECT_EQ(2u, objects.size());

  std::vector<std::unique_ptr<ChooserContextBase::Object>> all_origin_objects =
      store->GetAllGrantedObjects();
  for (const auto& object : all_origin_objects) {
    EXPECT_EQ(object->requesting_origin, kBarOrigin);
    EXPECT_EQ(object->embedding_origin, kBarOrigin);
  }
  EXPECT_EQ(2u, all_origin_objects.size());

  EXPECT_FALSE(
      store->HasDevicePermission(kFooOrigin, kFooOrigin, *device_info));
  EXPECT_FALSE(store->HasDevicePermission(kFooOrigin, kFooOrigin,
                                          *ephemeral_device_info));
  EXPECT_TRUE(store->HasDevicePermission(kBarOrigin, kBarOrigin, *device_info));
  EXPECT_TRUE(store->HasDevicePermission(kBarOrigin, kBarOrigin,
                                         *ephemeral_device_info));
}

namespace {

constexpr char kPolicySetting[] = R"(
    [
      {
        "devices": [{ "vendor_id": 6353, "product_id": 5678 }],
        "urls": ["https://product.vendor.com"]
      }, {
        "devices": [{ "vendor_id": 6353 }],
        "urls": ["https://vendor.com"]
      }, {
        "devices": [{}],
        "urls": ["https://anydevice.com"]
      }, {
        "devices": [{ "vendor_id": 6354, "product_id": 1357 }],
        "urls": ["https://gadget.com,https://cool.com"]
      }
    ])";

const GURL kPolicyOrigins[] = {
    GURL("https://product.vendor.com"), GURL("https://vendor.com"),
    GURL("https://anydevice.com"), GURL("https://gadget.com"),
    GURL("https://cool.com")};

void ExpectNoPermissions(UsbChooserContext* store,
                         const device::mojom::UsbDeviceInfo& device_info) {
  for (const auto& kRequestingOrigin : kPolicyOrigins) {
    for (const auto& kEmbeddingOrigin : kPolicyOrigins) {
      EXPECT_FALSE(store->HasDevicePermission(kRequestingOrigin,
                                              kEmbeddingOrigin, device_info));
    }
  }
}

void ExpectCorrectPermissions(
    UsbChooserContext* store,
    const std::vector<GURL>& kValidRequestingOrigins,
    const std::vector<GURL>& kInvalidRequestingOrigins,
    const device::mojom::UsbDeviceInfo& device_info) {
  // Ensure that only |kValidRequestingOrigin| as the requesting origin has
  // permission to access the device described by |device_info|.
  for (const auto& kEmbeddingOrigin : kPolicyOrigins) {
    for (const auto& kValidRequestingOrigin : kValidRequestingOrigins) {
      EXPECT_TRUE(store->HasDevicePermission(kValidRequestingOrigin,
                                             kEmbeddingOrigin, device_info));
    }

    for (const auto& kInvalidRequestingOrigin : kInvalidRequestingOrigins) {
      EXPECT_FALSE(store->HasDevicePermission(kInvalidRequestingOrigin,
                                              kEmbeddingOrigin, device_info));
    }
  }
}

}  // namespace

TEST_F(UsbChooserContextTest,
       UsbAllowDevicesForUrlsPermissionForSpecificDevice) {
  const std::vector<GURL> kValidRequestingOrigins = {
      GURL("https://product.vendor.com"), GURL("https://vendor.com"),
      GURL("https://anydevice.com")};
  const std::vector<GURL> kInvalidRequestingOrigins = {
      GURL("https://gadget.com"), GURL("https://cool.com")};

  UsbDeviceInfoPtr specific_device_info = device_manager_.CreateAndAddDevice(
      6353, 5678, "Google", "Gizmo", "ABC123");

  auto* store = GetChooserContext(profile());

  ExpectNoPermissions(store, *specific_device_info);

  profile()->GetPrefs()->Set(prefs::kManagedWebUsbAllowDevicesForUrls,
                             *base::JSONReader::Read(kPolicySetting));

  ExpectCorrectPermissions(store, kValidRequestingOrigins,
                           kInvalidRequestingOrigins, *specific_device_info);
}

TEST_F(UsbChooserContextTest,
       UsbAllowDevicesForUrlsPermissionForVendorRelatedDevice) {
  const std::vector<GURL> kValidRequestingOrigins = {
      GURL("https://vendor.com"), GURL("https://anydevice.com")};
  const std::vector<GURL> kInvalidRequestingOrigins = {
      GURL("https://product.vendor.com"), GURL("https://gadget.com"),
      GURL("https://cool.com")};

  UsbDeviceInfoPtr vendor_related_device_info =
      device_manager_.CreateAndAddDevice(6353, 8765, "Google", "Widget",
                                         "XYZ987");

  auto* store = GetChooserContext(profile());

  ExpectNoPermissions(store, *vendor_related_device_info);

  profile()->GetPrefs()->Set(prefs::kManagedWebUsbAllowDevicesForUrls,
                             *base::JSONReader::Read(kPolicySetting));

  ExpectCorrectPermissions(store, kValidRequestingOrigins,
                           kInvalidRequestingOrigins,
                           *vendor_related_device_info);
}

TEST_F(UsbChooserContextTest,
       UsbAllowDevicesForUrlsPermissionForUnrelatedDevice) {
  const std::vector<GURL> kValidRequestingOrigins = {
      GURL("https://anydevice.com")};
  const std::vector<GURL> kInvalidRequestingOrigins = {
      GURL("https://product.vendor.com"), GURL("https://vendor.com"),
      GURL("https://cool.com")};
  const GURL kGadgetOrigin("https://gadget.com");
  const GURL& kCoolOrigin = kInvalidRequestingOrigins[2];

  UsbDeviceInfoPtr unrelated_device_info = device_manager_.CreateAndAddDevice(
      6354, 1357, "Cool", "Gadget", "4W350M3");

  auto* store = GetChooserContext(profile());

  ExpectNoPermissions(store, *unrelated_device_info);

  profile()->GetPrefs()->Set(prefs::kManagedWebUsbAllowDevicesForUrls,
                             *base::JSONReader::Read(kPolicySetting));

  EXPECT_TRUE(store->HasDevicePermission(kGadgetOrigin, kCoolOrigin,
                                         *unrelated_device_info));
  for (const auto& kEmbeddingOrigin : kPolicyOrigins) {
    if (kEmbeddingOrigin != kCoolOrigin) {
      EXPECT_FALSE(store->HasDevicePermission(kGadgetOrigin, kEmbeddingOrigin,
                                              *unrelated_device_info));
    }
  }
  ExpectCorrectPermissions(store, kValidRequestingOrigins,
                           kInvalidRequestingOrigins, *unrelated_device_info);
}

TEST_F(UsbChooserContextTest,
       UsbAllowDevicesForUrlsPermissionOverrulesUsbGuardPermission) {
  const GURL kProductVendorOrigin("https://product.vendor.com");
  const GURL kGadgetOrigin("https://gadget.com");
  const GURL kCoolOrigin("https://cool.com");

  UsbDeviceInfoPtr specific_device_info = device_manager_.CreateAndAddDevice(
      6353, 5678, "Google", "Gizmo", "ABC123");
  UsbDeviceInfoPtr unrelated_device_info = device_manager_.CreateAndAddDevice(
      6354, 1357, "Cool", "Gadget", "4W350M3");

  auto* store = GetChooserContext(profile());

  ExpectNoPermissions(store, *specific_device_info);
  ExpectNoPermissions(store, *unrelated_device_info);

  auto* map = HostContentSettingsMapFactory::GetForProfile(profile());
  map->SetContentSettingDefaultScope(kProductVendorOrigin, kProductVendorOrigin,
                                     CONTENT_SETTINGS_TYPE_USB_GUARD,
                                     std::string(), CONTENT_SETTING_BLOCK);
  map->SetContentSettingDefaultScope(kGadgetOrigin, kCoolOrigin,
                                     CONTENT_SETTINGS_TYPE_USB_GUARD,
                                     std::string(), CONTENT_SETTING_BLOCK);
  EXPECT_FALSE(store->HasDevicePermission(
      kProductVendorOrigin, kProductVendorOrigin, *specific_device_info));
  EXPECT_FALSE(store->HasDevicePermission(
      kProductVendorOrigin, kProductVendorOrigin, *unrelated_device_info));
  EXPECT_FALSE(store->HasDevicePermission(kGadgetOrigin, kCoolOrigin,
                                          *specific_device_info));
  EXPECT_FALSE(store->HasDevicePermission(kGadgetOrigin, kCoolOrigin,
                                          *unrelated_device_info));

  profile()->GetPrefs()->Set(prefs::kManagedWebUsbAllowDevicesForUrls,
                             *base::JSONReader::Read(kPolicySetting));

  EXPECT_TRUE(store->HasDevicePermission(
      kProductVendorOrigin, kProductVendorOrigin, *specific_device_info));
  EXPECT_FALSE(store->HasDevicePermission(
      kProductVendorOrigin, kProductVendorOrigin, *unrelated_device_info));
  EXPECT_FALSE(store->HasDevicePermission(kGadgetOrigin, kCoolOrigin,
                                          *specific_device_info));
  EXPECT_TRUE(store->HasDevicePermission(kGadgetOrigin, kCoolOrigin,
                                         *unrelated_device_info));
}

namespace {

// Permission sources
constexpr char kPolicySource[] = "policy";
constexpr char kPreferenceSource[] = "preference";

// Test URLs
const GURL kAnyDeviceOrigin("https://anydevice.com");
const GURL kVendorOrigin("https://vendor.com");
const GURL kProductVendorOrigin("https://product.vendor.com");
const GURL kGadgetOrigin("https://gadget.com");
const GURL kCoolOrigin("https://cool.com");

void ExpectDeviceObjectInfo(const base::DictionaryValue& actual,
                            int vendor_id,
                            int product_id,
                            const std::string& name) {
  const base::Value* vendor_id_value =
      actual.FindKeyOfType(kVendorIdKey, base::Value::Type::INTEGER);
  ASSERT_TRUE(vendor_id_value);
  EXPECT_EQ(vendor_id_value->GetInt(), vendor_id);

  const base::Value* product_id_value =
      actual.FindKeyOfType(kProductIdKey, base::Value::Type::INTEGER);
  ASSERT_TRUE(product_id_value);
  EXPECT_EQ(product_id_value->GetInt(), product_id);

  const base::Value* device_name_value =
      actual.FindKeyOfType(kDeviceNameKey, base::Value::Type::STRING);
  ASSERT_TRUE(device_name_value);
  EXPECT_EQ(device_name_value->GetString(), name);
}

void ExpectChooserObjectInfo(const ChooserContextBase::Object* actual,
                             const GURL& requesting_origin,
                             const GURL& embedding_origin,
                             const std::string& source,
                             bool incognito,
                             int vendor_id,
                             int product_id,
                             const std::string& name) {
  ASSERT_TRUE(actual);
  EXPECT_EQ(actual->requesting_origin, requesting_origin);
  EXPECT_EQ(actual->embedding_origin, embedding_origin);
  EXPECT_EQ(actual->source, source);
  EXPECT_EQ(actual->incognito, incognito);
  ExpectDeviceObjectInfo(actual->object, vendor_id, product_id, name);
}

void ExpectChooserObjectInfo(const ChooserContextBase::Object* actual,
                             const GURL& requesting_origin,
                             const std::string& source,
                             bool incognito,
                             int vendor_id,
                             int product_id,
                             const std::string& name) {
  ExpectChooserObjectInfo(actual, requesting_origin, GURL::EmptyGURL(), source,
                          incognito, vendor_id, product_id, name);
}

}  // namespace

TEST_F(UsbChooserContextTest,
       GetAllGrantedObjectsWithOnlyPolicyAllowedDevices) {
  auto* store = GetChooserContext(profile());
  profile()->GetPrefs()->Set(prefs::kManagedWebUsbAllowDevicesForUrls,
                             *base::JSONReader::Read(kPolicySetting));

  auto objects = store->GetAllGrantedObjects();
  ASSERT_EQ(objects.size(), 4ul);

  // The policy enforced objects that are returned by GetAllGrantedObjects() are
  // ordered by the tuple (vendor_id, product_id) representing the device IDs.
  // Wildcard IDs are represented by a value of -1, so they appear first.
  ExpectChooserObjectInfo(objects[0].get(),
                          /*requesting_origin=*/kAnyDeviceOrigin,
                          /*source=*/kPolicySource,
                          /*incognito=*/false,
                          /*vendor_id=*/kDeviceIdWildcard,
                          /*product_id=*/kDeviceIdWildcard,
                          /*name=*/"Unknown device [ffffffff:ffffffff]");
  ExpectChooserObjectInfo(objects[1].get(),
                          /*requesting_origin=*/kVendorOrigin,
                          /*source=*/kPolicySource,
                          /*incognito=*/false,
                          /*vendor_id=*/6353,
                          /*product_id=*/kDeviceIdWildcard,
                          /*name=*/"Unknown device from Google Inc.");
  ExpectChooserObjectInfo(objects[2].get(),
                          /*requesting_origin=*/kProductVendorOrigin,
                          /*source=*/kPolicySource,
                          /*incognito=*/false,
                          /*vendor_id=*/6353,
                          /*product_id=*/5678,
                          /*name=*/"Unknown device from Google Inc.");
  ExpectChooserObjectInfo(objects[3].get(),
                          /*requesting_origin=*/kGadgetOrigin,
                          /*embedding_origin=*/kCoolOrigin,
                          /*source=*/kPolicySource,
                          /*incognito=*/false,
                          /*vendor_id=*/6354,
                          /*product_id=*/1357,
                          /*name=*/"Unknown device [18d2:054d]");
}

TEST_F(UsbChooserContextTest,
       GetAllGrantedObjectsWithUserAndPolicyAllowedDevices) {
  profile()->GetPrefs()->Set(prefs::kManagedWebUsbAllowDevicesForUrls,
                             *base::JSONReader::Read(kPolicySetting));

  const GURL kGoogleOrigin("https://www.google.com");
  UsbDeviceInfoPtr persistent_device_info =
      device_manager_.CreateAndAddDevice(1000, 1, "Google", "Gizmo", "123ABC");
  UsbDeviceInfoPtr ephemeral_device_info =
      device_manager_.CreateAndAddDevice(1000, 2, "Google", "Gadget", "");

  auto* store = GetChooserContext(profile());
  store->GrantDevicePermission(kGoogleOrigin, kGoogleOrigin,
                               *persistent_device_info);
  store->GrantDevicePermission(kGoogleOrigin, kGoogleOrigin,
                               *ephemeral_device_info);

  auto objects = store->GetAllGrantedObjects();
  ASSERT_EQ(objects.size(), 6ul);

  for (const auto& object : objects) {
    EXPECT_TRUE(store->IsValidObject(object->object));
  }

  // The user granted permissions appear before the policy granted permissions.
  // Within the user granted permissions, the persistent device permissions
  // are added to the vector before ephemeral device permissions.
  ExpectChooserObjectInfo(objects[0].get(),
                          /*requesting_origin=*/kGoogleOrigin,
                          /*embedding_origin=*/kGoogleOrigin,
                          /*source=*/kPreferenceSource,
                          /*incognito=*/false,
                          /*vendor_id=*/1000,
                          /*product_id=*/1,
                          /*name=*/"Gizmo");
  ExpectChooserObjectInfo(objects[1].get(),
                          /*requesting_origin=*/kGoogleOrigin,
                          /*embedding_origin=*/kGoogleOrigin,
                          /*source=*/kPreferenceSource,
                          /*incognito=*/false,
                          /*vendor_id=*/1000,
                          /*product_id=*/2,
                          /*name=*/"Gadget");
  ExpectChooserObjectInfo(objects[2].get(),
                          /*requesting_origin=*/kAnyDeviceOrigin,
                          /*source=*/kPolicySource,
                          /*incognito=*/false,
                          /*vendor_id=*/kDeviceIdWildcard,
                          /*product_id=*/kDeviceIdWildcard,
                          /*name=*/"Unknown device [ffffffff:ffffffff]");
  ExpectChooserObjectInfo(objects[3].get(),
                          /*requesting_origin=*/kVendorOrigin,
                          /*source=*/kPolicySource,
                          /*incognito=*/false,
                          /*vendor_id=*/6353,
                          /*product_id=*/kDeviceIdWildcard,
                          /*name=*/"Unknown device from Google Inc.");
  ExpectChooserObjectInfo(objects[4].get(),
                          /*requesting_origin=*/kProductVendorOrigin,
                          /*source=*/kPolicySource,
                          /*incognito=*/false,
                          /*vendor_id=*/6353,
                          /*product_id=*/5678,
                          /*name=*/"Unknown device from Google Inc.");
  ExpectChooserObjectInfo(objects[5].get(),
                          /*requesting_origin=*/kGadgetOrigin,
                          /*embedding_origin=*/kCoolOrigin,
                          /*source=*/kPolicySource,
                          /*incognito=*/false,
                          /*vendor_id=*/6354,
                          /*product_id=*/1357,
                          /*name=*/"Unknown device [18d2:054d]");
}

TEST_F(UsbChooserContextTest,
       GetAllGrantedObjectsWithSpecificPolicyAndUserGrantedDevice) {
  auto* store = GetChooserContext(profile());
  profile()->GetPrefs()->Set(prefs::kManagedWebUsbAllowDevicesForUrls,
                             *base::JSONReader::Read(kPolicySetting));

  const GURL kProductVendorOrigin("https://product.vendor.com");
  UsbDeviceInfoPtr persistent_device_info = device_manager_.CreateAndAddDevice(
      6353, 5678, "Specific", "Product", "123ABC");

  store->GrantDevicePermission(kProductVendorOrigin, kProductVendorOrigin,
                               *persistent_device_info);

  auto objects = store->GetAllGrantedObjects();
  ASSERT_EQ(objects.size(), 4ul);

  for (const auto& object : objects) {
    EXPECT_TRUE(store->IsValidObject(object->object));
  }

  ExpectChooserObjectInfo(objects[0].get(),
                          /*requesting_origin=*/kAnyDeviceOrigin,
                          /*source=*/kPolicySource,
                          /*incognito=*/false,
                          /*vendor_id=*/kDeviceIdWildcard,
                          /*product_id=*/kDeviceIdWildcard,
                          /*name=*/"Unknown device [ffffffff:ffffffff]");
  ExpectChooserObjectInfo(objects[1].get(),
                          /*requesting_origin=*/kVendorOrigin,
                          /*source=*/kPolicySource,
                          /*incognito=*/false,
                          /*vendor_id=*/6353,
                          /*product_id=*/kDeviceIdWildcard,
                          /*name=*/"Unknown device from Google Inc.");
  ExpectChooserObjectInfo(objects[2].get(),
                          /*requesting_origin=*/kProductVendorOrigin,
                          /*source=*/kPolicySource,
                          /*incognito=*/false,
                          /*vendor_id=*/6353,
                          /*product_id=*/5678,
                          /*name=*/"Product");
  ExpectChooserObjectInfo(objects[3].get(),
                          /*requesting_origin=*/kGadgetOrigin,
                          /*embedding_origin=*/kCoolOrigin,
                          /*source=*/kPolicySource,
                          /*incognito=*/false,
                          /*vendor_id=*/6354,
                          /*product_id=*/1357,
                          /*name=*/"Unknown device [18d2:054d]");
  ASSERT_TRUE(persistent_device_info->product_name);
  EXPECT_EQ(base::UTF8ToUTF16(store->GetObjectName(objects[2]->object)),
            persistent_device_info->product_name.value());
}

TEST_F(UsbChooserContextTest,
       GetAllGrantedObjectsWithVendorPolicyAndUserGrantedDevice) {
  const GURL kVendorOrigin("https://vendor.com");
  UsbDeviceInfoPtr persistent_device_info = device_manager_.CreateAndAddDevice(
      6353, 1000, "Vendor", "Product", "123ABC");

  auto* store = GetChooserContext(profile());
  profile()->GetPrefs()->Set(prefs::kManagedWebUsbAllowDevicesForUrls,
                             *base::JSONReader::Read(kPolicySetting));

  store->GrantDevicePermission(kVendorOrigin, kVendorOrigin,
                               *persistent_device_info);

  auto objects = store->GetAllGrantedObjects();
  ASSERT_EQ(objects.size(), 4ul);

  for (const auto& object : objects) {
    EXPECT_TRUE(store->IsValidObject(object->object));
  }

  ExpectChooserObjectInfo(objects[0].get(),
                          /*requesting_origin=*/kAnyDeviceOrigin,
                          /*source=*/kPolicySource,
                          /*incognito=*/false,
                          /*vendor_id=*/kDeviceIdWildcard,
                          /*product_id=*/kDeviceIdWildcard,
                          /*name=*/"Unknown device [ffffffff:ffffffff]");
  ExpectChooserObjectInfo(objects[1].get(),
                          /*requesting_origin=*/kVendorOrigin,
                          /*source=*/kPolicySource,
                          /*incognito=*/false,
                          /*vendor_id=*/6353,
                          /*product_id=*/kDeviceIdWildcard,
                          /*name=*/"Unknown device from Google Inc.");
  ExpectChooserObjectInfo(objects[2].get(),
                          /*requesting_origin=*/kProductVendorOrigin,
                          /*source=*/kPolicySource,
                          /*incognito=*/false,
                          /*vendor_id=*/6353,
                          /*product_id=*/5678,
                          /*name=*/"Unknown device from Google Inc.");
  ExpectChooserObjectInfo(objects[3].get(),
                          /*requesting_origin=*/kGadgetOrigin,
                          /*embedding_origin=*/kCoolOrigin,
                          /*source=*/kPolicySource,
                          /*incognito=*/false,
                          /*vendor_id=*/6354,
                          /*product_id=*/1357,
                          /*name=*/"Unknown device [18d2:054d]");
}

TEST_F(UsbChooserContextTest,
       GetAllGrantedObjectsWithAnyPolicyAndUserGrantedDevice) {
  const GURL kAnyDeviceOrigin("https://anydevice.com");
  UsbDeviceInfoPtr persistent_device_info = device_manager_.CreateAndAddDevice(
      1123, 5813, "Some", "Product", "123ABC");

  auto* store = GetChooserContext(profile());
  profile()->GetPrefs()->Set(prefs::kManagedWebUsbAllowDevicesForUrls,
                             *base::JSONReader::Read(kPolicySetting));

  store->GrantDevicePermission(kAnyDeviceOrigin, kAnyDeviceOrigin,
                               *persistent_device_info);

  auto objects = store->GetAllGrantedObjects();
  ASSERT_EQ(objects.size(), 4ul);

  for (const auto& object : objects) {
    EXPECT_TRUE(store->IsValidObject(object->object));
  }

  ExpectChooserObjectInfo(objects[0].get(),
                          /*requesting_origin=*/kAnyDeviceOrigin,
                          /*source=*/kPolicySource,
                          /*incognito=*/false,
                          /*vendor_id=*/kDeviceIdWildcard,
                          /*product_id=*/kDeviceIdWildcard,
                          /*name=*/"Unknown device [ffffffff:ffffffff]");
  ExpectChooserObjectInfo(objects[1].get(),
                          /*requesting_origin=*/kVendorOrigin,
                          /*source=*/kPolicySource,
                          /*incognito=*/false,
                          /*vendor_id=*/6353,
                          /*product_id=*/kDeviceIdWildcard,
                          /*name=*/"Unknown device from Google Inc.");
  ExpectChooserObjectInfo(objects[2].get(),
                          /*requesting_origin=*/kProductVendorOrigin,
                          /*source=*/kPolicySource,
                          /*incognito=*/false,
                          /*vendor_id=*/6353,
                          /*product_id=*/5678,
                          /*name=*/"Unknown device from Google Inc.");
  ExpectChooserObjectInfo(objects[3].get(),
                          /*requesting_origin=*/kGadgetOrigin,
                          /*embedding_origin=*/kCoolOrigin,
                          /*source=*/kPolicySource,
                          /*incognito=*/false,
                          /*vendor_id=*/6354,
                          /*product_id=*/1357,
                          /*name=*/"Unknown device [18d2:054d]");
}
