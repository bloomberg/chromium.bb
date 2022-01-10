// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <set>
#include <string>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_delegate.h"
#include "chrome/browser/enterprise/connectors/analysis/fake_content_analysis_delegate.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_test_utils.h"
#include "chrome/browser/ui/tab_contents/chrome_web_contents_view_handle_drop.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/drop_data.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class ChromeWebContentsViewDelegateHandleOnPerformDrop : public testing::Test {
 public:
  ChromeWebContentsViewDelegateHandleOnPerformDrop() {
    EXPECT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("test-user");
    scoped_feature_list_.InitWithFeatures(
        {enterprise_connectors::kEnterpriseConnectorsEnabled}, {});
  }

  void RunUntilDone() { run_loop_->Run(); }

  content::WebContents* contents() {
    if (!web_contents_) {
      content::WebContents::CreateParams params(profile_);
      web_contents_ = content::WebContents::Create(params);
    }
    return web_contents_.get();
  }

  void EnableDeepScanning(bool enable, bool scan_succeeds) {
    if (enable) {
      static constexpr char kEnabled[] = R"(
          {
              "service_provider": "google",
              "enable": [
                {
                  "url_list": ["*"],
                  "tags": ["dlp"]
                }
              ],
              "block_until_verdict": 1
          })";
      safe_browsing::SetAnalysisConnector(
          profile_->GetPrefs(), enterprise_connectors::FILE_ATTACHED, kEnabled);
      safe_browsing::SetAnalysisConnector(
          profile_->GetPrefs(), enterprise_connectors::BULK_DATA_ENTRY,
          kEnabled);
    } else {
      safe_browsing::ClearAnalysisConnector(
          profile_->GetPrefs(), enterprise_connectors::FILE_ATTACHED);
      safe_browsing::ClearAnalysisConnector(
          profile_->GetPrefs(), enterprise_connectors::BULK_DATA_ENTRY);
    }

    run_loop_ = std::make_unique<base::RunLoop>();

    using FakeDelegate = enterprise_connectors::FakeContentAnalysisDelegate;
    auto is_encrypted_callback =
        base::BindRepeating([](const base::FilePath&) { return false; });

    policy::SetDMTokenForTesting(
        policy::DMToken::CreateValidTokenForTesting("dm_token"));
    auto callback = base::BindLambdaForTesting(
        [this, scan_succeeds](const base::FilePath&)
            -> enterprise_connectors::ContentAnalysisResponse {
          std::set<std::string> dlp_tag = {"dlp"};
          current_requests_count_++;
          return scan_succeeds
                     ? FakeDelegate::SuccessfulResponse(std::move(dlp_tag))
                     : FakeDelegate::DlpResponse(
                           enterprise_connectors::ContentAnalysisResponse::
                               Result::SUCCESS,
                           "block_rule",
                           enterprise_connectors::ContentAnalysisResponse::
                               Result::TriggeredRule::BLOCK);
        });
    enterprise_connectors::ContentAnalysisDelegate::SetFactoryForTesting(
        base::BindRepeating(
            &enterprise_connectors::FakeContentAnalysisDelegate::Create,
            run_loop_->QuitClosure(), callback, is_encrypted_callback,
            "dm_token"));
    enterprise_connectors::ContentAnalysisDelegate::DisableUIForTesting();
  }

  // Common code for running the test cases.
  void RunTest(const content::DropData& data,
               bool enable,
               bool scan_succeeds = false) {
    current_requests_count_ = 0;
    EnableDeepScanning(enable, scan_succeeds);

    content::WebContentsViewDelegate::DropCompletionResult result =
        scan_succeeds
            ? content::WebContentsViewDelegate::DropCompletionResult::kContinue
            : content::WebContentsViewDelegate::DropCompletionResult::kAbort;
    bool called = false;
    HandleOnPerformDrop(
        contents(), data,
        base::BindOnce(
            [](content::WebContentsViewDelegate::DropCompletionResult
                   expected_result,
               bool* called,
               content::WebContentsViewDelegate::DropCompletionResult result) {
              EXPECT_EQ(expected_result, result);
              *called = true;
            },
            result, &called));
    if (enable)
      RunUntilDone();

    EXPECT_TRUE(called);
    ASSERT_EQ(expected_requests_count_, current_requests_count_);
  }

  void SetExpectedRequestsCount(int count) { expected_requests_count_ = count; }

  // Helpers to get text with sizes relative to the minimum required size of 100
  // bytes for scans to trigger.
  std::string large_text() const { return std::string(100, 'a'); }

  std::string small_text() const { return "random small text"; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  TestingProfileManager profile_manager_{TestingBrowserProcess::GetGlobal()};
  raw_ptr<TestingProfile> profile_;
  std::unique_ptr<base::RunLoop> run_loop_;
  std::unique_ptr<content::WebContents> web_contents_;
  int expected_requests_count_ = 0;
  int current_requests_count_ = 0;
};

