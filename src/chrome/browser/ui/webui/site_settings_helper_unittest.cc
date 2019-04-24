// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/site_settings_helper.h"

#include "base/bind_helpers.h"
#include "base/guid.h"
#include "base/json/json_reader.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/permissions/chooser_context_base.h"
#include "chrome/browser/usb/usb_chooser_context.h"
#include "chrome/browser/usb/usb_chooser_context_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/content_settings/core/test/content_settings_mock_provider.h"
#include "components/content_settings/core/test/content_settings_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "device/usb/public/cpp/fake_usb_device_manager.h"
#include "extensions/browser/extension_registry.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace site_settings {

namespace {
constexpr ContentSettingsType kContentType = CONTENT_SETTINGS_TYPE_GEOLOCATION;
}

class SiteSettingsHelperTest : public testing::Test {
 public:
  void VerifySetting(const base::ListValue& exceptions,
                     int index,
                     const std::string& pattern,
                     const std::string& pattern_display_name,
                     const ContentSetting setting) {
    const base::DictionaryValue* dict;
    exceptions.GetDictionary(index, &dict);
    std::string actual_pattern;
    dict->GetString("origin", &actual_pattern);
    EXPECT_EQ(pattern, actual_pattern);
    std::string actual_display_name;
    dict->GetString(kDisplayName, &actual_display_name);
    EXPECT_EQ(pattern_display_name, actual_display_name);
    std::string actual_setting;
    dict->GetString(kSetting, &actual_setting);
    EXPECT_EQ(content_settings::ContentSettingToString(setting),
              actual_setting);
  }

  void AddSetting(HostContentSettingsMap* map,
                  const std::string& pattern,
                  ContentSetting setting) {
    map->SetContentSettingCustomScope(
        ContentSettingsPattern::FromString(pattern),
        ContentSettingsPattern::Wildcard(), kContentType, std::string(),
        setting);
  }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
};

TEST_F(SiteSettingsHelperTest, CheckExceptionOrder) {
  TestingProfile profile;
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(&profile);

  base::ListValue exceptions;
  // Check that the initial state of the map is empty.
  GetExceptionsFromHostContentSettingsMap(
      map, kContentType, /*extension_registry=*/nullptr, /*web_ui=*/nullptr,
      /*incognito=*/false, /*filter=*/nullptr, &exceptions);
  EXPECT_EQ(0u, exceptions.GetSize());

  map->SetDefaultContentSetting(kContentType, CONTENT_SETTING_ALLOW);

  // Add a policy exception.
  std::string star_google_com = "http://[*.]google.com";
  auto policy_provider = std::make_unique<content_settings::MockProvider>();
  policy_provider->SetWebsiteSetting(
      ContentSettingsPattern::FromString(star_google_com),
      ContentSettingsPattern::Wildcard(), kContentType, "",
      new base::Value(CONTENT_SETTING_BLOCK));
  policy_provider->set_read_only(true);
  content_settings::TestUtils::OverrideProvider(
      map, std::move(policy_provider), HostContentSettingsMap::POLICY_PROVIDER);

  // Add user preferences.
  std::string http_star = "http://*";
  std::string maps_google_com = "http://maps.google.com";
  AddSetting(map, http_star, CONTENT_SETTING_BLOCK);
  AddSetting(map, maps_google_com, CONTENT_SETTING_BLOCK);
  AddSetting(map, star_google_com, CONTENT_SETTING_ALLOW);

  // Add an extension exception.
  std::string drive_google_com = "http://drive.google.com";
  auto extension_provider = std::make_unique<content_settings::MockProvider>();
  extension_provider->SetWebsiteSetting(
      ContentSettingsPattern::FromString(drive_google_com),
      ContentSettingsPattern::Wildcard(), kContentType, "",
      new base::Value(CONTENT_SETTING_ASK));
  extension_provider->set_read_only(true);
  content_settings::TestUtils::OverrideProvider(
      map, std::move(extension_provider),
      HostContentSettingsMap::CUSTOM_EXTENSION_PROVIDER);

  exceptions.Clear();
  GetExceptionsFromHostContentSettingsMap(
      map, kContentType, /*extension_registry=*/nullptr, /*web_ui=*/nullptr,
      /*incognito=*/false, /*filter=*/nullptr, &exceptions);

  EXPECT_EQ(5u, exceptions.GetSize());

  // The policy exception should be returned first, the extension exception
  // second and pref exceptions afterwards.
  // The default content setting should not be returned.
  int i = 0;
  // From policy provider:
  VerifySetting(exceptions, i++, star_google_com, star_google_com,
                CONTENT_SETTING_BLOCK);
  // From extension provider:
  VerifySetting(exceptions, i++, drive_google_com, drive_google_com,
                CONTENT_SETTING_ASK);
  // From user preferences:
  VerifySetting(exceptions, i++, maps_google_com, maps_google_com,
                CONTENT_SETTING_BLOCK);
  VerifySetting(exceptions, i++, star_google_com, star_google_com,
                CONTENT_SETTING_ALLOW);
  VerifySetting(exceptions, i++, http_star, "http://*", CONTENT_SETTING_BLOCK);
}

