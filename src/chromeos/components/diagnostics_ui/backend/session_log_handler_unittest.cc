// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/diagnostics_ui/backend/session_log_handler.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/strcat.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chromeos/components/diagnostics_ui/backend/log_test_helpers.h"
#include "chromeos/components/diagnostics_ui/backend/routine_log.h"
#include "chromeos/components/diagnostics_ui/backend/telemetry_log.h"
#include "chromeos/components/diagnostics_ui/mojom/system_data_provider.mojom.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/select_file_dialog_factory.h"
#include "ui/shell_dialogs/select_file_policy.h"

namespace chromeos {
namespace diagnostics {
namespace {

constexpr char kHandlerFunctionName[] = "handlerFunctionName";
constexpr char kRoutineLogFileName[] = "diagnostic_routine_log";

mojom::SystemInfoPtr CreateSystemInfoPtr(const std::string& board_name,
                                         const std::string& marketing_name,
                                         const std::string& cpu_model,
                                         uint32_t total_memory_kib,
                                         uint16_t cpu_threads_count,
                                         uint32_t cpu_max_clock_speed_khz,
                                         bool has_battery,
                                         const std::string& milestone_version,
                                         const std::string& full_version) {
  auto version_info = mojom::VersionInfo::New(milestone_version, full_version);
  auto device_capabilities = mojom::DeviceCapabilities::New(has_battery);

  auto system_info = mojom::SystemInfo::New(
      board_name, marketing_name, cpu_model, total_memory_kib,
      cpu_threads_count, cpu_max_clock_speed_khz, std::move(version_info),
      std::move(device_capabilities));
  return system_info;
}

std::vector<std::string> GetCombinedLogContents(
    const base::FilePath& log_path) {
  std::string contents;
  base::ReadFileToString(log_path, &contents);
  return GetLogLines(contents);
}

}  // namespace

class TestSelectFilePolicy : public ui::SelectFilePolicy {
 public:
  TestSelectFilePolicy& operator=(const TestSelectFilePolicy&) = delete;

  bool CanOpenSelectFileDialog() override { return true; }
  void SelectFileDenied() override {}
};

// A fake SelectFilePolicyCreator.
std::unique_ptr<ui::SelectFilePolicy> CreateTestSelectFilePolicy(
    content::WebContents* web_contents) {
  return std::make_unique<TestSelectFilePolicy>();
}

// A fake ui::SelectFileDialog.
class TestSelectFileDialog : public ui::SelectFileDialog {
 public:
  TestSelectFileDialog(Listener* listener,
                       std::unique_ptr<ui::SelectFilePolicy> policy,
                       base::FilePath selected_path)
      : ui::SelectFileDialog(listener, std::move(policy)),
        selected_path_(selected_path) {}

  TestSelectFileDialog(const TestSelectFileDialog&) = delete;
  TestSelectFileDialog& operator=(const TestSelectFileDialog&) = delete;

 protected:
  void SelectFileImpl(Type type,
                      const base::string16& title,
                      const base::FilePath& default_path,
                      const FileTypeInfo* file_types,
                      int file_type_index,
                      const base::FilePath::StringType& default_extension,
                      gfx::NativeWindow owning_window,
                      void* params) override {
    if (selected_path_.empty()) {
      listener_->FileSelectionCanceled(params);
      return;
    }

    listener_->FileSelected(selected_path_, /*index=*/0,
                            /*params=*/nullptr);
  }

  bool IsRunning(gfx::NativeWindow owning_window) const override {
    return true;
  }
  void ListenerDestroyed() override {}
  bool HasMultipleFileTypeChoicesImpl() override { return false; }

 private:
  ~TestSelectFileDialog() override = default;

  // The simulatd file path selected by the user.
  base::FilePath selected_path_;
};

// A factory associated with the artificial file picker.
class TestSelectFileDialogFactory : public ui::SelectFileDialogFactory {
 public:
  explicit TestSelectFileDialogFactory(base::FilePath selected_path)
      : selected_path_(selected_path) {}

