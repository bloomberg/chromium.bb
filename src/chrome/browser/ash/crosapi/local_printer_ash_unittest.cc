// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/local_printer_ash.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_piece.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/ash/crosapi/test_local_printer_ash.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/chromeos/printing/cups_printers_manager_factory.h"
#include "chrome/browser/chromeos/printing/test_cups_printers_manager.h"
#include "chrome/browser/chromeos/printing/test_printer_configurer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/printing/printer_capabilities.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/crosapi/mojom/local_printer.mojom.h"
#include "chromeos/printing/cups_printer_status.h"
#include "chromeos/printing/ppd_provider.h"
#include "chromeos/printing/printer_configuration.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_manager/user.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_task_environment.h"
#include "printing/backend/print_backend.h"
#include "printing/backend/printing_restrictions.h"
#include "printing/backend/test_print_backend.h"
#include "printing/buildflags/buildflags.h"
#include "printing/printing_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_OOP_PRINTING)
#include "chrome/browser/printing/print_backend_service_manager.h"
#include "chrome/browser/printing/print_backend_service_test_impl.h"
#else
#include "base/notreached.h"
#endif

using chromeos::CupsPrintersManager;
using chromeos::Printer;
using chromeos::PrinterClass;
using chromeos::PrinterConfigurer;
using chromeos::PrinterSetupCallback;
using chromeos::PrinterSetupResult;

namespace printing {

namespace {

constexpr char kPrinterUri[] = "http://localhost:80";

// Used as a callback to `GetPrinters()` in tests.
// Records list returned by `GetPrinters()`.
void RecordPrinterList(
    std::vector<crosapi::mojom::LocalDestinationInfoPtr>& printers_out,
    std::vector<crosapi::mojom::LocalDestinationInfoPtr> printers) {
  printers_out = std::move(printers);
}

void RecordGetCapability(
    crosapi::mojom::CapabilitiesResponsePtr& capabilities_out,
    crosapi::mojom::CapabilitiesResponsePtr capability) {
  capabilities_out = std::move(capability);
}

void RecordGetEulaUrl(GURL& fetched_eula_url, const GURL& eula_url) {
  fetched_eula_url = eula_url;
}

Printer CreateTestPrinter(const std::string& id,
                          const std::string& name,
                          const std::string& description) {
  Printer printer;
  printer.SetUri(kPrinterUri);
  printer.set_id(id);
  printer.set_display_name(name);
  printer.set_description(description);
  return printer;
}

Printer CreateTestPrinterWithPpdReference(const std::string& id,
                                          const std::string& name,
                                          const std::string& description,
                                          Printer::PpdReference ref) {
  Printer printer = CreateTestPrinter(id, name, description);
  Printer::PpdReference* mutable_ppd_reference =
      printer.mutable_ppd_reference();
  *mutable_ppd_reference = ref;
  return printer;
}

Printer CreateEnterprisePrinter(const std::string& id,
                                const std::string& name,
                                const std::string& description) {
  Printer printer = CreateTestPrinter(id, name, description);
  printer.set_source(Printer::SRC_POLICY);
  return printer;
}

}  // namespace

// Fake `PpdProvider` backend. This fake `PpdProvider` is used to fake fetching
// the PPD EULA license of a destination. If `effective_make_and_model` is
// empty, it will return with NOT_FOUND and an empty string. Otherwise, it will
// return SUCCESS with `effective_make_and_model` as the PPD license.
class FakePpdProvider : public chromeos::PpdProvider {
 public:
  FakePpdProvider() = default;

  void ResolvePpdLicense(base::StringPiece effective_make_and_model,
                         ResolvePpdLicenseCallback cb) override {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(cb),
                       effective_make_and_model.empty() ? PpdProvider::NOT_FOUND
                                                        : PpdProvider::SUCCESS,
                       std::string(effective_make_and_model)));
  }

  // These methods are not used by `CupsPrintersManager`.
  void ResolvePpd(const Printer::PpdReference& reference,
                  ResolvePpdCallback cb) override {}
  void ResolvePpdReference(const chromeos::PrinterSearchData& search_data,
                           ResolvePpdReferenceCallback cb) override {}
  void ResolveManufacturers(ResolveManufacturersCallback cb) override {}
  void ResolvePrinters(const std::string& manufacturer,
                       ResolvePrintersCallback cb) override {}
  void ReverseLookup(const std::string& effective_make_and_model,
                     ReverseLookupCallback cb) override {}

 private:
  ~FakePpdProvider() override = default;
};

class FakeUser : public user_manager::User {
 public:
  FakeUser()
      : user_manager::User(
            AccountId::FromUserEmail(TestingProfile::kDefaultProfileUserName)) {
  }
  FakeUser(const FakeUser&) = delete;
  FakeUser& operator=(const FakeUser&) = delete;
  ~FakeUser() override = default;

  using user_manager::User::set_display_email;

  // User:
  user_manager::UserType GetType() const override {
    return user_manager::USER_TYPE_REGULAR;
  }
};

class TestLocalPrinterAshWithPrinterConfigurer : public TestLocalPrinterAsh {
 public:
  TestLocalPrinterAshWithPrinterConfigurer(
      Profile* profile,
      scoped_refptr<chromeos::PpdProvider> ppd_provider,
      chromeos::TestCupsPrintersManager* manager)
      : TestLocalPrinterAsh(profile, ppd_provider), manager_(manager) {}
  TestLocalPrinterAshWithPrinterConfigurer(
      const TestLocalPrinterAshWithPrinterConfigurer&) = delete;
  TestLocalPrinterAshWithPrinterConfigurer& operator=(
      const TestLocalPrinterAshWithPrinterConfigurer&) = delete;
  ~TestLocalPrinterAshWithPrinterConfigurer() override = default;

