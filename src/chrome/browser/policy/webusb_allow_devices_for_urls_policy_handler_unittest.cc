// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/webusb_allow_devices_for_urls_policy_handler.h"

#include <memory>
#include <utility>

#include "base/json/json_reader.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/policy/core/browser/configuration_policy_pref_store.h"
#include "components/policy/core/browser/configuration_policy_pref_store_test.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/policy_constants.h"

namespace policy {

namespace {

constexpr char kDevicesKey[] = "devices";
constexpr char kUrlsKey[] = "urls";
constexpr char kVendorIdKey[] = "vendor_id";
constexpr char kProductIdKey[] = "product_id";
// This policy contains several valid entries. A valid |devices| item is an
// object that contains both IDs, only the |vendor_id|, or neither IDs. A valid
// |urls| entry is a string containing up to two valid URLs delimited by a
// comma.
constexpr char kValidPolicy[] = R"(
    [
      {
        "devices": [
          {
            "vendor_id": 1234,
            "product_id": 5678
          }, {
            "vendor_id": 4321
          }
        ],
        "urls": [
          "https://google.com,https://google.com",
          "https://www.youtube.com"
        ]
      }, {
        "devices": [{ }],
        "urls": ["https://chromium.org,"]
      }
    ])";
// An invalid entry invalidates the entire policy.
constexpr char kInvalidPolicyInvalidTopLevelEntry[] = R"(
    [
      {
        "devices": [
          {
            "vendor_id": 1234,
            "product_id": 5678
          }, {
            "vendor_id": 4321
          }
        ],
        "urls": [
          "https://google.com,https://google.com",
          "https://www.youtube.com"
        ]
      }, {
        "urls": ["https://crbug.com"]
      }
    ])";
// A list item must have both |devices| and |urls| specified.
constexpr char kInvalidPolicyMissingDevicesProperty[] = R"(
    [
      {
        "urls": [
          "https://google.com,https://google.com",
          "https://www.youtube.com"
        ]
      }
    ])";
constexpr char kInvalidPolicyMissingUrlsProperty[] = R"(
    [
      {
        "devices": [
          {
            "vendor_id": 1234,
            "product_id": 5678
          }
        ]
      }
    ])";
// The |vendor_id| and |product_id| values should fit into an unsigned short.
constexpr char kInvalidPolicyMismatchedVendorIdType[] = R"(
    [
      {
        "devices": [
          {
            "vendor_id": 70000,
            "product_id": 5678
          }
        ],
        "urls": [
          "https://google.com,https://google.com",
          "https://www.youtube.com"
        ]
      }
    ])";
constexpr char kInvalidPolicyMismatchedProductIdType[] = R"(
    [
      {
        "devices": [
          {
            "vendor_id": 1234,
            "product_id": 70000
          }
        ],
        "urls": [
          "https://google.com,https://google.com",
          "https://www.youtube.com"
        ]
      }
    ])";
// Unknown properties invalidate the policy.
constexpr char kInvalidPolicyUnknownProperty[] = R"(
    [
      {
        "devices": [
          {
            "vendor_id": 1234,
            "product_id": 5678,
            "serialNumber": "1234ABCD"
          }
        ],
        "urls": [
          "https://google.com,https://google.com",
          "https://www.youtube.com"
        ]
      }
    ])";
// A device containing a |product_id| must also have a |vendor_id|.
constexpr char kInvalidPolicyProductIdWithoutVendorId[] = R"(
    [
      {
        "devices": [
          {
            "product_id": 5678
          }
        ],
        "urls": [
          "https://google.com,https://google.com",
          "https://www.youtube.com"
        ]
      }
    ])";
// The |urls| array must contain valid URLs.
constexpr char kInvalidPolicyInvalidRequestingUrl[] = R"(
    [
      {
        "devices": [{ "vendor_id": 1234, "product_id": 5678 }],
        "urls": ["some.invalid.url"]
      }
    ])";