  ui::SelectFileDialog* Create(
      ui::SelectFileDialog::Listener* listener,
      std::unique_ptr<ui::SelectFilePolicy> policy) override {
    return new TestSelectFileDialog(listener, std::move(policy),
                                    selected_path_);
  }

  TestSelectFileDialogFactory(const TestSelectFileDialogFactory&) = delete;
  TestSelectFileDialogFactory& operator=(const TestSelectFileDialogFactory&) =
      delete;

 private:
  // The simulated file path selected by the user.
  base::FilePath selected_path_;
};

class SessionLogHandlerTest : public testing::Test {
 public:
  SessionLogHandlerTest()
      : task_environment_(content::BrowserTaskEnvironment::REAL_IO_THREAD),
        web_ui_(),
        session_log_handler_() {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    base::FilePath routine_log_path =
        temp_dir_.GetPath().AppendASCII(kRoutineLogFileName);
    auto telemetry_log = std::make_unique<TelemetryLog>();
    auto routine_log = std::make_unique<RoutineLog>(routine_log_path);
    telemetry_log_ = telemetry_log.get();
    routine_log_ = routine_log.get();
    session_log_handler_ = std::make_unique<diagnostics::SessionLogHandler>(
        base::BindRepeating(&CreateTestSelectFilePolicy),
        std::move(telemetry_log), std::move(routine_log));
    session_log_handler_->SetWebUIForTest(&web_ui_);
    session_log_handler_->RegisterMessages();

    base::ListValue args;
    web_ui_.HandleReceivedMessage("initialize", &args);
  }

  ~SessionLogHandlerTest() override {
    ui::SelectFileDialog::SetFactory(nullptr);
  }