 private:
  std::unique_ptr<chromeos::PrinterConfigurer> CreatePrinterConfigurer(
      Profile* profile) override {
    return std::make_unique<chromeos::TestPrinterConfigurer>(manager_);
  }

  chromeos::TestCupsPrintersManager* manager_;
};

// Base testing class for `LocalPrinterAsh`.  Contains the base
// logic to allow for using either a local task runner or a service to make
// print backend calls, and to possibly enable fallback when using a service.
// Tests to trigger those different paths can be done by overloading
// `UseService()` and `SupportFallback()`.
class LocalPrinterAshTestBase : public testing::Test {
 public:
  const std::vector<PrinterSemanticCapsAndDefaults::Paper> kPapers = {
      {"bar", "vendor", {600, 600}}};

  LocalPrinterAshTestBase() = default;
  LocalPrinterAshTestBase(const LocalPrinterAshTestBase&) = delete;
  LocalPrinterAshTestBase& operator=(const LocalPrinterAshTestBase&) = delete;
  ~LocalPrinterAshTestBase() override = default;

  TestPrintBackend* sandboxed_print_backend() {
    return sandboxed_test_backend_.get();
  }
  TestPrintBackend* unsandboxed_print_backend() {
    return unsandboxed_test_backend_.get();
  }

  void SetUsername(const std::string& username) {
    user_.set_display_email(username);
  }

  // Indicate if calls to print backend should be made using a service instead
  // of a local task runner.
  virtual bool UseService() = 0;

  // Indicate if fallback support for access-denied errors should be included
  // when using a service for print backend calls.
  virtual bool SupportFallback() = 0;

  sync_preferences::TestingPrefServiceSyncable* GetPrefs() {
    return profile_.GetTestingPrefService();
  }

  void SetUp() override {
    chromeos::ProfileHelper::Get()->SetProfileToUserMappingForTesting(&user_);

#if BUILDFLAG(ENABLE_OOP_PRINTING)
    // Choose between running with local test runner or via a service.
    feature_list_.InitWithFeatureState(features::kEnableOopPrintDrivers,
                                       UseService());
#endif

    sandboxed_test_backend_ = base::MakeRefCounted<TestPrintBackend>();
    ppd_provider_ = base::MakeRefCounted<FakePpdProvider>();
    chromeos::CupsPrintersManagerFactory::GetInstance()
        ->SetTestingFactoryAndUse(
            &profile_,
            base::BindLambdaForTesting([this](content::BrowserContext* context)
                                           -> std::unique_ptr<KeyedService> {
              auto printers_manager =
                  std::make_unique<chromeos::TestCupsPrintersManager>();
              printers_manager_ = printers_manager.get();
              return printers_manager;
            }));
    local_printer_ash_ =
        std::make_unique<TestLocalPrinterAshWithPrinterConfigurer>(
            &profile_, ppd_provider_, printers_manager_);

    if (UseService()) {
#if BUILDFLAG(ENABLE_OOP_PRINTING)
      sandboxed_print_backend_service_ =
          PrintBackendServiceTestImpl::LaunchForTesting(sandboxed_test_remote_,
                                                        sandboxed_test_backend_,
                                                        /*sandboxed=*/true);

      if (SupportFallback()) {
        unsandboxed_test_backend_ = base::MakeRefCounted<TestPrintBackend>();

        unsandboxed_print_backend_service_ =
            PrintBackendServiceTestImpl::LaunchForTesting(
                unsandboxed_test_remote_, unsandboxed_test_backend_,
                /*sandboxed=*/false);
      }
#else
      NOTREACHED();
#endif  // BUILDFLAG(ENABLE_OOP_PRINTING)
    } else {
      // Use of task runners will call `PrintBackend::CreateInstance()`, which
      // needs a test backend registered for it to use.
      PrintBackend::SetPrintBackendForTesting(sandboxed_test_backend_.get());
    }
  }

  void TearDown() override {
#if BUILDFLAG(ENABLE_OOP_PRINTING)
    PrintBackendServiceManager::ResetForTesting();
#endif
    chromeos::ProfileHelper::Get()->RemoveUserFromListForTesting(
        user_.GetAccountId());
  }

 protected:
  void AddPrinter(const std::string& id,
                  const std::string& display_name,
                  const std::string& description,
                  bool is_default,
                  bool requires_elevated_permissions) {
    auto caps = std::make_unique<PrinterSemanticCapsAndDefaults>();
    caps->papers = kPapers;
    auto basic_info = std::make_unique<PrinterBasicInfo>(
        id, display_name, description, /*printer_status=*/0, is_default,
        PrinterBasicInfoOptions{});

#if BUILDFLAG(ENABLE_OOP_PRINTING)
    if (SupportFallback()) {
      // Need to populate same values into a second print backend.
      // For fallback they will always be treated as valid.
      auto caps_unsandboxed =
          std::make_unique<PrinterSemanticCapsAndDefaults>(*caps);
      auto basic_info_unsandboxed =
          std::make_unique<PrinterBasicInfo>(*basic_info);
      unsandboxed_print_backend()->AddValidPrinter(
          id, std::move(caps_unsandboxed), std::move(basic_info_unsandboxed));
    }
#endif  // BUILDFLAG(ENABLE_OOP_PRINTING)

    if (requires_elevated_permissions) {
      sandboxed_print_backend()->AddAccessDeniedPrinter(id);
    } else {
      sandboxed_print_backend()->AddValidPrinter(id, std::move(caps),
                                                 std::move(basic_info));
    }
  }

#if BUILDFLAG(ENABLE_OOP_PRINTING)
  void SetTerminateServiceOnNextInteraction() {
    if (SupportFallback()) {
      unsandboxed_print_backend_service_
          ->SetTerminateReceiverOnNextInteraction();
    }

    sandboxed_print_backend_service_->SetTerminateReceiverOnNextInteraction();
  }
#endif  // BUILDFLAG(ENABLE_OOP_PRINTING)

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  chromeos::TestCupsPrintersManager& printers_manager() {
    DCHECK(printers_manager_);
    return *printers_manager_;
  }