constexpr char kInvalidPolicyInvalidEmbeddingUrl[] = R"(
    [
      {
        "devices": [{ "vendor_id": 1234, "product_id": 5678 }],
        "urls": ["https://google.com,some.invalid.url"]
      }
    ])";
constexpr char kInvalidPolicyInvalidUrlsEntry[] = R"(
    [
      {
        "devices": [{ "vendor_id": 1234, "product_id": 5678 }],
        "urls": ["https://google.com,https://google.com,https://google.com"]
      }
    ])";
constexpr char InvalidPolicyNoUrls[] = R"(
    [
      {
        "devices": [{ "vendor_id": 1234, "product_id": 5678 }],
        "urls": [""]
      }
    ])";

}  // namespace

class WebUsbAllowDevicesForUrlsPolicyHandlerTest
    : public ConfigurationPolicyPrefStoreTest {
 public:
  WebUsbAllowDevicesForUrlsPolicyHandler* handler() { return handler_; }

 private:
  void SetUp() override {
    Schema chrome_schema = Schema::Wrap(GetChromeSchemaData());
    auto handler =
        std::make_unique<WebUsbAllowDevicesForUrlsPolicyHandler>(chrome_schema);
    handler_ = handler.get();
    handler_list_.AddHandler(std::move(handler));
  }

  WebUsbAllowDevicesForUrlsPolicyHandler* handler_;
};

TEST_F(WebUsbAllowDevicesForUrlsPolicyHandlerTest, CheckPolicySettings) {
  PolicyMap policy;
  PolicyErrorMap errors;

  policy.Set(
      key::kWebUsbAllowDevicesForUrls, PolicyLevel::POLICY_LEVEL_MANDATORY,
      PolicyScope::POLICY_SCOPE_MACHINE, PolicySource::POLICY_SOURCE_CLOUD,
      base::JSONReader::ReadDeprecated(kValidPolicy), nullptr);
  ASSERT_TRUE(errors.empty());
  EXPECT_TRUE(handler()->CheckPolicySettings(policy, &errors));
  EXPECT_TRUE(errors.empty());
}

TEST_F(WebUsbAllowDevicesForUrlsPolicyHandlerTest,
       CheckPolicySettingsWithInvalidTopLevelEntry) {
  PolicyMap policy;
  PolicyErrorMap errors;

  policy.Set(
      key::kWebUsbAllowDevicesForUrls, PolicyLevel::POLICY_LEVEL_MANDATORY,
      PolicyScope::POLICY_SCOPE_MACHINE, PolicySource::POLICY_SOURCE_CLOUD,
      base::JSONReader::ReadDeprecated(kInvalidPolicyInvalidTopLevelEntry),
      nullptr);

  ASSERT_TRUE(errors.empty());
  EXPECT_FALSE(handler()->CheckPolicySettings(policy, &errors));
  EXPECT_EQ(errors.size(), 1ul);

  const base::string16 kExpected = base::ASCIIToUTF16(
      "Schema validation error at \"items[1]\": Missing or invalid required "
      "property: devices");
  EXPECT_EQ(errors.GetErrors(key::kWebUsbAllowDevicesForUrls), kExpected);
}

TEST_F(WebUsbAllowDevicesForUrlsPolicyHandlerTest,
       CheckPolicySettingsWithMissingDevicesProperty) {
  PolicyMap policy;
  PolicyErrorMap errors;

  policy.Set(
      key::kWebUsbAllowDevicesForUrls, PolicyLevel::POLICY_LEVEL_MANDATORY,
      PolicyScope::POLICY_SCOPE_MACHINE, PolicySource::POLICY_SOURCE_CLOUD,
      base::JSONReader::ReadDeprecated(kInvalidPolicyMissingDevicesProperty),
      nullptr);

  ASSERT_TRUE(errors.empty());
  EXPECT_FALSE(handler()->CheckPolicySettings(policy, &errors));
  EXPECT_EQ(errors.size(), 1ul);

  const base::string16 kExpected = base::ASCIIToUTF16(
      "Schema validation error at \"items[0]\": Missing or invalid required "
      "property: devices");
  EXPECT_EQ(errors.GetErrors(key::kWebUsbAllowDevicesForUrls), kExpected);
}