  const content::TestWebUI::CallData& CallDataAtIndex(size_t index) {
    return *web_ui_.call_data()[index];
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  content::TestWebUI web_ui_;
  std::unique_ptr<diagnostics::SessionLogHandler> session_log_handler_;
  TelemetryLog* telemetry_log_;
  RoutineLog* routine_log_;
  base::ScopedTempDir temp_dir_;
};

TEST_F(SessionLogHandlerTest, SaveSessionLog) {
  // Populate routine log
  routine_log_->LogRoutineStarted(mojom::RoutineType::kCpuStress);

  // Populate telemetry log
  const std::string expected_board_name = "board_name";
  const std::string expected_marketing_name = "marketing_name";
  const std::string expected_cpu_model = "cpu_model";
  const uint32_t expected_total_memory_kib = 1234;
  const uint16_t expected_cpu_threads_count = 5678;
  const uint32_t expected_cpu_max_clock_speed_khz = 91011;
  const bool expected_has_battery = true;
  const std::string expected_milestone_version = "M99";
  const std::string expected_full_version = "M99.1234.5.6";
  mojom::SystemInfoPtr test_info = CreateSystemInfoPtr(
      expected_board_name, expected_marketing_name, expected_cpu_model,
      expected_total_memory_kib, expected_cpu_threads_count,
      expected_cpu_max_clock_speed_khz, expected_has_battery,
      expected_milestone_version, expected_full_version);

  telemetry_log_->UpdateSystemInfo(std::move(test_info));

  // Select file
  base::FilePath log_path = temp_dir_.GetPath().AppendASCII("test_path");
  ui::SelectFileDialog::SetFactory(new TestSelectFileDialogFactory(log_path));
  base::ListValue args;
  args.Append(kHandlerFunctionName);
  web_ui_.HandleReceivedMessage("saveSessionLog", &args);

  const std::string expected_telemetry_log_header = "=== Telemetry Log ===";
  const std::string expected_system_info_section_name = "--- System Info ---";
  const std::string expected_snapshot_time_prefix = "Snapshot Time: ";
  const std::vector<std::string> log_lines = GetCombinedLogContents(log_path);
  ASSERT_EQ(13u, log_lines.size());
  EXPECT_EQ(expected_telemetry_log_header, log_lines[0]);
  EXPECT_EQ(expected_system_info_section_name, log_lines[1]);
  EXPECT_GT(log_lines[2].size(), expected_snapshot_time_prefix.size());
  EXPECT_TRUE(base::StartsWith(log_lines[2], expected_snapshot_time_prefix));
  EXPECT_EQ("Board Name: " + expected_board_name, log_lines[3]);
  EXPECT_EQ("Marketing Name: " + expected_marketing_name, log_lines[4]);
  EXPECT_EQ("CpuModel Name: " + expected_cpu_model, log_lines[5]);
  EXPECT_EQ(
      "Total Memory (kib): " + base::NumberToString(expected_total_memory_kib),
      log_lines[6]);
  EXPECT_EQ(
      "Thread Count:  " + base::NumberToString(expected_cpu_threads_count),
      log_lines[7]);
  EXPECT_EQ("Cpu Max Clock Speed (kHz):  " +
                base::NumberToString(expected_cpu_max_clock_speed_khz),
            log_lines[8]);
  EXPECT_EQ("Version: " + expected_full_version, log_lines[9]);
  EXPECT_EQ("Has Battery: true", log_lines[10]);

  const std::string expected_routine_log_header = "=== Routine Log ===";
  EXPECT_EQ(expected_routine_log_header, log_lines[11]);

  const std::vector<std::string> first_routine_log_line_contents =
      GetLogLineContents(log_lines[12]);
  ASSERT_EQ(3u, first_routine_log_line_contents.size());
  // first_routine_log_line_contents[0] is ignored because it's a timestamp.
  EXPECT_EQ("CpuStress", first_routine_log_line_contents[1]);
  EXPECT_EQ("Started", first_routine_log_line_contents[2]);
}

// Validates that invoking the saveSessionLog Web UI event opens the
// select dialog. Choosing a directory should return that the operation
// was successful.
TEST_F(SessionLogHandlerTest, SelectDirectory) {
  base::FilePath log_path = temp_dir_.GetPath().AppendASCII("test_path");
  ui::SelectFileDialog::SetFactory(new TestSelectFileDialogFactory(log_path));

  const size_t call_data_count_before_call = web_ui_.call_data().size();
  base::ListValue args;
  args.Append(kHandlerFunctionName);
  web_ui_.HandleReceivedMessage("saveSessionLog", &args);

  EXPECT_EQ(call_data_count_before_call + 1u, web_ui_.call_data().size());
  const content::TestWebUI::CallData& call_data =
      CallDataAtIndex(call_data_count_before_call);
  EXPECT_EQ("cr.webUIResponse", call_data.function_name());
  EXPECT_EQ("handlerFunctionName", call_data.arg1()->GetString());
  EXPECT_TRUE(/*success=*/call_data.arg2()->GetBool());
}

TEST_F(SessionLogHandlerTest, CancelDialog) {
  // A dialog returning an empty file path simulates the user closing the
  // dialog without selecting a path.
  ui::SelectFileDialog::SetFactory(
      new TestSelectFileDialogFactory(base::FilePath()));

  const size_t call_data_count_before_call = web_ui_.call_data().size();
  base::ListValue args;
  args.Append(kHandlerFunctionName);
  web_ui_.HandleReceivedMessage("saveSessionLog", &args);

  EXPECT_EQ(call_data_count_before_call + 1u, web_ui_.call_data().size());
  const content::TestWebUI::CallData& call_data =
      CallDataAtIndex(call_data_count_before_call);
  EXPECT_EQ("cr.webUIResponse", call_data.function_name());
  EXPECT_EQ("handlerFunctionName", call_data.arg1()->GetString());
  EXPECT_FALSE(/*success=*/call_data.arg2()->GetBool());
}

}  // namespace diagnostics
}  // namespace chromeos