  crosapi::LocalPrinterAsh* local_printer_ash() {
    return local_printer_ash_.get();
  }

 private:
  // Must outlive `profile_`.
  content::BrowserTaskEnvironment task_environment_;

  // Must outlive `printers_manager_`.
  TestingProfile profile_;
  scoped_refptr<TestPrintBackend> sandboxed_test_backend_;
  scoped_refptr<TestPrintBackend> unsandboxed_test_backend_;
  chromeos::TestCupsPrintersManager* printers_manager_;
  scoped_refptr<FakePpdProvider> ppd_provider_;
  std::unique_ptr<crosapi::LocalPrinterAsh> local_printer_ash_;

#if BUILDFLAG(ENABLE_OOP_PRINTING)
  // Support for testing via a service instead of with a local task runner.
  base::test::ScopedFeatureList feature_list_;
  mojo::Remote<mojom::PrintBackendService> sandboxed_test_remote_;
  mojo::Remote<mojom::PrintBackendService> unsandboxed_test_remote_;
  std::unique_ptr<PrintBackendServiceTestImpl> sandboxed_print_backend_service_;
  std::unique_ptr<PrintBackendServiceTestImpl>
      unsandboxed_print_backend_service_;
#endif  // BUILDFLAG(ENABLE_OOP_PRINTING)

  FakeUser user_;
};

// Testing class to cover `LocalPrinterAsh` handling using a local
// task runner.
class LocalPrinterAshTest : public LocalPrinterAshTestBase {
 public:
  LocalPrinterAshTest() = default;
  LocalPrinterAshTest(const LocalPrinterAshTest&) = delete;
  LocalPrinterAshTest& operator=(const LocalPrinterAshTest&) = delete;
  ~LocalPrinterAshTest() override = default;

  bool UseService() override { return false; }
  bool SupportFallback() override { return false; }
};

// Testing class to cover `LocalPrinterAsh` handling using either a
// local task runner or a service.  Makes no attempt to cover fallback when
// using a service, which is handled separately by
// `LocalPrinterAshServiceTest`.
class LocalPrinterAshProcessScopeTest
    : public LocalPrinterAshTestBase,
      public testing::WithParamInterface<bool> {
 public:
  LocalPrinterAshProcessScopeTest() = default;
  LocalPrinterAshProcessScopeTest(const LocalPrinterAshProcessScopeTest&) =
      delete;
  LocalPrinterAshProcessScopeTest& operator=(
      const LocalPrinterAshProcessScopeTest&) = delete;
  ~LocalPrinterAshProcessScopeTest() override = default;

  bool UseService() override { return GetParam(); }
  bool SupportFallback() override { return false; }
};

#if BUILDFLAG(ENABLE_OOP_PRINTING)

// Testing class to cover `LocalPrinterAsh` handling using only a
// service.  This can check different behavior for whether fallback is enabled,
// Mojom data validation conditions, or service termination.
class LocalPrinterAshServiceTest : public LocalPrinterAshTestBase {
 public:
  LocalPrinterAshServiceTest() = default;
  LocalPrinterAshServiceTest(const LocalPrinterAshServiceTest&) = delete;
  LocalPrinterAshServiceTest& operator=(const LocalPrinterAshServiceTest&) =
      delete;
  ~LocalPrinterAshServiceTest() override = default;

  bool UseService() override { return true; }
  bool SupportFallback() override { return true; }
};

INSTANTIATE_TEST_SUITE_P(All, LocalPrinterAshProcessScopeTest, testing::Bool());

#else

// Without OOP printing we only test local test runner configuration.
INSTANTIATE_TEST_SUITE_P(/*no prefix */,
                         LocalPrinterAshProcessScopeTest,
                         testing::Values(false));

#endif  // BUILDFLAG(ENABLE_OOP_PRINTING)

TEST_F(LocalPrinterAshTest, GetStatus) {
  chromeos::CupsPrinterStatus printer1("printer1");
  printer1.AddStatusReason(crosapi::mojom::StatusReason::Reason::kPaperJam,
                           crosapi::mojom::StatusReason::Severity::kError);
  printers_manager().SetPrinterStatus(printer1);
  crosapi::mojom::PrinterStatusPtr printer_status;
  local_printer_ash()->GetStatus(
      "printer1", base::BindOnce(base::BindLambdaForTesting(
                      [&](crosapi::mojom::PrinterStatusPtr status) {
                        printer_status = std::move(status);
                      })));
  auto expected_status = crosapi::mojom::PrinterStatus::New();
  expected_status->printer_id = "printer1";
  expected_status->timestamp = printer1.GetTimestamp();
  expected_status->status_reasons.push_back(crosapi::mojom::StatusReason::New(
      crosapi::mojom::StatusReason::Reason::kPaperJam,
      crosapi::mojom::StatusReason::Severity::kError));
  EXPECT_EQ(expected_status, printer_status);
}

