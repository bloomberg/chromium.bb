// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/cups_printers_handler.h"

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/json/json_string_value_serializer.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon_client.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace settings {

class CupsPrintersHandlerTest;

namespace {

// Converts JSON string to base::ListValue object.
// On failure, returns NULL and fills |*error| string.
std::unique_ptr<base::ListValue> GetJSONAsListValue(const std::string& json,
                                                    std::string* error) {
  auto ret = base::ListValue::From(
      JSONStringValueDeserializer(json).Deserialize(nullptr, error));
  if (!ret)
    *error = "Value is not a list.";
  return ret;
}

}  // namespace

// Callback used for testing CupsAddAutoConfiguredPrinter().
void AddedPrinter(int32_t status) {
  ASSERT_EQ(status, 0);
}

// Callback used for testing CupsRemovePrinter().
void RemovedPrinter(base::OnceClosure quit_closure,
                    bool* expected,
                    bool result) {
  *expected = result;
  std::move(quit_closure).Run();
}

class FakePrinterConfigurer : public PrinterConfigurer {
 public:
  void SetUpPrinter(const Printer& printer,
                    PrinterSetupCallback callback) override {}
};

class FakeCupsPrintersManager : public CupsPrintersManager {
 public:
  FakeCupsPrintersManager() = default;

  std::vector<Printer> GetPrinters(PrinterClass printer_class) const override {
    return {};
  }

  void RemoveUnavailablePrinters(std::vector<Printer>*) const override {}
  void UpdateSavedPrinter(const Printer& printer) override {}
  void RemoveSavedPrinter(const std::string& printer_id) override {}
  void AddObserver(CupsPrintersManager::Observer* observer) override {}
  void RemoveObserver(CupsPrintersManager::Observer* observer) override {}
  void PrinterInstalled(const Printer& printer, bool is_automatic) override {}
  void RecordSetupAbandoned(const Printer& printer) override {}

  bool IsPrinterInstalled(const Printer& printer) const override {
    return false;
  }

  base::Optional<Printer> GetPrinter(const std::string& id) const override {
    return Printer();
  }
};

class FakePpdProvider : public PpdProvider {
 public:
  FakePpdProvider() = default;

  void ResolveManufacturers(ResolveManufacturersCallback cb) override {}
  void ResolvePrinters(const std::string& manufacturer,
                       ResolvePrintersCallback cb) override {}
  void ResolvePpdReference(const PrinterSearchData& search_data,
                           ResolvePpdReferenceCallback cb) override {}
  void ResolvePpd(const Printer::PpdReference& reference,
                  ResolvePpdCallback cb) override {}
  void ReverseLookup(const std::string& effective_make_and_model,
                     ReverseLookupCallback cb) override {}

 private:
  ~FakePpdProvider() override {}
};

class CupsPrintersHandlerTest : public testing::Test {
 public:
  CupsPrintersHandlerTest()
      : thread_bundle_(content::TestBrowserThreadBundle::REAL_IO_THREAD),
        profile_(),
        web_ui_(),
        printers_handler_() {}
  ~CupsPrintersHandlerTest() override = default;

  void SetUp() override {
    printers_handler_ = CupsPrintersHandler::CreateForTesting(
        &profile_, base::MakeRefCounted<FakePpdProvider>(),
        std::make_unique<FakePrinterConfigurer>(), &printers_manager_);
    printers_handler_->SetWebUIForTest(&web_ui_);
    printers_handler_->RegisterMessages();
  }

 protected:
  // Must outlive |profile_|.
  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;
  content::TestWebUI web_ui_;
  std::unique_ptr<CupsPrintersHandler> printers_handler_;
  FakeCupsPrintersManager printers_manager_;
};

TEST_F(CupsPrintersHandlerTest, RemoveCorrectPrinter) {
  DBusThreadManager::Initialize();
  DebugDaemonClient* client = DBusThreadManager::Get()->GetDebugDaemonClient();
  client->CupsAddAutoConfiguredPrinter("testprinter1", "fakeuri",
                                       base::BindOnce(&AddedPrinter));

  const std::string remove_list = R"(
    ["testprinter1", "Test Printer 1"]
  )";
  std::string error;
  std::unique_ptr<base::ListValue> remove_printers(
      GetJSONAsListValue(remove_list, &error));
  ASSERT_TRUE(remove_printers) << "Error deserializing list: " << error;

  web_ui_.HandleReceivedMessage("removeCupsPrinter", remove_printers.get());

  // We expect this printer removal to fail since the printer should have
  // already been removed by the previous call to 'removeCupsPrinter'.
  base::RunLoop run_loop;
  bool expected = true;
  client->CupsRemovePrinter(
      "testprinter1",
      base::BindRepeating(&RemovedPrinter, run_loop.QuitClosure(), &expected),
      base::DoNothing());
  run_loop.Run();
  EXPECT_FALSE(expected);
}

}  // namespace settings.
}  // namespace chromeos.