// Tests the following content setting sources: Chrome default, user-set global
// default, user-set pattern, user-set origin setting, extension, and policy.
TEST_F(SiteSettingsHelperTest, ContentSettingSource) {
  TestingProfile profile;
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(&profile);

  GURL origin("https://www.example.com/");
  auto* extension_registry = extensions::ExtensionRegistry::Get(&profile);
  std::string source;
  std::string display_name;
  ContentSetting content_setting;

  // Built in Chrome default.
  content_setting =
      GetContentSettingForOrigin(&profile, map, origin, kContentType, &source,
                                 extension_registry, &display_name);
  EXPECT_EQ(SiteSettingSourceToString(SiteSettingSource::kDefault), source);
  EXPECT_EQ(CONTENT_SETTING_ASK, content_setting);

  // User-set global default.
  map->SetDefaultContentSetting(kContentType, CONTENT_SETTING_ALLOW);
  content_setting =
      GetContentSettingForOrigin(&profile, map, origin, kContentType, &source,
                                 extension_registry, &display_name);
  EXPECT_EQ(SiteSettingSourceToString(SiteSettingSource::kDefault), source);
  EXPECT_EQ(CONTENT_SETTING_ALLOW, content_setting);

  // User-set pattern.
  AddSetting(map, "https://*", CONTENT_SETTING_BLOCK);
  content_setting =
      GetContentSettingForOrigin(&profile, map, origin, kContentType, &source,
                                 extension_registry, &display_name);
  EXPECT_EQ(SiteSettingSourceToString(SiteSettingSource::kPreference), source);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, content_setting);

  // User-set origin setting.
  map->SetContentSettingDefaultScope(origin, origin, kContentType,
                                     std::string(), CONTENT_SETTING_ALLOW);
  content_setting =
      GetContentSettingForOrigin(&profile, map, origin, kContentType, &source,
                                 extension_registry, &display_name);
  EXPECT_EQ(SiteSettingSourceToString(SiteSettingSource::kPreference), source);
  EXPECT_EQ(CONTENT_SETTING_ALLOW, content_setting);

// ChromeOS - DRM disabled.
#if defined(OS_CHROMEOS)
  profile.GetPrefs()->SetBoolean(prefs::kEnableDRM, false);
  // Note this is not testing |kContentType|, because this setting is only valid
  // for protected content.
  content_setting = GetContentSettingForOrigin(
      &profile, map, origin, CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER,
      &source, extension_registry, &display_name);
  EXPECT_EQ(SiteSettingSourceToString(SiteSettingSource::kDrmDisabled), source);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, content_setting);
#endif

  // Extension.
  auto extension_provider = std::make_unique<content_settings::MockProvider>();
  extension_provider->SetWebsiteSetting(ContentSettingsPattern::FromURL(origin),
                                        ContentSettingsPattern::FromURL(origin),
                                        kContentType, "",
                                        new base::Value(CONTENT_SETTING_BLOCK));
  extension_provider->set_read_only(true);
  content_settings::TestUtils::OverrideProvider(
      map, std::move(extension_provider),
      HostContentSettingsMap::CUSTOM_EXTENSION_PROVIDER);
  content_setting =
      GetContentSettingForOrigin(&profile, map, origin, kContentType, &source,
                                 extension_registry, &display_name);
  EXPECT_EQ(SiteSettingSourceToString(SiteSettingSource::kExtension), source);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, content_setting);

  // Enterprise policy.
  auto policy_provider = std::make_unique<content_settings::MockProvider>();
  policy_provider->SetWebsiteSetting(ContentSettingsPattern::FromURL(origin),
                                     ContentSettingsPattern::FromURL(origin),
                                     kContentType, "",
                                     new base::Value(CONTENT_SETTING_ALLOW));
  policy_provider->set_read_only(true);
  content_settings::TestUtils::OverrideProvider(
      map, std::move(policy_provider), HostContentSettingsMap::POLICY_PROVIDER);
  content_setting =
      GetContentSettingForOrigin(&profile, map, origin, kContentType, &source,
                                 extension_registry, &display_name);
  EXPECT_EQ(SiteSettingSourceToString(SiteSettingSource::kPolicy), source);
  EXPECT_EQ(CONTENT_SETTING_ALLOW, content_setting);

  // Insecure origins.
  content_setting = GetContentSettingForOrigin(
      &profile, map, GURL("http://www.insecure_http_site.com/"), kContentType,
      &source, extension_registry, &display_name);
  EXPECT_EQ(SiteSettingSourceToString(SiteSettingSource::kInsecureOrigin),
            source);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, content_setting);
}