TEST_F(LocalPrinterAshTest, GetPrinters) {
  Printer saved_printer =
      CreateTestPrinter("printer1", "saved", "description1");
  Printer enterprise_printer =
      CreateEnterprisePrinter("printer2", "enterprise", "description2");
  Printer automatic_printer =
      CreateTestPrinter("printer3", "automatic", "description3");

  printers_manager().AddPrinter(saved_printer, PrinterClass::kSaved);
  printers_manager().AddPrinter(enterprise_printer, PrinterClass::kEnterprise);
  printers_manager().AddPrinter(automatic_printer, PrinterClass::kAutomatic);

  std::vector<crosapi::mojom::LocalDestinationInfoPtr> printers;
  local_printer_ash()->GetPrinters(
      base::BindOnce(&RecordPrinterList, std::ref(printers)));

  ASSERT_EQ(3u, printers.size());

  EXPECT_EQ("printer1", printers[0]->id);
  EXPECT_EQ("saved", printers[0]->name);
  EXPECT_EQ("description1", printers[0]->description);
  EXPECT_FALSE(printers[0]->configured_via_policy);
  ASSERT_TRUE(printers[0]->uri);
  EXPECT_EQ(kPrinterUri, *printers[0]->uri);

  EXPECT_EQ("printer2", printers[1]->id);
  EXPECT_EQ("enterprise", printers[1]->name);
  EXPECT_EQ("description2", printers[1]->description);
  EXPECT_TRUE(printers[1]->configured_via_policy);
  ASSERT_TRUE(printers[1]->uri);
  EXPECT_EQ(kPrinterUri, *printers[1]->uri);

  EXPECT_EQ("printer3", printers[2]->id);
  EXPECT_EQ("automatic", printers[2]->name);
  EXPECT_EQ("description3", printers[2]->description);
  EXPECT_FALSE(printers[2]->configured_via_policy);
  ASSERT_TRUE(printers[2]->uri);
  EXPECT_EQ(kPrinterUri, *printers[2]->uri);
}

// Tests that fetching capabilities for an existing installed printer is
// successful.
TEST_P(LocalPrinterAshProcessScopeTest, GetCapabilityValidPrinter) {
  Printer saved_printer =
      CreateTestPrinter("printer1", "saved", "description1");
  printers_manager().AddPrinter(saved_printer, PrinterClass::kSaved);
  printers_manager().InstallPrinter("printer1");

  // Add printer capabilities to `test_backend_`.
  AddPrinter("printer1", "saved", "description1", /*is_default=*/true,
             /*requires_elevated_permissions=*/false);

  crosapi::mojom::CapabilitiesResponsePtr fetched_caps;
  local_printer_ash()->GetCapability(
      "printer1", base::BindOnce(&RecordGetCapability, std::ref(fetched_caps)));

  RunUntilIdle();

  ASSERT_TRUE(fetched_caps);
  EXPECT_FALSE(fetched_caps->has_secure_protocol);
  ASSERT_TRUE(fetched_caps->basic_info);
  EXPECT_EQ("printer1", fetched_caps->basic_info->id);
  EXPECT_EQ("saved", fetched_caps->basic_info->name);
  EXPECT_EQ("description1", fetched_caps->basic_info->description);
  EXPECT_FALSE(fetched_caps->basic_info->configured_via_policy);
  ASSERT_TRUE(fetched_caps->basic_info->uri);
  EXPECT_EQ(kPrinterUri, *fetched_caps->basic_info->uri);

  ASSERT_TRUE(fetched_caps->capabilities);
  EXPECT_EQ(kPapers, fetched_caps->capabilities->papers);
}

// Test that printers which have not yet been installed are installed with
// `SetUpPrinter` before their capabilities are fetched.
TEST_P(LocalPrinterAshProcessScopeTest, GetCapabilityPrinterNotInstalled) {
  Printer discovered_printer =
      CreateTestPrinter("printer1", "discovered", "description1");
  // NOTE: The printer `discovered_printer` is not installed using
  // `InstallPrinter`.
  printers_manager().AddPrinter(discovered_printer, PrinterClass::kDiscovered);

  // Add printer capabilities to `test_backend_`.
  AddPrinter("printer1", "discovered", "description1", /*is_default=*/true,
             /*requires_elevated_permissions=*/false);

  crosapi::mojom::CapabilitiesResponsePtr fetched_caps;

  EXPECT_FALSE(printers_manager().IsPrinterInstalled(discovered_printer));

  // Install printer and fetch capabilities.
  local_printer_ash()->GetCapability(
      "printer1", base::BindOnce(&RecordGetCapability, std::ref(fetched_caps)));

  RunUntilIdle();

  EXPECT_TRUE(printers_manager().IsPrinterInstalled(discovered_printer));
  ASSERT_TRUE(fetched_caps);
  ASSERT_TRUE(fetched_caps->basic_info);
  EXPECT_EQ("printer1", fetched_caps->basic_info->id);
  ASSERT_TRUE(fetched_caps->capabilities);
  EXPECT_EQ(kPapers, fetched_caps->capabilities->papers);
}