TEST_F(WebUsbAllowDevicesForUrlsPolicyHandlerTest,
       CheckPolicySettingsWithMissingUrlsProperty) {
  PolicyMap policy;
  PolicyErrorMap errors;

  policy.Set(
      key::kWebUsbAllowDevicesForUrls, PolicyLevel::POLICY_LEVEL_MANDATORY,
      PolicyScope::POLICY_SCOPE_MACHINE, PolicySource::POLICY_SOURCE_CLOUD,
      base::JSONReader::ReadDeprecated(kInvalidPolicyMissingUrlsProperty),
      nullptr);

  ASSERT_TRUE(errors.empty());
  EXPECT_FALSE(handler()->CheckPolicySettings(policy, &errors));
  EXPECT_EQ(errors.size(), 1ul);

  const base::string16 kExpected = base::ASCIIToUTF16(
      "Schema validation error at \"items[0]\": Missing or invalid required "
      "property: urls");
  EXPECT_EQ(errors.GetErrors(key::kWebUsbAllowDevicesForUrls), kExpected);
}

TEST_F(WebUsbAllowDevicesForUrlsPolicyHandlerTest,
       CheckPolicySettingsUnknownProperty) {
  PolicyMap policy;
  PolicyErrorMap errors;

  policy.Set(
      key::kWebUsbAllowDevicesForUrls, PolicyLevel::POLICY_LEVEL_MANDATORY,
      PolicyScope::POLICY_SCOPE_MACHINE, PolicySource::POLICY_SOURCE_CLOUD,
      base::JSONReader::ReadDeprecated(kInvalidPolicyUnknownProperty), nullptr);

  ASSERT_TRUE(errors.empty());
  EXPECT_FALSE(handler()->CheckPolicySettings(policy, &errors));
  EXPECT_EQ(errors.size(), 1ul);

  const base::string16 kExpected = base::ASCIIToUTF16(
      "Schema validation error at \"items[0].devices.items[0]\": Unknown "
      "property: serialNumber");
  EXPECT_EQ(errors.GetErrors(key::kWebUsbAllowDevicesForUrls), kExpected);
}

TEST_F(WebUsbAllowDevicesForUrlsPolicyHandlerTest,
       CheckPolicySettingsWithMismatchedVendorIdType) {
  PolicyMap policy;
  PolicyErrorMap errors;

  policy.Set(
      key::kWebUsbAllowDevicesForUrls, PolicyLevel::POLICY_LEVEL_MANDATORY,
      PolicyScope::POLICY_SCOPE_MACHINE, PolicySource::POLICY_SOURCE_CLOUD,
      base::JSONReader::ReadDeprecated(kInvalidPolicyMismatchedVendorIdType),
      nullptr);

  ASSERT_TRUE(errors.empty());
  EXPECT_FALSE(handler()->CheckPolicySettings(policy, &errors));
  EXPECT_EQ(errors.size(), 1ul);

  const base::string16 kExpected = base::ASCIIToUTF16(
      "Schema validation error at \"items[0].devices.items[0]\": The vendor_id "
      "must be an unsigned short integer");
  EXPECT_EQ(errors.GetErrors(key::kWebUsbAllowDevicesForUrls), kExpected);
}

TEST_F(WebUsbAllowDevicesForUrlsPolicyHandlerTest,
       CheckPolicySettingsWithMismatchedProductIdType) {
  PolicyMap policy;
  PolicyErrorMap errors;

  policy.Set(
      key::kWebUsbAllowDevicesForUrls, PolicyLevel::POLICY_LEVEL_MANDATORY,
      PolicyScope::POLICY_SCOPE_MACHINE, PolicySource::POLICY_SOURCE_CLOUD,
      base::JSONReader::ReadDeprecated(kInvalidPolicyMismatchedProductIdType),
      nullptr);

  ASSERT_TRUE(errors.empty());
  EXPECT_FALSE(handler()->CheckPolicySettings(policy, &errors));
  EXPECT_EQ(errors.size(), 1ul);

  const base::string16 kExpected = base::ASCIIToUTF16(
      "Schema validation error at \"items[0].devices.items[0]\": The "
      "product_id must be an unsigned short integer");
  EXPECT_EQ(errors.GetErrors(key::kWebUsbAllowDevicesForUrls), kExpected);
}