// When no drop data is specified, HandleOnPerformDrop() should indicate
// the caller can proceed, whether scanning is enabled or not.
TEST_F(ChromeWebContentsViewDelegateHandleOnPerformDrop, NoData) {
  content::DropData data;

  SetExpectedRequestsCount(0);
  RunTest(data, /*enable=*/false, /*scan_succeeds=*/true);
  RunTest(data, /*enable=*/true, /*scan_succeeds=*/true);
}

// Make sure DropData::url_title is handled correctly.
TEST_F(ChromeWebContentsViewDelegateHandleOnPerformDrop, UrlTitle) {
  content::DropData data;
  data.url_title = base::UTF8ToUTF16(large_text());

  SetExpectedRequestsCount(0);
  RunTest(data, /*enable=*/false, /*scan_succeeds=*/true);

  SetExpectedRequestsCount(1);
  RunTest(data, /*enable=*/true, /*scan_succeeds=*/false);
  RunTest(data, /*enable=*/true, /*scan_succeeds=*/true);

  data.url_title = base::UTF8ToUTF16(small_text());
  SetExpectedRequestsCount(0);
  RunTest(data, /*enable=*/true, /*scan_succeeds=*/true);
}

// Make sure DropData::text is handled correctly.
TEST_F(ChromeWebContentsViewDelegateHandleOnPerformDrop, Text) {
  content::DropData data;
  data.text = base::UTF8ToUTF16(large_text());

  SetExpectedRequestsCount(0);
  RunTest(data, /*enable=*/false, /*scan_succeeds=*/true);

  SetExpectedRequestsCount(1);
  RunTest(data, /*enable=*/true, /*scan_succeeds=*/false);
  RunTest(data, /*enable=*/true, /*scan_succeeds=*/true);

  data.text = base::UTF8ToUTF16(small_text());
  SetExpectedRequestsCount(0);
  RunTest(data, /*enable=*/true, /*scan_succeeds=*/true);
}

// Make sure DropData::html is handled correctly.
TEST_F(ChromeWebContentsViewDelegateHandleOnPerformDrop, Html) {
  content::DropData data;
  data.html = base::UTF8ToUTF16(large_text());

  SetExpectedRequestsCount(0);
  RunTest(data, /*enable=*/false, /*scan_succeeds=*/true);

  SetExpectedRequestsCount(1);
  RunTest(data, /*enable=*/true, /*scan_succeeds=*/false);
  RunTest(data, /*enable=*/true, /*scan_succeeds=*/true);

  data.html = base::UTF8ToUTF16(small_text());
  SetExpectedRequestsCount(0);
  RunTest(data, /*enable=*/true, /*scan_succeeds=*/true);
}

// Make sure DropData::filenames is handled correctly.
TEST_F(ChromeWebContentsViewDelegateHandleOnPerformDrop, Files) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath path_1 = temp_dir.GetPath().AppendASCII("Foo.doc");
  base::FilePath path_2 = temp_dir.GetPath().AppendASCII("Bar.doc");

  base::File file_1(path_1, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  base::File file_2(path_2, base::File::FLAG_CREATE | base::File::FLAG_WRITE);

  ASSERT_TRUE(file_1.IsValid());
  ASSERT_TRUE(file_2.IsValid());

  file_1.WriteAtCurrentPos("foo content", 11);
  file_2.WriteAtCurrentPos("bar content", 11);

  content::DropData data;
  data.filenames.emplace_back(path_1, path_1);
  data.filenames.emplace_back(path_2, path_2);

  SetExpectedRequestsCount(0);
  RunTest(data, /*enable=*/false, /*scan_succeeds=*/true);

  SetExpectedRequestsCount(2);
  RunTest(data, /*enable=*/true, /*scan_succeeds=*/false);
  RunTest(data, /*enable=*/true, /*scan_succeeds=*/true);
}

// Make sure DropData::filenames directories are handled correctly.
TEST_F(ChromeWebContentsViewDelegateHandleOnPerformDrop, Directories) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath path_1 = temp_dir.GetPath().AppendASCII("Foo.doc");
  base::FilePath path_2 = temp_dir.GetPath().AppendASCII("Bar.doc");
  base::FilePath path_3 = temp_dir.GetPath().AppendASCII("Baz.doc");

  base::File file_1(path_1, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  base::File file_2(path_2, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  base::File file_3(path_3, base::File::FLAG_CREATE | base::File::FLAG_WRITE);

  ASSERT_TRUE(file_1.IsValid());
  ASSERT_TRUE(file_2.IsValid());
  ASSERT_TRUE(file_3.IsValid());

  file_1.WriteAtCurrentPos("foo content", 11);
  file_2.WriteAtCurrentPos("bar content", 11);
  file_3.WriteAtCurrentPos("baz content", 11);

  content::DropData data;
  data.filenames.emplace_back(temp_dir.GetPath(), temp_dir.GetPath());

  SetExpectedRequestsCount(0);
  RunTest(data, /*enable=*/false, /*scan_succeeds=*/true);

  SetExpectedRequestsCount(3);
  RunTest(data, /*enable=*/true, /*scan_succeeds=*/false);
  RunTest(data, /*enable=*/true, /*scan_succeeds=*/true);
}