// In this test we expect the `GetCapability` to bail early because the
// provided printer can't be found in the `CupsPrintersManager`.
TEST_P(LocalPrinterAshProcessScopeTest, GetCapabilityInvalidPrinter) {
  auto fetched_caps = crosapi::mojom::CapabilitiesResponse::New();
  local_printer_ash()->GetCapability(
      "invalid printer",
      base::BindOnce(&RecordGetCapability, std::ref(fetched_caps)));

  RunUntilIdle();

  EXPECT_FALSE(fetched_caps);
}

// Tests that no capabilities are returned if a printer is unreachable from
// CUPS. We simulate this behavior by not calling AddPrinter(), which registers
// a printer with the test backend.
TEST_P(LocalPrinterAshProcessScopeTest, GetCapabilityUnreachablePrinter) {
  Printer saved_printer =
      CreateTestPrinter("printer1", "saved", "description1");
  printers_manager().AddPrinter(saved_printer, PrinterClass::kSaved);
  printers_manager().InstallPrinter("printer1");

  crosapi::mojom::CapabilitiesResponsePtr fetched_caps;
  local_printer_ash()->GetCapability(
      /*destination_id=*/"printer1",
      base::BindOnce(&RecordGetCapability, std::ref(fetched_caps)));

  RunUntilIdle();

  ASSERT_TRUE(fetched_caps);
  ASSERT_TRUE(fetched_caps->basic_info);
  EXPECT_EQ("printer1", fetched_caps->basic_info->id);
  EXPECT_FALSE(fetched_caps->capabilities);
}

#if BUILDFLAG(ENABLE_OOP_PRINTING)

// Tests that fetching capabilities fails if the print backend service
// terminates early, such as it would from a crash.
TEST_F(LocalPrinterAshServiceTest, GetCapabilityTerminatedService) {
  Printer saved_printer =
      CreateTestPrinter("printer1", "saved", "description1");
  printers_manager().AddPrinter(saved_printer, PrinterClass::kSaved);
  printers_manager().InstallPrinter("printer1");

  // Add printer capabilities to `test_backend_`.
  AddPrinter("printer1", "saved", "description1", /*is_default=*/true,
             /*requires_elevated_permissions=*/false);

  // Set up for service to terminate on next use.
  SetTerminateServiceOnNextInteraction();

  crosapi::mojom::CapabilitiesResponsePtr fetched_caps;
  local_printer_ash()->GetCapability(
      /*destination_id=*/"printer1",
      base::BindOnce(&RecordGetCapability, std::ref(fetched_caps)));

  RunUntilIdle();

  ASSERT_TRUE(fetched_caps);
  ASSERT_TRUE(fetched_caps->basic_info);
  EXPECT_EQ("printer1", fetched_caps->basic_info->id);
  EXPECT_FALSE(fetched_caps->capabilities);
}

#endif  // BUILDFLAG(ENABLE_OOP_PRINTING)

// Test that installed printers to which the user does not have permission to
// access will receive a dictionary for the capabilities but will not have any
// settings in that.
TEST_P(LocalPrinterAshProcessScopeTest, GetCapabilityAccessDenied) {
  Printer saved_printer =
      CreateTestPrinter("printer1", "saved", "description1");
  printers_manager().AddPrinter(saved_printer, PrinterClass::kSaved);
  printers_manager().InstallPrinter("printer1");

  // Add printer capabilities to `test_backend_`.
  AddPrinter("printer1", "saved", "description1", /*is_default=*/true,
             /*requires_elevated_permissions=*/true);

  crosapi::mojom::CapabilitiesResponsePtr fetched_caps;
  local_printer_ash()->GetCapability(
      "printer1", base::BindOnce(&RecordGetCapability, std::ref(fetched_caps)));

  RunUntilIdle();

  ASSERT_TRUE(fetched_caps);
  ASSERT_TRUE(fetched_caps->basic_info);
  EXPECT_EQ("printer1", fetched_caps->basic_info->id);
  EXPECT_FALSE(fetched_caps->capabilities);
}

#if BUILDFLAG(ENABLE_OOP_PRINTING)

TEST_F(LocalPrinterAshServiceTest, GetCapabilityElevatedPermissionsSucceeds) {
  Printer saved_printer =
      CreateTestPrinter("printer1", "saved", "description1");
  printers_manager().AddPrinter(saved_printer, PrinterClass::kSaved);
  printers_manager().InstallPrinter("printer1");

  // Add printer capabilities to `test_backend_`.
  AddPrinter("printer1", "saved", "description1", /*is_default=*/true,
             /*requires_elevated_permissions=*/true);

  // Note that printer does not initially show as requiring elevated privileges.
  EXPECT_FALSE(PrintBackendServiceManager::GetInstance()
                   .PrinterDriverRequiresElevatedPrivilege("printer1"));

  crosapi::mojom::CapabilitiesResponsePtr fetched_caps;
  local_printer_ash()->GetCapability(
      "printer1", base::BindOnce(&RecordGetCapability, std::ref(fetched_caps)));

  RunUntilIdle();

  // Verify that this printer now shows up as requiring elevated privileges.
  EXPECT_TRUE(PrintBackendServiceManager::GetInstance()
                  .PrinterDriverRequiresElevatedPrivilege("printer1"));

  // Getting capabilities should succeed when fallback is supported.
  ASSERT_TRUE(fetched_caps);
  ASSERT_TRUE(fetched_caps->basic_info);
  EXPECT_EQ("printer1", fetched_caps->basic_info->id);
  ASSERT_TRUE(fetched_caps->capabilities);
  EXPECT_EQ(kPapers, fetched_caps->capabilities->papers);
}

#endif  // BUILDFLAG(ENABLE_OOP_PRINTING)