namespace {

// Test GURLs
const GURL kGoogleOrigin("https://google.com");
const GURL kChromiumOrigin("https://chromium.org");
const GURL kAndroidOrigin("https://android.com");

void ExpectValidChooserExceptionObject(
    const base::Value& actual_exception_object,
    const std::string& chooser_type,
    const std::string& display_name,
    const base::Value& chooser_object) {
  const base::Value* chooser_type_value = actual_exception_object.FindKeyOfType(
      kChooserType, base::Value::Type::STRING);
  ASSERT_TRUE(chooser_type_value);
  EXPECT_EQ(chooser_type_value->GetString(), chooser_type);

  const base::Value* display_name_value = actual_exception_object.FindKeyOfType(
      kDisplayName, base::Value::Type::STRING);
  ASSERT_TRUE(display_name_value);
  EXPECT_EQ(display_name_value->GetString(), display_name);

  const base::Value* object_value = actual_exception_object.FindKeyOfType(
      kObject, base::Value::Type::DICTIONARY);
  ASSERT_TRUE(object_value);
  EXPECT_EQ(*object_value, chooser_object);

  const base::Value* sites_value =
      actual_exception_object.FindKeyOfType(kSites, base::Value::Type::LIST);
  ASSERT_TRUE(sites_value);
}

void ExpectValidSiteExceptionObject(const base::Value& actual_site_object,
                                    const GURL& origin,
                                    const GURL& embedding_origin,
                                    const std::string source,
                                    bool incognito) {
  ASSERT_TRUE(actual_site_object.is_dict());

  const base::Value* display_name_value =
      actual_site_object.FindKeyOfType(kDisplayName, base::Value::Type::STRING);
  ASSERT_TRUE(display_name_value);
  EXPECT_EQ(display_name_value->GetString(), origin.GetOrigin().spec());

  const base::Value* origin_value =
      actual_site_object.FindKeyOfType(kOrigin, base::Value::Type::STRING);
  ASSERT_TRUE(origin_value);
  EXPECT_EQ(origin_value->GetString(), origin.GetOrigin().spec());

  const base::Value* embedding_origin_value = actual_site_object.FindKeyOfType(
      kEmbeddingOrigin, base::Value::Type::STRING);
  ASSERT_TRUE(embedding_origin_value);
  EXPECT_EQ(embedding_origin_value->GetString(),
            embedding_origin.GetOrigin().spec());

  const base::Value* setting_value =
      actual_site_object.FindKeyOfType(kSetting, base::Value::Type::STRING);
  ASSERT_TRUE(setting_value);
  EXPECT_EQ(setting_value->GetString(),
            content_settings::ContentSettingToString(CONTENT_SETTING_DEFAULT));

  const base::Value* source_value =
      actual_site_object.FindKeyOfType(kSource, base::Value::Type::STRING);
  ASSERT_TRUE(source_value);
  EXPECT_EQ(source_value->GetString(), source);

  const base::Value* incognito_value =
      actual_site_object.FindKeyOfType(kIncognito, base::Value::Type::BOOLEAN);
  ASSERT_TRUE(incognito_value);
  EXPECT_EQ(incognito_value->GetBool(), incognito);
}

void ExpectValidSiteExceptionObject(const base::Value& actual_site_object,
                                    const GURL& origin,
                                    const std::string source,
                                    bool incognito) {
  ExpectValidSiteExceptionObject(actual_site_object, origin, GURL::EmptyGURL(),
                                 source, incognito);
}

}  // namespace