TEST_F(WebUsbAllowDevicesForUrlsPolicyHandlerTest,
       CheckPolicySettingsWithProductIdWithoutVendorId) {
  PolicyMap policy;
  PolicyErrorMap errors;

  policy.Set(
      key::kWebUsbAllowDevicesForUrls, PolicyLevel::POLICY_LEVEL_MANDATORY,
      PolicyScope::POLICY_SCOPE_MACHINE, PolicySource::POLICY_SOURCE_CLOUD,
      base::JSONReader::ReadDeprecated(kInvalidPolicyProductIdWithoutVendorId),
      nullptr);

  ASSERT_TRUE(errors.empty());
  EXPECT_FALSE(handler()->CheckPolicySettings(policy, &errors));
  EXPECT_EQ(errors.size(), 1ul);

  const base::string16 kExpected = base::ASCIIToUTF16(
      "Schema validation error at \"items[0].devices.items[0]\": A vendor_id "
      "must also be specified");
  EXPECT_EQ(errors.GetErrors(key::kWebUsbAllowDevicesForUrls), kExpected);
}

TEST_F(WebUsbAllowDevicesForUrlsPolicyHandlerTest,
       CheckPolicySettingsWithInvalidRequestingUrlEntry) {
  PolicyMap policy;
  PolicyErrorMap errors;

  policy.Set(
      key::kWebUsbAllowDevicesForUrls, PolicyLevel::POLICY_LEVEL_MANDATORY,
      PolicyScope::POLICY_SCOPE_MACHINE, PolicySource::POLICY_SOURCE_CLOUD,
      base::JSONReader::ReadDeprecated(kInvalidPolicyInvalidRequestingUrl),
      nullptr);

  ASSERT_TRUE(errors.empty());
  EXPECT_FALSE(handler()->CheckPolicySettings(policy, &errors));
  EXPECT_EQ(errors.size(), 1ul);

  const base::string16 kExpected = base::ASCIIToUTF16(
      "Schema validation error at \"items[0].urls.items[0]\": The urls item "
      "must contain valid URLs");
  EXPECT_EQ(errors.GetErrors(key::kWebUsbAllowDevicesForUrls), kExpected);
}

TEST_F(WebUsbAllowDevicesForUrlsPolicyHandlerTest,
       CheckPolicySettingsWithInvalidEmbeddingUrlEntry) {
  PolicyMap policy;
  PolicyErrorMap errors;

  policy.Set(
      key::kWebUsbAllowDevicesForUrls, PolicyLevel::POLICY_LEVEL_MANDATORY,
      PolicyScope::POLICY_SCOPE_MACHINE, PolicySource::POLICY_SOURCE_CLOUD,
      base::JSONReader::ReadDeprecated(kInvalidPolicyInvalidEmbeddingUrl),
      nullptr);

  ASSERT_TRUE(errors.empty());
  EXPECT_FALSE(handler()->CheckPolicySettings(policy, &errors));
  EXPECT_EQ(errors.size(), 1ul);

  const base::string16 kExpected = base::ASCIIToUTF16(
      "Schema validation error at \"items[0].urls.items[0]\": The urls item "
      "must contain valid URLs");
  EXPECT_EQ(errors.GetErrors(key::kWebUsbAllowDevicesForUrls), kExpected);
}