// Test that fetching a PPD license will return a license if the printer has one
// available.
TEST_F(LocalPrinterAshTest, FetchValidEulaUrl) {
  Printer::PpdReference ref;
  ref.effective_make_and_model = "expected_make_model";

  // Printers with a `PpdReference` will return a license
  Printer saved_printer = CreateTestPrinterWithPpdReference(
      "printer1", "saved", "description1", ref);
  printers_manager().AddPrinter(saved_printer, PrinterClass::kSaved);
  printers_manager().InstallPrinter("printer1");

  GURL fetched_eula_url;
  local_printer_ash()->GetEulaUrl(
      "printer1",
      base::BindOnce(&RecordGetEulaUrl, std::ref(fetched_eula_url)));

  RunUntilIdle();

  EXPECT_EQ(fetched_eula_url, GURL("chrome://os-credits/#expected_make_model"));
}

// Test that a printer with no PPD license will return an empty string.
TEST_F(LocalPrinterAshTest, FetchNotFoundEulaUrl) {
  // A printer without a `PpdReference` will simulate a PPD without a license.
  Printer saved_printer =
      CreateTestPrinter("printer1", "saved", "description1");
  printers_manager().AddPrinter(saved_printer, PrinterClass::kSaved);
  printers_manager().InstallPrinter("printer1");

  GURL fetched_eula_url;
  local_printer_ash()->GetEulaUrl(
      "printer1",
      base::BindOnce(&RecordGetEulaUrl, std::ref(fetched_eula_url)));

  RunUntilIdle();

  EXPECT_TRUE(fetched_eula_url.is_empty());
}

// Test that fetching a PPD license will exit early if the printer is not found
// in `CupsPrintersManager`.
TEST_F(LocalPrinterAshTest, FetchEulaUrlOnNonExistantPrinter) {
  Printer saved_printer =
      CreateTestPrinter("printer1", "saved", "description1");

  GURL fetched_eula_url;
  local_printer_ash()->GetEulaUrl(
      "printer1",
      base::BindOnce(&RecordGetEulaUrl, std::ref(fetched_eula_url)));

  RunUntilIdle();

  EXPECT_TRUE(fetched_eula_url.is_empty());
}

TEST_F(LocalPrinterAshTest, GetPolicies_Unset) {
  crosapi::mojom::PoliciesPtr policies;
  local_printer_ash()->GetPolicies(base::BindOnce(base::BindLambdaForTesting(
      [&](crosapi::mojom::PoliciesPtr data) { policies = std::move(data); })));
  EXPECT_EQ(crosapi::mojom::Policies::New(), policies);
}

TEST_F(LocalPrinterAshTest, GetPolicies_PaperSize) {
  auto* prefs = GetPrefs();
  base::Value paper_size(base::Value::Type::DICTIONARY);
  paper_size.SetStringKey(kPaperSizeName, "iso_a4_210x297mm");
  prefs->Set("printing.paper_size_default", std::move(paper_size));

  crosapi::mojom::PoliciesPtr policies;
  local_printer_ash()->GetPolicies(base::BindOnce(base::BindLambdaForTesting(
      [&](crosapi::mojom::PoliciesPtr data) { policies = std::move(data); })));

  ASSERT_TRUE(policies);
  EXPECT_EQ(gfx::Size(210000, 297000), policies->paper_size_default);
}

TEST_F(LocalPrinterAshTest, GetPolicies_BackgroundGraphics) {
  auto* prefs = GetPrefs();
  // crosapi::mojom::Policies::BackgroundGraphicsModeRestriction::kDisabled
  prefs->SetInteger(prefs::kPrintingAllowedBackgroundGraphicsModes, 2);
  // crosapi::mojom::Policies::BackgroundGraphicsModeRestriction::kEnabled
  prefs->SetInteger(prefs::kPrintingBackgroundGraphicsDefault, 1);

  crosapi::mojom::PoliciesPtr policies;
  local_printer_ash()->GetPolicies(base::BindOnce(base::BindLambdaForTesting(
      [&](crosapi::mojom::PoliciesPtr data) { policies = std::move(data); })));

  ASSERT_TRUE(policies);
  EXPECT_EQ(
      crosapi::mojom::Policies::BackgroundGraphicsModeRestriction::kDisabled,
      policies->allowed_background_graphics_modes);
  EXPECT_EQ(
      crosapi::mojom::Policies::BackgroundGraphicsModeRestriction::kEnabled,
      policies->background_graphics_default);
}

TEST_F(LocalPrinterAshTest, GetPolicies_MaxSheetsAllowed) {
  auto* prefs = GetPrefs();
  prefs->SetInteger(prefs::kPrintingMaxSheetsAllowed, 5);

  crosapi::mojom::PoliciesPtr policies;
  local_printer_ash()->GetPolicies(base::BindOnce(base::BindLambdaForTesting(
      [&](crosapi::mojom::PoliciesPtr data) { policies = std::move(data); })));

  EXPECT_TRUE(policies->max_sheets_allowed_has_value);
  EXPECT_EQ(5, policies->max_sheets_allowed);
}

// Zero sheets allowed is a valid policy.
TEST_F(LocalPrinterAshTest, GetPolicies_ZeroSheetsAllowed) {
  auto* prefs = GetPrefs();
  prefs->SetInteger(prefs::kPrintingMaxSheetsAllowed, 0);

  crosapi::mojom::PoliciesPtr policies;
  local_printer_ash()->GetPolicies(base::BindOnce(base::BindLambdaForTesting(
      [&](crosapi::mojom::PoliciesPtr data) { policies = std::move(data); })));

  ASSERT_TRUE(policies);
  EXPECT_TRUE(policies->max_sheets_allowed_has_value);
  EXPECT_EQ(0, policies->max_sheets_allowed);
}