TEST_F(SiteSettingsHelperTest, CreateChooserExceptionObject) {
  const std::string kUsbChooserGroupName =
      ContentSettingsTypeToGroupName(CONTENT_SETTINGS_TYPE_USB_CHOOSER_DATA);
  const std::string& kPolicySource =
      SiteSettingSourceToString(SiteSettingSource::kPolicy);
  const std::string& kPreferenceSource =
      SiteSettingSourceToString(SiteSettingSource::kPreference);
  const char kObjectName[] = "Gadget";
  ChooserExceptionDetails exception_details;

  // Create a chooser object for testing.
  auto chooser_object = std::make_unique<base::DictionaryValue>();
  chooser_object->SetKey("name", base::Value(kObjectName));

  // Add a user permission for a requesting origin of |kGoogleOrigin| and an
  // embedding origin of |kChromiumOrigin|.
  exception_details[std::make_pair(kGoogleOrigin.GetOrigin(),
                                   kPreferenceSource)]
      .insert(std::make_pair(kChromiumOrigin.GetOrigin(), /*incognito=*/false));

  {
    auto exception = CreateChooserExceptionObject(
        /*display_name=*/kObjectName,
        /*object=*/*chooser_object,
        /*chooser_type=*/kUsbChooserGroupName,
        /*chooser_exception_details=*/exception_details);
    ExpectValidChooserExceptionObject(
        *exception, /*chooser_type=*/kUsbChooserGroupName,
        /*display_name=*/kObjectName, *chooser_object);

    const auto& sites_list = exception->FindKey(kSites)->GetList();
    ExpectValidSiteExceptionObject(/*actual_site_object=*/sites_list[0],
                                   /*origin=*/kGoogleOrigin,
                                   /*embedding_origin=*/kChromiumOrigin,
                                   /*source=*/kPreferenceSource,
                                   /*incognito=*/false);
  }

  // Add a user permissions for a requesting and embedding origin pair of
  // |kAndroidOrigin| granted in an off the record profile.
  exception_details[std::make_pair(kAndroidOrigin.GetOrigin(),
                                   kPreferenceSource)]
      .insert(std::make_pair(kAndroidOrigin.GetOrigin(), /*incognito=*/true));

  {
    auto exception = CreateChooserExceptionObject(
        /*display_name=*/kObjectName,
        /*object=*/*chooser_object,
        /*chooser_type=*/kUsbChooserGroupName,
        /*chooser_exception_details=*/exception_details);
    ExpectValidChooserExceptionObject(*exception,
                                      /*chooser_type=*/kUsbChooserGroupName,
                                      /*display_name=*/kObjectName,
                                      *chooser_object);

    // The map sorts the sites by requesting origin, so |kAndroidOrigin| should
    // be first, followed by the origin pair (kGoogleOrigin, kChromiumOrigin).
    const auto& sites_list = exception->FindKey(kSites)->GetList();
    ExpectValidSiteExceptionObject(/*actual_site_object=*/sites_list[0],
                                   /*origin=*/kAndroidOrigin,
                                   /*embedding_origin=*/kAndroidOrigin,
                                   /*source=*/kPreferenceSource,
                                   /*incognito=*/true);
    ExpectValidSiteExceptionObject(/*actual_site_object=*/sites_list[1],
                                   /*origin=*/kGoogleOrigin,
                                   /*embedding_origin=*/kChromiumOrigin,
                                   /*source=*/kPreferenceSource,
                                   /*incognito=*/false);
  }

  // Add a policy permission for a requesting origin of |kGoogleOrigin| with a
  // wildcard embedding origin.
  exception_details[std::make_pair(kGoogleOrigin.GetOrigin(), kPolicySource)]
      .insert(std::make_pair(GURL::EmptyGURL(), /*incognito=*/false));
  {
    auto exception = CreateChooserExceptionObject(
        /*display_name=*/kObjectName,
        /*object=*/*chooser_object,
        /*chooser_type=*/kUsbChooserGroupName,
        /*chooser_exception_details=*/exception_details);
    ExpectValidChooserExceptionObject(*exception,
                                      /*chooser_type=*/kUsbChooserGroupName,
                                      /*display_name=*/kObjectName,
                                      *chooser_object);

    // The map sorts the sites by requesting origin, but the
    // CreateChooserExceptionObject method sorts the sites further by the
    // source. Therefore, policy granted sites are listed before user granted
    // sites.
    const auto& sites_list = exception->FindKey(kSites)->GetList();
    ExpectValidSiteExceptionObject(/*actual_site_object=*/sites_list[0],
                                   /*origin=*/kGoogleOrigin,
                                   /*embedding_origin=*/GURL::EmptyGURL(),
                                   /*source=*/kPolicySource,
                                   /*incognito=*/false);
    ExpectValidSiteExceptionObject(/*actual_site_object=*/sites_list[1],
                                   /*origin=*/kAndroidOrigin,
                                   /*embedding_origin=*/kAndroidOrigin,
                                   /*source=*/kPreferenceSource,
                                   /*incognito=*/true);
    ExpectValidSiteExceptionObject(/*actual_site_object=*/sites_list[2],
                                   /*origin=*/kGoogleOrigin,
                                   /*embedding_origin=*/kChromiumOrigin,
                                   /*source=*/kPreferenceSource,
                                   /*incognito=*/false);
  }
}

