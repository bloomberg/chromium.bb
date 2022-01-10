// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "chrome/browser/chromeos/extensions/telemetry/api/base_telemetry_extension_browser_test.h"
#include "chromeos/dbus/cros_healthd/fake_cros_healthd_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon/fake_debug_daemon_client.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

using TelemetryExtensionTelemetryApiBrowserTest =
    BaseTelemetryExtensionBrowserTest;

IN_PROC_BROWSER_TEST_P(TelemetryExtensionTelemetryApiBrowserTest,
                       GetVpdInfoError) {
  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function getVpdInfo() {
        await chrome.test.assertPromiseRejects(
            chrome.os.telemetry.getVpdInfo(),
            'Error: API internal error'
        );
        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_P(TelemetryExtensionTelemetryApiBrowserTest,
                       GetVpdInfoWithSerialNumberPermission) {
  // Configure fake cros_healthd response.
  {
    auto telemetry_info = cros_healthd::mojom::TelemetryInfo::New();

    {
      auto os_version = cros_healthd::mojom::OsVersion::New();

      auto system_info = cros_healthd::mojom::SystemInfo::New();
      system_info->first_power_date = "2021-50";
      system_info->product_model_name = "COOL-LAPTOP-CHROME";
      system_info->product_serial_number = "5CD9132880";
      system_info->product_sku_number = "sku15";
      system_info->os_version = std::move(os_version);

      telemetry_info->system_result =
          cros_healthd::mojom::SystemResult::NewSystemInfo(
              std::move(system_info));
    }

    ASSERT_TRUE(cros_healthd::FakeCrosHealthdClient::Get());

    cros_healthd::FakeCrosHealthdClient::Get()
        ->SetProbeTelemetryInfoResponseForTesting(telemetry_info);
  }

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function getVpdInfo() {
        const result = await chrome.os.telemetry.getVpdInfo();
        chrome.test.assertEq("2021-50", result.activateDate);
        chrome.test.assertEq("COOL-LAPTOP-CHROME", result.modelName);
        chrome.test.assertEq("5CD9132880", result.serialNumber);
        chrome.test.assertEq("sku15", result.skuNumber);
        chrome.test.succeed();
      }
    ]);
  )");
}

namespace {

class TestDebugDaemonClient : public FakeDebugDaemonClient {
 public:
  TestDebugDaemonClient() = default;
  ~TestDebugDaemonClient() override = default;

  void GetLog(const std::string& log_name,
              DBusMethodCallback<std::string> callback) override {
    EXPECT_EQ(log_name, "oemdata");
    std::move(callback).Run(absl::nullopt);
  }
};

}  // namespace

IN_PROC_BROWSER_TEST_P(TelemetryExtensionTelemetryApiBrowserTest,
                       GetOemDataWithSerialNumberPermission_Error) {
  DBusThreadManager::GetSetterForTesting()->SetDebugDaemonClient(
      std::make_unique<TestDebugDaemonClient>());

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function getOemData() {
        await chrome.test.assertPromiseRejects(
            chrome.os.telemetry.getOemData(),
            'Error: API internal error'
        );
        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_P(TelemetryExtensionTelemetryApiBrowserTest,
                       GetOemDataWithSerialNumberPermission_Success) {
  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function getOemData() {
        const result = await chrome.os.telemetry.getOemData();
        chrome.test.assertEq(
          "oemdata: response from GetLog", result.oemData);
        chrome.test.succeed();
      }
    ]);
  )");
}

INSTANTIATE_TEST_SUITE_P(
    All,
    TelemetryExtensionTelemetryApiBrowserTest,
    testing::Combine(
        testing::Bool(),
        testing::ValuesIn(
            BaseTelemetryExtensionBrowserTest::kAllExtensionInfoTestParams)));

class TelemetryExtensionTelemetryApiWithoutSerialNumberBrowserTest
    : public TelemetryExtensionTelemetryApiBrowserTest {
 public:
  TelemetryExtensionTelemetryApiWithoutSerialNumberBrowserTest() = default;
  ~TelemetryExtensionTelemetryApiWithoutSerialNumberBrowserTest() override =
      default;

  TelemetryExtensionTelemetryApiWithoutSerialNumberBrowserTest(
      const BaseTelemetryExtensionBrowserTest&) = delete;
  TelemetryExtensionTelemetryApiWithoutSerialNumberBrowserTest& operator=(
      const TelemetryExtensionTelemetryApiWithoutSerialNumberBrowserTest&) =
      delete;

 protected:
  std::string GetManifestFile(const std::string& public_key,
                              const std::string& matches_origin) override {
    return base::StringPrintf(R"(
          {
            "key": "%s",
            "name": "Test Telemetry Extension",
            "version": "1",
            "manifest_version": 3,
            "chromeos_system_extension": {},
            "background": {
              "service_worker": "sw.js"
            },
            "permissions": [ "os.diagnostics", "os.telemetry" ],
            "externally_connectable": {
              "matches": [
                "%s"
              ]
            },
            "options_page": "options.html"
          }
        )", public_key.c_str(), matches_origin.c_str());
  }
};

IN_PROC_BROWSER_TEST_P(
    TelemetryExtensionTelemetryApiWithoutSerialNumberBrowserTest,
    GetVpdInfoWithoutSerialNumberPermission) {
  // Configure fake cros_healthd response.
  {
    auto telemetry_info = cros_healthd::mojom::TelemetryInfo::New();

    {
      auto os_version = cros_healthd::mojom::OsVersion::New();

      auto system_info = cros_healthd::mojom::SystemInfo::New();
      system_info->first_power_date = "2021-50";
      system_info->product_model_name = "COOL-LAPTOP-CHROME";
      system_info->product_serial_number = "5CD9132880";
      system_info->product_sku_number = "sku15";
      system_info->os_version = std::move(os_version);

      telemetry_info->system_result =
          cros_healthd::mojom::SystemResult::NewSystemInfo(
              std::move(system_info));
    }

    ASSERT_TRUE(cros_healthd::FakeCrosHealthdClient::Get());

    cros_healthd::FakeCrosHealthdClient::Get()
        ->SetProbeTelemetryInfoResponseForTesting(telemetry_info);
  }

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function getVpdInfo() {
        const result = await chrome.os.telemetry.getVpdInfo();
        chrome.test.assertEq("2021-50", result.activateDate);
        chrome.test.assertEq("COOL-LAPTOP-CHROME", result.modelName);
        chrome.test.assertEq(null, result.serialNumber);
        chrome.test.assertEq("sku15", result.skuNumber);
        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_P(
    TelemetryExtensionTelemetryApiWithoutSerialNumberBrowserTest,
    GetOemDataWithoutSerialNumberPermission) {
  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function getOemData() {
        await chrome.test.assertPromiseRejects(
            chrome.os.telemetry.getOemData(),
            'Error: Unauthorized access to chrome.os.telemetry.getOemData. ' +
            'Extension doesn\'t have the permission.'
        );
        chrome.test.succeed();
      }
    ]);
  )");
}

INSTANTIATE_TEST_SUITE_P(
    All,
    TelemetryExtensionTelemetryApiWithoutSerialNumberBrowserTest,
    testing::Combine(
        testing::Bool(),
        testing::ValuesIn(
            BaseTelemetryExtensionBrowserTest::kAllExtensionInfoTestParams)));

}  // namespace chromeos