// Negative sheets allowed is not a valid policy.
TEST_F(LocalPrinterAshTest, GetPolicies_NegativeMaxSheets) {
  auto* prefs = GetPrefs();
  prefs->SetInteger(prefs::kPrintingMaxSheetsAllowed, -1);

  crosapi::mojom::PoliciesPtr policies;
  local_printer_ash()->GetPolicies(base::BindOnce(base::BindLambdaForTesting(
      [&](crosapi::mojom::PoliciesPtr data) { policies = std::move(data); })));

  ASSERT_TRUE(policies);
  EXPECT_FALSE(policies->max_sheets_allowed_has_value);
}

TEST_F(LocalPrinterAshTest, GetPolicies_PrintHeaderFooter_UnmanagedDisabled) {
  auto* prefs = GetPrefs();
  prefs->SetBoolean(prefs::kPrintHeaderFooter, false);
  crosapi::mojom::PoliciesPtr policies;
  local_printer_ash()->GetPolicies(base::BindOnce(base::BindLambdaForTesting(
      [&](crosapi::mojom::PoliciesPtr data) { policies = std::move(data); })));
  ASSERT_TRUE(policies);
  EXPECT_EQ(crosapi::mojom::Policies::OptionalBool::kUnset,
            policies->print_header_footer_allowed);
  EXPECT_EQ(crosapi::mojom::Policies::OptionalBool::kFalse,
            policies->print_header_footer_default);
}

TEST_F(LocalPrinterAshTest, GetPolicies_PrintHeaderFooter_UnmanagedEnabled) {
  auto* prefs = GetPrefs();
  prefs->SetBoolean(prefs::kPrintHeaderFooter, true);
  crosapi::mojom::PoliciesPtr policies;
  local_printer_ash()->GetPolicies(base::BindOnce(base::BindLambdaForTesting(
      [&](crosapi::mojom::PoliciesPtr data) { policies = std::move(data); })));
  ASSERT_TRUE(policies);
  EXPECT_EQ(crosapi::mojom::Policies::OptionalBool::kUnset,
            policies->print_header_footer_allowed);
  EXPECT_EQ(crosapi::mojom::Policies::OptionalBool::kTrue,
            policies->print_header_footer_default);
}

TEST_F(LocalPrinterAshTest, GetPolicies_PrintHeaderFooter_ManagedDisabled) {
  auto* prefs = GetPrefs();
  prefs->SetManagedPref(prefs::kPrintHeaderFooter,
                        std::make_unique<base::Value>(false));
  crosapi::mojom::PoliciesPtr policies;
  local_printer_ash()->GetPolicies(base::BindOnce(base::BindLambdaForTesting(
      [&](crosapi::mojom::PoliciesPtr data) { policies = std::move(data); })));
  ASSERT_TRUE(policies);
  EXPECT_EQ(crosapi::mojom::Policies::OptionalBool::kFalse,
            policies->print_header_footer_allowed);
  EXPECT_EQ(crosapi::mojom::Policies::OptionalBool::kUnset,
            policies->print_header_footer_default);
}

TEST_F(LocalPrinterAshTest, GetPolicies_PrintHeaderFooter_ManagedEnabled) {
  auto* prefs = GetPrefs();
  prefs->SetManagedPref(prefs::kPrintHeaderFooter,
                        std::make_unique<base::Value>(true));
  crosapi::mojom::PoliciesPtr policies;
  local_printer_ash()->GetPolicies(base::BindOnce(base::BindLambdaForTesting(
      [&](crosapi::mojom::PoliciesPtr data) { policies = std::move(data); })));
  ASSERT_TRUE(policies);
  EXPECT_EQ(crosapi::mojom::Policies::OptionalBool::kTrue,
            policies->print_header_footer_allowed);
  EXPECT_EQ(crosapi::mojom::Policies::OptionalBool::kUnset,
            policies->print_header_footer_default);
}

TEST_F(LocalPrinterAshTest, GetPolicies_Color) {
  const uint32_t expected_allowed_color_modes = static_cast<uint32_t>(
      static_cast<int32_t>(printing::mojom::ColorModeRestriction::kMonochrome) |
      static_cast<int32_t>(printing::mojom::ColorModeRestriction::kColor));
  auto* prefs = GetPrefs();
  prefs->SetInteger(prefs::kPrintingAllowedColorModes, 3);
  prefs->SetInteger(prefs::kPrintingColorDefault, 2);

  crosapi::mojom::PoliciesPtr policies;
  local_printer_ash()->GetPolicies(base::BindOnce(base::BindLambdaForTesting(
      [&](crosapi::mojom::PoliciesPtr data) { policies = std::move(data); })));

  EXPECT_EQ(expected_allowed_color_modes, policies->allowed_color_modes);
  EXPECT_EQ(printing::mojom::ColorModeRestriction::kColor,
            policies->default_color_mode);
}