TEST_F(WebUsbAllowDevicesForUrlsPolicyHandlerTest,
       CheckPolicySettingsWithInvalidUrlsEntry) {
  PolicyMap policy;
  PolicyErrorMap errors;

  policy.Set(
      key::kWebUsbAllowDevicesForUrls, PolicyLevel::POLICY_LEVEL_MANDATORY,
      PolicyScope::POLICY_SCOPE_MACHINE, PolicySource::POLICY_SOURCE_CLOUD,
      base::JSONReader::ReadDeprecated(kInvalidPolicyInvalidUrlsEntry),
      nullptr);

  ASSERT_TRUE(errors.empty());
  EXPECT_FALSE(handler()->CheckPolicySettings(policy, &errors));
  EXPECT_EQ(errors.size(), 1ul);

  const base::string16 kExpected = base::ASCIIToUTF16(
      "Schema validation error at \"items[0].urls.items[0]\": Each urls "
      "string entry must contain between 1 to 2 URLs");
  EXPECT_EQ(errors.GetErrors(key::kWebUsbAllowDevicesForUrls), kExpected);
}

TEST_F(WebUsbAllowDevicesForUrlsPolicyHandlerTest,
       CheckPolicySettingsWithNoUrls) {
  PolicyMap policy;
  PolicyErrorMap errors;

  policy.Set(
      key::kWebUsbAllowDevicesForUrls, PolicyLevel::POLICY_LEVEL_MANDATORY,
      PolicyScope::POLICY_SCOPE_MACHINE, PolicySource::POLICY_SOURCE_CLOUD,
      base::JSONReader::ReadDeprecated(InvalidPolicyNoUrls), nullptr);

  ASSERT_TRUE(errors.empty());
  EXPECT_FALSE(handler()->CheckPolicySettings(policy, &errors));
  EXPECT_EQ(errors.size(), 1ul);

  const base::string16 kExpected = base::ASCIIToUTF16(
      "Schema validation error at \"items[0].urls.items[0]\": Each urls "
      "string entry must contain between 1 to 2 URLs");
  EXPECT_EQ(errors.GetErrors(key::kWebUsbAllowDevicesForUrls), kExpected);
}

TEST_F(WebUsbAllowDevicesForUrlsPolicyHandlerTest, ApplyPolicySettings) {
  EXPECT_FALSE(
      store_->GetValue(prefs::kManagedWebUsbAllowDevicesForUrls, nullptr));

  PolicyMap policy;
  policy.Set(
      key::kWebUsbAllowDevicesForUrls, PolicyLevel::POLICY_LEVEL_MANDATORY,
      PolicyScope::POLICY_SCOPE_MACHINE, PolicySource::POLICY_SOURCE_CLOUD,
      base::JSONReader::ReadDeprecated(kValidPolicy), nullptr);
  UpdateProviderPolicy(policy);

  const base::Value* pref_value = nullptr;
  EXPECT_TRUE(
      store_->GetValue(prefs::kManagedWebUsbAllowDevicesForUrls, &pref_value));
  ASSERT_TRUE(pref_value);
  ASSERT_TRUE(pref_value->is_list());

  // Ensure that the kManagedWebUsbAllowDevicesForUrls pref is set correctly.
  const auto& list = pref_value->GetList();
  ASSERT_EQ(list.size(), 2ul);

  // Check the first item's devices list.
  const base::Value* devices = list[0].FindKey(kDevicesKey);
  ASSERT_TRUE(devices);

  const auto& first_devices_list = devices->GetList();
  ASSERT_EQ(first_devices_list.size(), 2ul);

  const base::Value* vendor_id = first_devices_list[0].FindKey(kVendorIdKey);
  ASSERT_TRUE(vendor_id);
  EXPECT_EQ(vendor_id->GetInt(), 1234);

  const base::Value* product_id = first_devices_list[0].FindKey(kProductIdKey);
  ASSERT_TRUE(product_id);
  EXPECT_EQ(product_id->GetInt(), 5678);

  vendor_id = first_devices_list[1].FindKey(kVendorIdKey);
  ASSERT_TRUE(vendor_id);
  EXPECT_EQ(vendor_id->GetInt(), 4321);

  product_id = first_devices_list[1].FindKey(kProductIdKey);
  EXPECT_FALSE(product_id);

  // Check the first item's urls list.
  const base::Value* urls = list[0].FindKey(kUrlsKey);
  ASSERT_TRUE(urls);

  const auto& first_urls_list = urls->GetList();
  ASSERT_EQ(first_urls_list.size(), 2ul);
  ASSERT_TRUE(first_urls_list[0].is_string());
  ASSERT_TRUE(first_urls_list[1].is_string());
  EXPECT_EQ(first_urls_list[0].GetString(),
            "https://google.com,https://google.com");
  EXPECT_EQ(first_urls_list[1].GetString(), "https://www.youtube.com");

  // Check the second item's devices list.
  devices = list[1].FindKey(kDevicesKey);
  ASSERT_TRUE(devices);

  const auto& second_devices_list = devices->GetList();
  ASSERT_EQ(second_devices_list.size(), 1ul);

  vendor_id = second_devices_list[0].FindKey(kVendorIdKey);
  EXPECT_FALSE(vendor_id);

  product_id = second_devices_list[0].FindKey(kProductIdKey);
  EXPECT_FALSE(product_id);

  // Check the second item's urls list.
  urls = list[1].FindKey(kUrlsKey);
  ASSERT_TRUE(urls);

  const auto& second_urls_list = urls->GetList();
  ASSERT_EQ(second_urls_list.size(), 1ul);
  ASSERT_TRUE(second_urls_list[0].is_string());
  EXPECT_EQ(second_urls_list[0].GetString(), "https://chromium.org,");
}