namespace {

constexpr char kUsbPolicySetting[] = R"(
    [
      {
        "devices": [{ "vendor_id": 6353, "product_id": 5678 }],
        "urls": ["https://chromium.org"]
      }, {
        "devices": [{ "vendor_id": 6353 }],
        "urls": ["https://google.com,https://android.com"]
      }, {
        "devices": [{ "vendor_id": 6354 }],
        "urls": ["https://android.com,"]
      }, {
        "devices": [{}],
        "urls": ["https://google.com,https://google.com"]
      }
    ])";

class SiteSettingsHelperChooserExceptionTest : public testing::Test {
 protected:
  Profile* profile() { return &profile_; }

  void SetUp() override { SetUpUsbChooserContext(); }

  // Sets up the UsbChooserContext with two devices and permissions for these
  // devices. It also adds three policy defined permissions. The two devices
  // represent the two types of USB devices, persistent and ephemeral, that can
  // be granted permission.
  void SetUpUsbChooserContext() {
    device::mojom::UsbDeviceInfoPtr persistent_device_info =
        device_manager_.CreateAndAddDevice(6353, 5678, "Google", "Gizmo",
                                           "123ABC");
    device::mojom::UsbDeviceInfoPtr ephemeral_device_info =
        device_manager_.CreateAndAddDevice(6354, 0, "Google", "Gadget", "");

    auto* chooser_context = UsbChooserContextFactory::GetForProfile(profile());
    device::mojom::UsbDeviceManagerPtr device_manager_ptr;
    device_manager_.AddBinding(mojo::MakeRequest(&device_manager_ptr));
    chooser_context->SetDeviceManagerForTesting(std::move(device_manager_ptr));
    chooser_context->GetDevices(
        base::DoNothing::Once<std::vector<device::mojom::UsbDeviceInfoPtr>>());
    base::RunLoop().RunUntilIdle();

    // Add the user granted permissions for testing.
    // These two persistent device permissions should be lumped together with
    // the policy permissions, since they apply to the same device and URL.
    chooser_context->GrantDevicePermission(kChromiumOrigin, kChromiumOrigin,
                                           *persistent_device_info);
    chooser_context->GrantDevicePermission(kChromiumOrigin, kGoogleOrigin,
                                           *persistent_device_info);
    chooser_context->GrantDevicePermission(kAndroidOrigin, kChromiumOrigin,
                                           *persistent_device_info);
    chooser_context->GrantDevicePermission(kAndroidOrigin, kAndroidOrigin,
                                           *ephemeral_device_info);

    // Add the policy granted permissions for testing.
    auto policy_value = base::JSONReader::ReadDeprecated(kUsbPolicySetting);
    DCHECK(policy_value);
    profile()->GetPrefs()->Set(prefs::kManagedWebUsbAllowDevicesForUrls,
                               *policy_value);
  }

  device::FakeUsbDeviceManager device_manager_;

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;
};

void ExpectDisplayNameEq(const base::Value& actual_exception_object,
                         const std::string& display_name) {
  const base::Value* display_name_value = actual_exception_object.FindKeyOfType(
      kDisplayName, base::Value::Type::STRING);
  ASSERT_TRUE(display_name_value);
  EXPECT_EQ(display_name_value->GetString(), display_name);
}

}  // namespace