TEST_F(LocalPrinterAshTest, GetPolicies_Duplex) {
  const uint32_t expected_allowed_duplex_modes = static_cast<uint32_t>(
      static_cast<int32_t>(printing::mojom::DuplexModeRestriction::kSimplex) |
      static_cast<int32_t>(printing::mojom::DuplexModeRestriction::kDuplex));
  auto* prefs = GetPrefs();
  prefs->SetInteger(prefs::kPrintingAllowedDuplexModes, 7);
  prefs->SetInteger(prefs::kPrintingDuplexDefault, 1);

  crosapi::mojom::PoliciesPtr policies;
  local_printer_ash()->GetPolicies(base::BindOnce(base::BindLambdaForTesting(
      [&](crosapi::mojom::PoliciesPtr data) { policies = std::move(data); })));

  EXPECT_EQ(expected_allowed_duplex_modes, policies->allowed_duplex_modes);
  EXPECT_EQ(printing::mojom::DuplexModeRestriction::kSimplex,
            policies->default_duplex_mode);
}

TEST_F(LocalPrinterAshTest, GetPolicies_Pin) {
  auto* prefs = GetPrefs();
  prefs->SetInteger(prefs::kPrintingAllowedPinModes, 1);
  prefs->SetInteger(prefs::kPrintingPinDefault, 2);

  crosapi::mojom::PoliciesPtr policies;
  local_printer_ash()->GetPolicies(base::BindOnce(base::BindLambdaForTesting(
      [&](crosapi::mojom::PoliciesPtr data) { policies = std::move(data); })));

  EXPECT_EQ(printing::mojom::PinModeRestriction::kPin,
            policies->allowed_pin_modes);
  EXPECT_EQ(printing::mojom::PinModeRestriction::kNoPin,
            policies->default_pin_mode);
}

TEST_F(LocalPrinterAshTest, GetUsernamePerPolicy_Allowed) {
  SetUsername("user@email.com");
  GetPrefs()->SetBoolean(prefs::kPrintingSendUsernameAndFilenameEnabled, true);
  absl::optional<std::string> username;
  local_printer_ash()->GetUsernamePerPolicy(base::BindOnce(
      base::BindLambdaForTesting([&](const absl::optional<std::string>& uname) {
        username = uname;
      })));
  ASSERT_TRUE(username);
  EXPECT_EQ("user@email.com", *username);
}

TEST_F(LocalPrinterAshTest, GetUsernamePerPolicy_Denied) {
  SetUsername("user@email.com");
  GetPrefs()->SetBoolean(prefs::kPrintingSendUsernameAndFilenameEnabled, false);
  absl::optional<std::string> username;
  local_printer_ash()->GetUsernamePerPolicy(base::BindOnce(
      base::BindLambdaForTesting([&](const absl::optional<std::string>& uname) {
        username = uname;
      })));
  EXPECT_EQ(absl::nullopt, username);
}

TEST(LocalPrinterAsh, ConfigToMojom) {
  chromeos::PrintServersConfig config;
  config.fetching_mode = crosapi::mojom::PrintServersConfig::
      ServerPrintersFetchingMode::kSingleServerOnly;
  config.print_servers.push_back(
      chromeos::PrintServer("id", GURL("http://localhost"), "name"));
  crosapi::mojom::PrintServersConfigPtr mojom =
      crosapi::LocalPrinterAsh::ConfigToMojom(config);
  ASSERT_TRUE(mojom);
  EXPECT_EQ(crosapi::mojom::PrintServersConfig::ServerPrintersFetchingMode::
                kSingleServerOnly,
            mojom->fetching_mode);
  ASSERT_EQ(1u, mojom->print_servers.size());
  EXPECT_EQ("id", mojom->print_servers[0]->id);
  EXPECT_EQ(GURL("http://localhost"), mojom->print_servers[0]->url);
  EXPECT_EQ("name", mojom->print_servers[0]->name);
}

TEST(LocalPrinterAsh, PrinterToMojom) {
  Printer printer("id");
  printer.set_display_name("name");
  printer.set_description("description");
  crosapi::mojom::LocalDestinationInfoPtr mojom =
      crosapi::LocalPrinterAsh::PrinterToMojom(printer);
  ASSERT_TRUE(mojom);
  EXPECT_EQ("id", mojom->id);
  EXPECT_EQ("name", mojom->name);
  EXPECT_EQ("description", mojom->description);
  EXPECT_FALSE(mojom->configured_via_policy);
}

TEST(LocalPrinterAsh, PrinterToMojom_ConfiguredViaPolicy) {
  Printer printer("id");
  printer.set_source(Printer::SRC_POLICY);
  crosapi::mojom::LocalDestinationInfoPtr mojom =
      crosapi::LocalPrinterAsh::PrinterToMojom(printer);
  ASSERT_TRUE(mojom);
  EXPECT_EQ("id", mojom->id);
  EXPECT_TRUE(mojom->configured_via_policy);
}

TEST(LocalPrinterAsh, StatusToMojom) {
  chromeos::CupsPrinterStatus status("id");
  status.AddStatusReason(crosapi::mojom::StatusReason::Reason::kOutOfInk,
                         crosapi::mojom::StatusReason::Severity::kWarning);
  crosapi::mojom::PrinterStatusPtr mojom =
      crosapi::LocalPrinterAsh::StatusToMojom(status);
  ASSERT_TRUE(mojom);
  EXPECT_EQ("id", mojom->printer_id);
  EXPECT_EQ(status.GetTimestamp(), mojom->timestamp);
  ASSERT_EQ(1u, mojom->status_reasons.size());
  EXPECT_EQ(crosapi::mojom::StatusReason::Reason::kOutOfInk,
            mojom->status_reasons[0]->reason);
  EXPECT_EQ(crosapi::mojom::StatusReason::Severity::kWarning,
            mojom->status_reasons[0]->severity);
}

}  // namespace printing