TEST_F(WebUsbAllowDevicesForUrlsPolicyHandlerTest,
       ApplyPolicySettingsWithInvalidTopLevelEntry) {
  EXPECT_FALSE(
      store_->GetValue(prefs::kManagedWebUsbAllowDevicesForUrls, nullptr));

  PolicyMap policy;
  policy.Set(
      key::kWebUsbAllowDevicesForUrls, PolicyLevel::POLICY_LEVEL_MANDATORY,
      PolicyScope::POLICY_SCOPE_MACHINE, PolicySource::POLICY_SOURCE_CLOUD,
      base::JSONReader::ReadDeprecated(kInvalidPolicyInvalidTopLevelEntry),
      nullptr);
  UpdateProviderPolicy(policy);

  const base::Value* pref_value = nullptr;
  EXPECT_FALSE(
      store_->GetValue(prefs::kManagedWebUsbAllowDevicesForUrls, &pref_value));
  EXPECT_FALSE(pref_value);
}

TEST_F(WebUsbAllowDevicesForUrlsPolicyHandlerTest,
       ApplyPolicySettingsWithMissingDevicesProperty) {
  EXPECT_FALSE(
      store_->GetValue(prefs::kManagedWebUsbAllowDevicesForUrls, nullptr));

  PolicyMap policy;
  policy.Set(
      key::kWebUsbAllowDevicesForUrls, PolicyLevel::POLICY_LEVEL_MANDATORY,
      PolicyScope::POLICY_SCOPE_MACHINE, PolicySource::POLICY_SOURCE_CLOUD,
      base::JSONReader::ReadDeprecated(kInvalidPolicyMissingDevicesProperty),
      nullptr);
  UpdateProviderPolicy(policy);
  const base::Value* pref_value = nullptr;
  EXPECT_FALSE(
      store_->GetValue(prefs::kManagedWebUsbAllowDevicesForUrls, &pref_value));
  EXPECT_FALSE(pref_value);
}

TEST_F(WebUsbAllowDevicesForUrlsPolicyHandlerTest,
       ApplyPolicySettingsWithMissingUrlsProperty) {
  EXPECT_FALSE(
      store_->GetValue(prefs::kManagedWebUsbAllowDevicesForUrls, nullptr));

  PolicyMap policy;
  policy.Set(
      key::kWebUsbAllowDevicesForUrls, PolicyLevel::POLICY_LEVEL_MANDATORY,
      PolicyScope::POLICY_SCOPE_MACHINE, PolicySource::POLICY_SOURCE_CLOUD,
      base::JSONReader::ReadDeprecated(kInvalidPolicyMissingUrlsProperty),
      nullptr);
  UpdateProviderPolicy(policy);
  const base::Value* pref_value = nullptr;
  EXPECT_FALSE(
      store_->GetValue(prefs::kManagedWebUsbAllowDevicesForUrls, &pref_value));
  EXPECT_FALSE(pref_value);
}