TEST_F(SiteSettingsHelperChooserExceptionTest,
       GetChooserExceptionListFromProfile) {
  const std::string kUsbChooserGroupName =
      ContentSettingsTypeToGroupName(CONTENT_SETTINGS_TYPE_USB_CHOOSER_DATA);
  const ChooserTypeNameEntry* chooser_type =
      ChooserTypeFromGroupName(kUsbChooserGroupName);
  const std::string& kPolicySource =
      SiteSettingSourceToString(SiteSettingSource::kPolicy);
  const std::string& kPreferenceSource =
      SiteSettingSourceToString(SiteSettingSource::kPreference);

  // The chooser exceptions are ordered by display name. Their corresponding
  // sites are ordered by permission source precedence, then by the requesting
  // origin and the embedding origin. User granted permissions that are also
  // granted by policy are combined with the policy so that duplicate
  // permissions are not displayed.
  std::unique_ptr<base::ListValue> exceptions =
      GetChooserExceptionListFromProfile(profile(), *chooser_type);
  ASSERT_EQ(exceptions->GetSize(), 4u);
  auto& exceptions_list = exceptions->GetList();

  // This exception should describe the permissions for any device with the
  // vendor ID corresponding to "Google Inc.". There are no user granted
  // permissions that intersect with this permission, and this policy only
  // grants one permission to the following site pair:
  // * ("https://google.com", "https://android.com")
  {
    const auto& exception = exceptions_list[0];
    ExpectDisplayNameEq(exception,
                        /*display_name=*/"Devices from Google Inc.");

    const auto& sites_list = exception.FindKey(kSites)->GetList();
    ASSERT_EQ(sites_list.size(), 1u);
    ExpectValidSiteExceptionObject(sites_list[0],
                                   /*origin=*/kGoogleOrigin,
                                   /*embedding_origin=*/kAndroidOrigin,
                                   /*source=*/kPolicySource,
                                   /*incognito=*/false);
  }

  // This exception should describe the permissions for any device.
  // There are no user granted permissions that intersect with this permission,
  // and this policy only grants one permission to the following site pair:
  // * ("https://google.com", "https://google.com")
  {
    const auto& exception = exceptions_list[1];
    ExpectDisplayNameEq(exception,
                        /*display_name=*/"Devices from any vendor");

    const auto& sites_list = exception.FindKey(kSites)->GetList();
    ASSERT_EQ(sites_list.size(), 1u);
    ExpectValidSiteExceptionObject(sites_list[0],
                                   /*origin=*/kGoogleOrigin,
                                   /*embedding_origin=*/kGoogleOrigin,
                                   /*source=*/kPolicySource,
                                   /*incognito=*/false);
  }

  // This exception should describe the permissions for any device with the
  // vendor ID 6354. There is a user granted permission for a device with that
  // vendor ID, so the site list for this exception will only have the policy
  // granted permission, which is the following:
  // * ("https://android.com", "")
  {
    const auto& exception = exceptions_list[2];
    ExpectDisplayNameEq(exception,
                        /*display_name=*/"Devices from vendor 0x18D2");

    const auto& sites_list = exception.FindKey(kSites)->GetList();
    ASSERT_EQ(sites_list.size(), 1u);
    ExpectValidSiteExceptionObject(sites_list[0],
                                   /*origin=*/kAndroidOrigin,
                                   /*source=*/kPolicySource,
                                   /*incognito=*/false);
  }

  // This exception should describe the permissions for the "Gizmo" device.
  // The user granted permissions are the following:
  // * ("https://chromium.org", "https://chromium.org")
  // * ("https://chromium.org", "https://google.com")
  // * ("https://android.com", "https://chromium.org")
  // The policy granted permission is the following:
  // * ("https://chromium.org", "")
  // The embedding origin is a wildcard, so the policy granted permission covers
  // any user granted permissions that contain a requesting origin of
  // "https://chromium.org", so the site list for this exception will only have
  // the following permissions:
  // * ("https://chromium.org", "")
  // * ("https://android.com", "https://chromium.org")
  {
    const auto& exception = exceptions_list[3];
    ExpectDisplayNameEq(exception, /*display_name=*/"Gizmo");

    const auto& sites_list = exception.FindKey(kSites)->GetList();
    ASSERT_EQ(sites_list.size(), 2u);
    ExpectValidSiteExceptionObject(sites_list[0],
                                   /*origin=*/kChromiumOrigin,
                                   /*source=*/kPolicySource,
                                   /*incognito=*/false);
    ExpectValidSiteExceptionObject(sites_list[1],
                                   /*origin=*/kAndroidOrigin,
                                   /*embedding_origin=*/kChromiumOrigin,
                                   /*source=*/kPreferenceSource,
                                   /*incognito=*/false);
  }
}

}  // namespace site_settings