TEST_F(WebUsbAllowDevicesForUrlsPolicyHandlerTest,
       ApplyPolicySettingsWithUnknownProperty) {
  EXPECT_FALSE(
      store_->GetValue(prefs::kManagedWebUsbAllowDevicesForUrls, nullptr));

  PolicyMap policy;
  policy.Set(
      key::kWebUsbAllowDevicesForUrls, PolicyLevel::POLICY_LEVEL_MANDATORY,
      PolicyScope::POLICY_SCOPE_MACHINE, PolicySource::POLICY_SOURCE_CLOUD,
      base::JSONReader::ReadDeprecated(kInvalidPolicyUnknownProperty), nullptr);
  UpdateProviderPolicy(policy);
  const base::Value* pref_value = nullptr;
  EXPECT_FALSE(
      store_->GetValue(prefs::kManagedWebUsbAllowDevicesForUrls, &pref_value));
  EXPECT_FALSE(pref_value);
}

TEST_F(WebUsbAllowDevicesForUrlsPolicyHandlerTest,
       ApplyPolicySettingsWithMismatchedVendorIdType) {
  EXPECT_FALSE(
      store_->GetValue(prefs::kManagedWebUsbAllowDevicesForUrls, nullptr));

  PolicyMap policy;
  policy.Set(
      key::kWebUsbAllowDevicesForUrls, PolicyLevel::POLICY_LEVEL_MANDATORY,
      PolicyScope::POLICY_SCOPE_MACHINE, PolicySource::POLICY_SOURCE_CLOUD,
      base::JSONReader::ReadDeprecated(kInvalidPolicyMismatchedVendorIdType),
      nullptr);
  UpdateProviderPolicy(policy);
  const base::Value* pref_value = nullptr;
  EXPECT_FALSE(
      store_->GetValue(prefs::kManagedWebUsbAllowDevicesForUrls, &pref_value));
  EXPECT_FALSE(pref_value);
}

TEST_F(WebUsbAllowDevicesForUrlsPolicyHandlerTest,
       ApplyPolicySettingsWithMismatchedProductIdType) {
  EXPECT_FALSE(
      store_->GetValue(prefs::kManagedWebUsbAllowDevicesForUrls, nullptr));

  PolicyMap policy;
  policy.Set(
      key::kWebUsbAllowDevicesForUrls, PolicyLevel::POLICY_LEVEL_MANDATORY,
      PolicyScope::POLICY_SCOPE_MACHINE, PolicySource::POLICY_SOURCE_CLOUD,
      base::JSONReader::ReadDeprecated(kInvalidPolicyMismatchedProductIdType),
      nullptr);
  UpdateProviderPolicy(policy);
  const base::Value* pref_value = nullptr;
  EXPECT_FALSE(
      store_->GetValue(prefs::kManagedWebUsbAllowDevicesForUrls, &pref_value));
  EXPECT_FALSE(pref_value);
}

TEST_F(WebUsbAllowDevicesForUrlsPolicyHandlerTest,
       ApplyPolicySettingsProductIdWithoutVendorId) {
  EXPECT_FALSE(
      store_->GetValue(prefs::kManagedWebUsbAllowDevicesForUrls, nullptr));

  PolicyMap policy;
  policy.Set(
      key::kWebUsbAllowDevicesForUrls, PolicyLevel::POLICY_LEVEL_MANDATORY,
      PolicyScope::POLICY_SCOPE_MACHINE, PolicySource::POLICY_SOURCE_CLOUD,
      base::JSONReader::ReadDeprecated(kInvalidPolicyProductIdWithoutVendorId),
      nullptr);
  UpdateProviderPolicy(policy);
  const base::Value* pref_value = nullptr;
  EXPECT_FALSE(
      store_->GetValue(prefs::kManagedWebUsbAllowDevicesForUrls, &pref_value));
  EXPECT_FALSE(pref_value);
}

TEST_F(WebUsbAllowDevicesForUrlsPolicyHandlerTest,
       ApplyPolicySettingsInvalidRequestingUrl) {
  EXPECT_FALSE(
      store_->GetValue(prefs::kManagedWebUsbAllowDevicesForUrls, nullptr));

  PolicyMap policy;
  policy.Set(
      key::kWebUsbAllowDevicesForUrls, PolicyLevel::POLICY_LEVEL_MANDATORY,
      PolicyScope::POLICY_SCOPE_MACHINE, PolicySource::POLICY_SOURCE_CLOUD,
      base::JSONReader::ReadDeprecated(kInvalidPolicyInvalidRequestingUrl),
      nullptr);
  UpdateProviderPolicy(policy);
  const base::Value* pref_value = nullptr;
  EXPECT_FALSE(
      store_->GetValue(prefs::kManagedWebUsbAllowDevicesForUrls, &pref_value));
  EXPECT_FALSE(pref_value);
}

TEST_F(WebUsbAllowDevicesForUrlsPolicyHandlerTest,
       ApplyPolicySettingsInvalidEmbeddingUrl) {
  EXPECT_FALSE(
      store_->GetValue(prefs::kManagedWebUsbAllowDevicesForUrls, nullptr));

  PolicyMap policy;
  policy.Set(
      key::kWebUsbAllowDevicesForUrls, PolicyLevel::POLICY_LEVEL_MANDATORY,
      PolicyScope::POLICY_SCOPE_MACHINE, PolicySource::POLICY_SOURCE_CLOUD,
      base::JSONReader::ReadDeprecated(kInvalidPolicyInvalidEmbeddingUrl),
      nullptr);
  UpdateProviderPolicy(policy);
  const base::Value* pref_value = nullptr;
  EXPECT_FALSE(
      store_->GetValue(prefs::kManagedWebUsbAllowDevicesForUrls, &pref_value));
  EXPECT_FALSE(pref_value);
}

TEST_F(WebUsbAllowDevicesForUrlsPolicyHandlerTest,
       ApplyPolicySettingsInvalidUrlsEntry) {
  EXPECT_FALSE(
      store_->GetValue(prefs::kManagedWebUsbAllowDevicesForUrls, nullptr));

  PolicyMap policy;
  policy.Set(
      key::kWebUsbAllowDevicesForUrls, PolicyLevel::POLICY_LEVEL_MANDATORY,
      PolicyScope::POLICY_SCOPE_MACHINE, PolicySource::POLICY_SOURCE_CLOUD,
      base::JSONReader::ReadDeprecated(kInvalidPolicyInvalidUrlsEntry),
      nullptr);
  UpdateProviderPolicy(policy);
  const base::Value* pref_value = nullptr;
  EXPECT_FALSE(
      store_->GetValue(prefs::kManagedWebUsbAllowDevicesForUrls, &pref_value));
  EXPECT_FALSE(pref_value);
}

TEST_F(WebUsbAllowDevicesForUrlsPolicyHandlerTest, ApplyPolicySettingsNoUrls) {
  EXPECT_FALSE(
      store_->GetValue(prefs::kManagedWebUsbAllowDevicesForUrls, nullptr));

  PolicyMap policy;
  policy.Set(
      key::kWebUsbAllowDevicesForUrls, PolicyLevel::POLICY_LEVEL_MANDATORY,
      PolicyScope::POLICY_SCOPE_MACHINE, PolicySource::POLICY_SOURCE_CLOUD,
      base::JSONReader::ReadDeprecated(InvalidPolicyNoUrls), nullptr);
  UpdateProviderPolicy(policy);
  const base::Value* pref_value = nullptr;
  EXPECT_FALSE(
      store_->GetValue(prefs::kManagedWebUsbAllowDevicesForUrls, &pref_value));
  EXPECT_FALSE(pref_value);
}

}  // namespace policy
