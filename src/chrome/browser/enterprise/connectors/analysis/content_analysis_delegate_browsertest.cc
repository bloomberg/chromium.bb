// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <set>

#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_delegate.h"
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_dialog.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router_factory.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_browsertest_base.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_test_utils.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/cloud/realtime_reporting_job_configuration.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_test.h"

using extensions::SafeBrowsingPrivateEventRouter;
using safe_browsing::BinaryUploadService;
using ::testing::_;
using ::testing::Mock;

namespace enterprise_connectors {

namespace {

constexpr char kUserName[] = "test@chromium.org";

constexpr char kScanId1[] = "scan id 1";
constexpr char kScanId2[] = "scan id 2";

std::string text() {
  return std::string(100, 'a');
}

class FakeBinaryUploadService : public BinaryUploadService {
 public:
  FakeBinaryUploadService() : BinaryUploadService(nullptr, nullptr, nullptr) {}

  // Sets whether the user is authorized to upload data for Deep Scanning.
  void SetAuthorized(bool authorized) {
    authorization_result_ = authorized
                                ? BinaryUploadService::Result::SUCCESS
                                : BinaryUploadService::Result::UNAUTHORIZED;
  }

  // Finish the authentication request. Called after CreateForWebContents to
  // simulate an async callback.
  void ReturnAuthorizedResponse() {
    FinishRequest(authorization_request_.get(), authorization_result_,
                  ContentAnalysisResponse());
  }

  void SetResponseForText(BinaryUploadService::Result result,
                          const ContentAnalysisResponse& response) {
    prepared_text_result_ = result;
    prepared_text_response_ = response;
  }

  void SetResponseForFile(const std::string& path,
                          BinaryUploadService::Result result,
                          const ContentAnalysisResponse& response) {
    prepared_file_results_[path] = result;
    prepared_file_responses_[path] = response;
  }

  void SetShouldAutomaticallyAuthorize(bool authorize) {
    should_automatically_authorize_ = authorize;
  }

  int requests_count() const { return requests_count_; }

 private:
  void UploadForDeepScanning(std::unique_ptr<Request> request) override {
    ++requests_count_;

    // A request without tags indicates that it's used for authentication
    if (request->content_analysis_request().tags().empty()) {
      authorization_request_.swap(request);

      if (should_automatically_authorize_)
        ReturnAuthorizedResponse();
    } else {
      std::string file = request->filename();
      if (file.empty()) {
        request->FinishRequest(prepared_text_result_, prepared_text_response_);
      } else {
        ASSERT_TRUE(prepared_file_results_.count(file));
        ASSERT_TRUE(prepared_file_responses_.count(file));
        request->FinishRequest(prepared_file_results_[file],
                               prepared_file_responses_[file]);
      }
    }
  }

  BinaryUploadService::Result authorization_result_;
  std::unique_ptr<Request> authorization_request_;

  BinaryUploadService::Result prepared_text_result_;
  ContentAnalysisResponse prepared_text_response_;

  std::map<std::string, BinaryUploadService::Result> prepared_file_results_;
  std::map<std::string, ContentAnalysisResponse> prepared_file_responses_;

  int requests_count_ = 0;
  bool should_automatically_authorize_ = false;
};

FakeBinaryUploadService* FakeBinaryUploadServiceStorage() {
  static FakeBinaryUploadService service;
  return &service;
}

const std::set<std::string>* DocMimeTypes() {
  static std::set<std::string> set = {
      "application/msword", "text/plain",
      // The 50 MB file can result in no mimetype being found.
      ""};
  return &set;
}

const std::set<std::string>* ExeMimeTypes() {
  static std::set<std::string> set = {"application/x-msdownload",
                                      "application/x-ms-dos-executable",
                                      "application/octet-stream"};
  return &set;
}

const std::set<std::string>* ZipMimeTypes() {
  static std::set<std::string> set = {"application/zip",
                                      "application/x-zip-compressed"};
  return &set;
}

const std::set<std::string>* PngMimeTypes() {
  static std::set<std::string> set = {"image/png"};
  return &set;
}

const std::set<std::string>* TextMimeTypes() {
  static std::set<std::string> set = {"text/plain"};
  return &set;
}

// A fake delegate with minimal overrides to obtain behavior that's as close to
// the real one as possible.
class MinimalFakeContentAnalysisDelegate : public ContentAnalysisDelegate {
 public:
  MinimalFakeContentAnalysisDelegate(
      content::WebContents* web_contents,
      ContentAnalysisDelegate::Data data,
      ContentAnalysisDelegate::CompletionCallback callback)
      : ContentAnalysisDelegate(web_contents,
                                std::move(data),
                                std::move(callback),
                                safe_browsing::DeepScanAccessPoint::UPLOAD) {}

  static std::unique_ptr<ContentAnalysisDelegate> Create(
      content::WebContents* web_contents,
      ContentAnalysisDelegate::Data data,
      ContentAnalysisDelegate::CompletionCallback callback) {
    return std::make_unique<MinimalFakeContentAnalysisDelegate>(
        web_contents, std::move(data), std::move(callback));
  }

 private:
  BinaryUploadService* GetBinaryUploadService() override {
    return FakeBinaryUploadServiceStorage();
  }
};

constexpr char kBrowserDMToken[] = "browser_dm_token";
constexpr char kProfileDMToken[] = "profile_dm_token";

constexpr char kTestUrl[] = "https://google.com";

}  // namespace

// Tests the behavior of the dialog delegate with minimal overriding of methods.
// Only responses obtained via the BinaryUploadService are faked.
class ContentAnalysisDelegateBrowserTestBase
    : public safe_browsing::DeepScanningBrowserTestBase,
      public ContentAnalysisDialog::TestObserver {
 public:
  explicit ContentAnalysisDelegateBrowserTestBase(bool machine_scope)
      : machine_scope_(machine_scope) {
    ContentAnalysisDialog::SetObserverForTesting(this);
  }

  void EnableUploadsScanningAndReporting() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    SetDMTokenForTesting(
        policy::DMToken::CreateValidTokenForTesting(kBrowserDMToken));
#else
    if (machine_scope_) {
      SetDMTokenForTesting(
          policy::DMToken::CreateValidTokenForTesting(kBrowserDMToken));
    } else {
      safe_browsing::SetProfileDMToken(browser()->profile(), kProfileDMToken);
    }
#endif

    constexpr char kBlockingScansForDlpAndMalware[] = R"({
      "service_provider": "google",
      "enable": [
        {
          "url_list": ["*"],
          "tags": ["dlp", "malware"]
        }
      ],
      "block_until_verdict": 1
    })";
    safe_browsing::SetAnalysisConnector(
        browser()->profile()->GetPrefs(), FILE_ATTACHED,
        kBlockingScansForDlpAndMalware, machine_scope_);
    safe_browsing::SetAnalysisConnector(
        browser()->profile()->GetPrefs(), BULK_DATA_ENTRY,
        kBlockingScansForDlpAndMalware, machine_scope_);
    safe_browsing::SetOnSecurityEventReporting(browser()->profile()->GetPrefs(),
                                               /*enabled*/ true,
                                               /*enabled_event_names*/ {},
#if BUILDFLAG(IS_CHROMEOS_ASH)
                                               /*machine_scope*/ false);
#else
                                               machine_scope_);
#endif

    client_ = std::make_unique<policy::MockCloudPolicyClient>();
    client_->SetDMToken(
#if BUILDFLAG(IS_CHROMEOS_ASH)
        kBrowserDMToken);
#else
        machine_scope_ ? kBrowserDMToken : kProfileDMToken);
#endif
    if (machine_scope_) {
      extensions::SafeBrowsingPrivateEventRouterFactory::GetForProfile(
          browser()->profile())
          ->SetBrowserCloudPolicyClientForTesting(client_.get());
    } else {
      extensions::SafeBrowsingPrivateEventRouterFactory::GetForProfile(
          browser()->profile())
#if BUILDFLAG(IS_CHROMEOS_ASH)
          ->SetBrowserCloudPolicyClientForTesting(client_.get());
#else
          ->SetProfileCloudPolicyClientForTesting(client_.get());
#endif
    }
    identity_test_environment_ =
        std::make_unique<signin::IdentityTestEnvironment>();
    identity_test_environment_->MakePrimaryAccountAvailable(
        kUserName, signin::ConsentLevel::kSync);
    extensions::SafeBrowsingPrivateEventRouterFactory::GetForProfile(
        browser()->profile())
        ->SetIdentityManagerForTesting(
            identity_test_environment_->identity_manager());
  }

  void DestructorCalled(ContentAnalysisDialog* dialog) override {
    // The test is over once the views are destroyed.
    CallQuitClosure();
  }

  policy::MockCloudPolicyClient* client() { return client_.get(); }

 private:
  std::unique_ptr<policy::MockCloudPolicyClient> client_;
  std::unique_ptr<signin::IdentityTestEnvironment> identity_test_environment_;
  base::ScopedTempDir temp_dir_;
  bool machine_scope_;
};

class ContentAnalysisDelegateBrowserTest
    : public ContentAnalysisDelegateBrowserTestBase,
      public testing::WithParamInterface<bool> {
 public:
  ContentAnalysisDelegateBrowserTest()
      : ContentAnalysisDelegateBrowserTestBase(GetParam()) {}
};

INSTANTIATE_TEST_CASE_P(, ContentAnalysisDelegateBrowserTest, testing::Bool());

IN_PROC_BROWSER_TEST_P(ContentAnalysisDelegateBrowserTest, Unauthorized) {
  // The reading of the browser DM token is blocking and happens in this test
  // when checking if the browser is enrolled.
  base::ScopedAllowBlockingForTesting allow_blocking;

  EnableUploadsScanningAndReporting();

  ContentAnalysisDelegate::SetFactoryForTesting(
      base::BindRepeating(&MinimalFakeContentAnalysisDelegate::Create));

  FakeBinaryUploadServiceStorage()->SetAuthorized(false);
  // This causes the DM Token to be rejected, and unauthorized for 24 hours.
  client()->SetStatus(policy::DM_STATUS_SERVICE_MANAGEMENT_NOT_SUPPORTED);
  client()->NotifyClientError();

  bool called = false;
  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();

  ContentAnalysisDelegate::Data data;
  data.text.emplace_back(text());
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.doc"));
  ASSERT_TRUE(ContentAnalysisDelegate::IsEnabled(
      browser()->profile(), GURL(kTestUrl), &data, FILE_ATTACHED));

  // Nothing should be reported for unauthorized users.
  safe_browsing::EventReportValidator validator(client());
  validator.ExpectNoReport();

  ContentAnalysisDelegate::CreateForWebContents(
      browser()->tab_strip_model()->GetActiveWebContents(), std::move(data),
      base::BindLambdaForTesting(
          [&quit_closure, &called](
              const ContentAnalysisDelegate::Data& data,
              const ContentAnalysisDelegate::Result& result) {
            ASSERT_EQ(result.text_results.size(), 1u);
            ASSERT_EQ(result.paths_results.size(), 1u);
            ASSERT_TRUE(result.text_results[0]);
            ASSERT_TRUE(result.paths_results[0]);
            called = true;
            quit_closure.Run();
          }),
      safe_browsing::DeepScanAccessPoint::UPLOAD);

  FakeBinaryUploadServiceStorage()->ReturnAuthorizedResponse();

  run_loop.Run();
  EXPECT_TRUE(called);

  // 1 request to authenticate for upload.
  ASSERT_EQ(FakeBinaryUploadServiceStorage()->requests_count(), 1);
}

IN_PROC_BROWSER_TEST_P(ContentAnalysisDelegateBrowserTest, Files) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  // Set up delegate and upload service.
  EnableUploadsScanningAndReporting();

  ContentAnalysisDelegate::SetFactoryForTesting(
      base::BindRepeating(&MinimalFakeContentAnalysisDelegate::Create));

  FakeBinaryUploadServiceStorage()->SetAuthorized(true);
  FakeBinaryUploadServiceStorage()->SetShouldAutomaticallyAuthorize(true);

  // Create the files to be opened and scanned.
  ContentAnalysisDelegate::Data data;
  CreateFilesForTest({"ok.doc", "bad.exe"},
                     {"ok file content", "bad file content"}, &data);
  ASSERT_TRUE(ContentAnalysisDelegate::IsEnabled(
      browser()->profile(), GURL(kTestUrl), &data, FILE_ATTACHED));

  // The malware verdict means an event should be reported.
  safe_browsing::EventReportValidator validator(client());
  validator.ExpectDangerousDeepScanningResult(
      /*url*/ "about:blank",
      /*filename*/ created_file_paths()[1].AsUTF8Unsafe(),
      // printf "bad file content" | sha256sum |  tr '[:lower:]' '[:upper:]'
      /*sha*/
      "77AE96C38386429D28E53F5005C46C7B4D8D39BE73D757CE61E0AE65CC1A5A5D",
      /*threat_type*/ "DANGEROUS",
      /*trigger*/ SafeBrowsingPrivateEventRouter::kTriggerFileUpload,
      /*mimetypes*/ ExeMimeTypes(),
      /*size*/ std::string("bad file content").size(),
      /*result*/
      safe_browsing::EventResultToString(safe_browsing::EventResult::BLOCKED),
      /*username*/ kUserName, /*scan_id*/ kScanId2);

  ContentAnalysisResponse ok_response;
  ok_response.set_request_token(kScanId1);
  auto* ok_result = ok_response.add_results();
  ok_result->set_status(ContentAnalysisResponse::Result::SUCCESS);
  ok_result->set_tag("malware");

  ContentAnalysisResponse bad_response;
  bad_response.set_request_token(kScanId2);
  auto* bad_result = bad_response.add_results();
  bad_result->set_status(ContentAnalysisResponse::Result::SUCCESS);
  bad_result->set_tag("malware");
  auto* bad_rule = bad_result->add_triggered_rules();
  bad_rule->set_action(TriggeredRule::BLOCK);
  bad_rule->set_rule_name("malware");

  FakeBinaryUploadServiceStorage()->SetResponseForFile(
      created_file_paths()[0].AsUTF8Unsafe(),
      BinaryUploadService::Result::SUCCESS, ok_response);
  FakeBinaryUploadServiceStorage()->SetResponseForFile(
      created_file_paths()[1].AsUTF8Unsafe(),
      BinaryUploadService::Result::SUCCESS, bad_response);

  bool called = false;
  base::RunLoop run_loop;
  SetQuitClosure(run_loop.QuitClosure());

  // Start test.
  ContentAnalysisDelegate::CreateForWebContents(
      browser()->tab_strip_model()->GetActiveWebContents(), std::move(data),
      base::BindLambdaForTesting(
          [&called](const ContentAnalysisDelegate::Data& data,
                    const ContentAnalysisDelegate::Result& result) {
            ASSERT_TRUE(result.text_results.empty());
            ASSERT_EQ(result.paths_results.size(), 2u);
            ASSERT_TRUE(result.paths_results[0]);
            ASSERT_FALSE(result.paths_results[1]);
            called = true;
          }),
      safe_browsing::DeepScanAccessPoint::UPLOAD);

  run_loop.Run();

  EXPECT_TRUE(called);

  // There should have been 1 request per file (2 files) and 1 for
  // authentication.
  ASSERT_EQ(FakeBinaryUploadServiceStorage()->requests_count(), 3);
}

IN_PROC_BROWSER_TEST_P(ContentAnalysisDelegateBrowserTest, Texts) {
  // The reading of the browser DM token is blocking and happens in this test
  // when checking if the browser is enrolled.
  base::ScopedAllowBlockingForTesting allow_blocking;

  // Set up delegate and upload service.
  EnableUploadsScanningAndReporting();

  ContentAnalysisDelegate::SetFactoryForTesting(
      base::BindRepeating(&MinimalFakeContentAnalysisDelegate::Create));

  FakeBinaryUploadServiceStorage()->SetAuthorized(true);

  safe_browsing::EventReportValidator validator(client());
  // Prepare a complex DLP response to test that the verdict is reported
  // correctly in the sensitive data event.
  ContentAnalysisResponse response;
  response.set_request_token(kScanId1);
  auto* result = response.add_results();
  result->set_status(ContentAnalysisResponse::Result::SUCCESS);
  result->set_tag("dlp");

  auto* rule1 = result->add_triggered_rules();
  rule1->set_action(TriggeredRule::REPORT_ONLY);
  rule1->set_rule_id("1");
  rule1->set_rule_name("resource rule 1");

  auto* rule2 = result->add_triggered_rules();
  rule2->set_action(TriggeredRule::BLOCK);
  rule2->set_rule_id("3");
  rule2->set_rule_name("resource rule 2");

  FakeBinaryUploadServiceStorage()->SetResponseForText(
      BinaryUploadService::Result::SUCCESS, response);

  // The DLP verdict means an event should be reported. The content size is
  // equal to the length of the concatenated texts (2 * 100 * 'a').
  validator.ExpectSensitiveDataEvent(
      /*url*/ "about:blank",
      /*filename*/ "Text data",
      // The hash should not be included for string requests.
      /*sha*/ "",
      /*trigger*/ SafeBrowsingPrivateEventRouter::kTriggerWebContentUpload,
      /*dlp_verdict*/ *result,
      /*mimetype*/ TextMimeTypes(),
      /*size*/ 200,
      /*result*/
      safe_browsing::EventResultToString(safe_browsing::EventResult::BLOCKED),
      /*username*/ kUserName,
      /*scan_id*/ kScanId1);

  bool called = false;
  base::RunLoop run_loop;
  SetQuitClosure(run_loop.QuitClosure());

  ContentAnalysisDelegate::Data data;
  data.text.emplace_back(text());
  data.text.emplace_back(text());
  ASSERT_TRUE(ContentAnalysisDelegate::IsEnabled(
      browser()->profile(), GURL(kTestUrl), &data, BULK_DATA_ENTRY));

  // Start test.
  ContentAnalysisDelegate::CreateForWebContents(
      browser()->tab_strip_model()->GetActiveWebContents(), std::move(data),
      base::BindLambdaForTesting(
          [&called](const ContentAnalysisDelegate::Data& data,
                    const ContentAnalysisDelegate::Result& result) {
            ASSERT_TRUE(result.paths_results.empty());
            ASSERT_EQ(result.text_results.size(), 2u);
            ASSERT_FALSE(result.text_results[0]);
            ASSERT_FALSE(result.text_results[1]);
            called = true;
          }),
      safe_browsing::DeepScanAccessPoint::PASTE);

  FakeBinaryUploadServiceStorage()->ReturnAuthorizedResponse();

  run_loop.Run();
  EXPECT_TRUE(called);

  // There should have been 1 request for all texts,
  // 1 for authentication of the scanning request.
  ASSERT_EQ(FakeBinaryUploadServiceStorage()->requests_count(), 2);
}

IN_PROC_BROWSER_TEST_P(ContentAnalysisDelegateBrowserTest, Throttled) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  // Set up delegate and upload service.
  EnableUploadsScanningAndReporting();

  ContentAnalysisDelegate::SetFactoryForTesting(
      base::BindRepeating(&MinimalFakeContentAnalysisDelegate::Create));

  FakeBinaryUploadServiceStorage()->SetAuthorized(true);
  FakeBinaryUploadServiceStorage()->SetShouldAutomaticallyAuthorize(true);

  // Create the files to be opened and scanned.
  ContentAnalysisDelegate::Data data;
  CreateFilesForTest({"a.exe", "b.exe", "c.exe"},
                     {"a content", "b content", "c content"}, &data);
  ASSERT_TRUE(ContentAnalysisDelegate::IsEnabled(
      browser()->profile(), GURL(kTestUrl), &data, FILE_ATTACHED));

  // The malware verdict means an event should be reported.
  safe_browsing::EventReportValidator validator(client());
  validator.ExpectUnscannedFileEvents(
      /*url*/ "about:blank",
      {
          created_file_paths()[0].AsUTF8Unsafe(),
          created_file_paths()[1].AsUTF8Unsafe(),
          created_file_paths()[2].AsUTF8Unsafe(),
      },
      {
          // printf "a content" | sha256sum | tr '[:lower:]' '[:upper:]'
          "D2D2ACF640179223BF9E1EB43C5FBF854C4E50FFB6733BC3A9279D3FF7DE9BE1",
          // printf "b content" | sha256sum | tr '[:lower:]' '[:upper:]'
          "93CB3641ADD6A9A6619D7E2F304EBCF5160B2DB016B27C6E3D641C5306897224",
          // printf "c content" | sha256sum | tr '[:lower:]' '[:upper:]'
          "2E6D1C4A1F39A02562BF1505AD775C0323D7A04C0C37C9B29D25F532B9972080",
      },
      /*trigger*/ SafeBrowsingPrivateEventRouter::kTriggerFileUpload,
      /*reason*/ "TOO_MANY_REQUESTS",
      /*mimetypes*/ ExeMimeTypes(),
      /*size*/ 9,
      /*result*/
      safe_browsing::EventResultToString(safe_browsing::EventResult::ALLOWED),
      /*username*/ kUserName);

  // While only one file should reach the upload part and get a
  // TOO_MANY_REQUEST result, it can be any of them depending on how quickly
  // they are opened asynchronously. This means responses must be set up for
  // each of them.
  for (size_t i = 0; i < 3; ++i) {
    FakeBinaryUploadServiceStorage()->SetResponseForFile(
        created_file_paths()[i].AsUTF8Unsafe(),
        BinaryUploadService::Result::TOO_MANY_REQUESTS,
        ContentAnalysisResponse());
  }

  bool called = false;
  base::RunLoop run_loop;
  SetQuitClosure(run_loop.QuitClosure());

  // Start test.
  ContentAnalysisDelegate::CreateForWebContents(
      browser()->tab_strip_model()->GetActiveWebContents(), std::move(data),
      base::BindLambdaForTesting(
          [&called](const ContentAnalysisDelegate::Data& data,
                    const ContentAnalysisDelegate::Result& result) {
            ASSERT_TRUE(result.text_results.empty());
            ASSERT_EQ(result.paths_results.size(), 3u);
            for (bool paths_result : result.paths_results)
              ASSERT_TRUE(paths_result);
            called = true;
          }),
      safe_browsing::DeepScanAccessPoint::UPLOAD);

  run_loop.Run();

  EXPECT_TRUE(called);

  // There should have been 1 request for the first file and 1 for
  // authentication.
  ASSERT_EQ(FakeBinaryUploadServiceStorage()->requests_count(), 2);
}

// This class tests each of the blocking settings used in Connector policies:
// - block_until_verdict
// - block_password_protected
// - block_large_files
// - block_unsupported_file_types
class ContentAnalysisDelegateBlockingSettingBrowserTest
    : public ContentAnalysisDelegateBrowserTestBase,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  ContentAnalysisDelegateBlockingSettingBrowserTest()
      : ContentAnalysisDelegateBrowserTestBase(machine_scope()) {}

  bool machine_scope() const { return std::get<0>(GetParam()); }

  bool setting_param() const { return std::get<1>(GetParam()); }

  // Use a string since the setting value is inserted into a JSON policy.
  const char* bool_setting_value() const {
    return setting_param() ? "true" : "false";
  }
  const char* int_setting_value() const { return setting_param() ? "1" : "0"; }

  bool expected_result() const { return !setting_param(); }
};

INSTANTIATE_TEST_SUITE_P(,
                         ContentAnalysisDelegateBlockingSettingBrowserTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

IN_PROC_BROWSER_TEST_P(ContentAnalysisDelegateBlockingSettingBrowserTest,
                       BlockPasswordProtected) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  base::FilePath test_zip;
  EXPECT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_zip));
  test_zip = test_zip.AppendASCII("safe_browsing")
                 .AppendASCII("download_protection")
                 .AppendASCII("encrypted.zip");

  // Set up delegate and upload service.
  EnableUploadsScanningAndReporting();
  constexpr char kPasswordProtectedPref[] = R"({
    "service_provider": "google",
    "enable": [
      {
        "url_list": ["*"],
        "tags": ["dlp"]
      }
    ],
    "block_until_verdict": 1,
    "block_password_protected": %s
  })";
  safe_browsing::SetAnalysisConnector(
      browser()->profile()->GetPrefs(), FILE_ATTACHED,
      base::StringPrintf(kPasswordProtectedPref, bool_setting_value()),
      machine_scope());

  ContentAnalysisDelegate::SetFactoryForTesting(
      base::BindRepeating(&MinimalFakeContentAnalysisDelegate::Create));

  FakeBinaryUploadServiceStorage()->SetAuthorized(true);
  FakeBinaryUploadServiceStorage()->SetShouldAutomaticallyAuthorize(true);

  bool called = false;
  base::RunLoop run_loop;
  SetQuitClosure(run_loop.QuitClosure());

  ContentAnalysisDelegate::Data data;
  data.paths.emplace_back(test_zip);
  ASSERT_TRUE(ContentAnalysisDelegate::IsEnabled(
      browser()->profile(), GURL(kTestUrl), &data, FILE_ATTACHED));

  // The file should be reported as unscanned.
  safe_browsing::EventReportValidator validator(client());
  validator.ExpectUnscannedFileEvent(
      /*url*/ "about:blank",
      /*filename*/ test_zip.AsUTF8Unsafe(),
      // sha256sum < chrome/test/data/safe_browsing/download_protection/\
      // encrypted.zip |  tr '[:lower:]' '[:upper:]'
      /*sha*/
      "701FCEA8B2112FFAB257A8A8DFD3382ABCF047689AB028D42903E3B3AA488D9A",
      /*trigger*/ SafeBrowsingPrivateEventRouter::kTriggerFileUpload,
      /*reason*/ "FILE_PASSWORD_PROTECTED",
      /*mimetypes*/ ZipMimeTypes(),
      // du chrome/test/data/safe_browsing/download_protection/encrypted.zip -b
      /*size*/ 20015,
      /*result*/
      expected_result() ? safe_browsing::EventResultToString(
                              safe_browsing::EventResult::ALLOWED)
                        : safe_browsing::EventResultToString(
                              safe_browsing::EventResult::BLOCKED),
      /*username*/ kUserName);

  // Start test.
  ContentAnalysisDelegate::CreateForWebContents(
      browser()->tab_strip_model()->GetActiveWebContents(), std::move(data),
      base::BindLambdaForTesting(
          [this, &called](const ContentAnalysisDelegate::Data& data,
                          const ContentAnalysisDelegate::Result& result) {
            ASSERT_TRUE(result.text_results.empty());
            ASSERT_EQ(result.paths_results.size(), 1u);
            ASSERT_EQ(result.paths_results[0], expected_result());
            called = true;
          }),
      safe_browsing::DeepScanAccessPoint::UPLOAD);

  run_loop.Run();
  EXPECT_TRUE(called);
  ASSERT_EQ(FakeBinaryUploadServiceStorage()->requests_count(), 0);
}

IN_PROC_BROWSER_TEST_P(ContentAnalysisDelegateBlockingSettingBrowserTest,
                       BlockUnsupportedFileTypes) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  // Set up delegate and upload service.
  EnableUploadsScanningAndReporting();
  constexpr char kBlockUnsupportedFileTypesPref[] = R"({
    "service_provider": "google",
    "enable": [
      {
        "url_list": ["*"],
        "tags": ["dlp"]
      }
    ],
    "block_until_verdict": 1,
    "block_unsupported_file_types": %s
  })";
  safe_browsing::SetAnalysisConnector(
      browser()->profile()->GetPrefs(), FILE_ATTACHED,
      base::StringPrintf(kBlockUnsupportedFileTypesPref, bool_setting_value()),
      machine_scope());

  ContentAnalysisDelegate::SetFactoryForTesting(
      base::BindRepeating(&MinimalFakeContentAnalysisDelegate::Create));

  FakeBinaryUploadServiceStorage()->SetAuthorized(true);
  FakeBinaryUploadServiceStorage()->SetShouldAutomaticallyAuthorize(true);

  // Create the files with unsupported types.
  std::string png_file_content = "\x89PNG\x0D\x0A\x1A\x0A";
  ContentAnalysisDelegate::Data data;
  CreateFilesForTest({"a.png"}, {png_file_content}, &data);
  ASSERT_TRUE(ContentAnalysisDelegate::IsEnabled(
      browser()->profile(), GURL(kTestUrl), &data, FILE_ATTACHED));

  // The file should be reported as unscanned.
  safe_browsing::EventReportValidator validator(client());
  validator.ExpectUnscannedFileEvent(
      /*url*/ "about:blank",
      /*filename*/ created_file_paths()[0].AsUTF8Unsafe(),
      // printf "\x89PNG\x0D\x0A\x1A\x0A" | sha256sum |  tr '[:lower:]' \
      // '[:upper:]'
      "4C4B6A3BE1314AB86138BEF4314DDE022E600960D8689A2C8F8631802D20DAB6",
      /*trigger*/ SafeBrowsingPrivateEventRouter::kTriggerFileUpload,
      /*reason*/ "DLP_SCAN_UNSUPPORTED_FILE_TYPE",
      /*mimetype*/ PngMimeTypes(),
      /*size*/ png_file_content.size(),
      /*result*/
      expected_result() ? safe_browsing::EventResultToString(
                              safe_browsing::EventResult::ALLOWED)
                        : safe_browsing::EventResultToString(
                              safe_browsing::EventResult::BLOCKED),
      /*username*/ kUserName);

  bool called = false;
  base::RunLoop run_loop;
  SetQuitClosure(run_loop.QuitClosure());

  // Start test.
  ContentAnalysisDelegate::CreateForWebContents(
      browser()->tab_strip_model()->GetActiveWebContents(), std::move(data),
      base::BindLambdaForTesting(
          [this, &called](const ContentAnalysisDelegate::Data& data,
                          const ContentAnalysisDelegate::Result& result) {
            ASSERT_TRUE(result.text_results.empty());
            ASSERT_EQ(result.paths_results.size(), 1u);
            ASSERT_EQ(result.paths_results[0], expected_result());

            called = true;
          }),
      safe_browsing::DeepScanAccessPoint::UPLOAD);

  run_loop.Run();
  EXPECT_TRUE(called);
}

IN_PROC_BROWSER_TEST_P(ContentAnalysisDelegateBlockingSettingBrowserTest,
                       BlockLargeFiles) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  // Set up delegate and upload service.
  EnableUploadsScanningAndReporting();
  constexpr char kBlockLargeFilesPref[] = R"({
    "service_provider": "google",
    "enable": [
      {
        "url_list": ["*"],
        "tags": ["dlp", "malware"]
      }
    ],
    "block_until_verdict": 1,
    "block_large_files": %s
  })";
  safe_browsing::SetAnalysisConnector(
      browser()->profile()->GetPrefs(), FILE_ATTACHED,
      base::StringPrintf(kBlockLargeFilesPref, bool_setting_value()),
      machine_scope());

  ContentAnalysisDelegate::SetFactoryForTesting(
      base::BindRepeating(&MinimalFakeContentAnalysisDelegate::Create));

  FakeBinaryUploadServiceStorage()->SetAuthorized(true);
  FakeBinaryUploadServiceStorage()->SetShouldAutomaticallyAuthorize(true);

  // Create the large file.
  ContentAnalysisDelegate::Data data;

  CreateFilesForTest(
      {"large.doc"},
      {std::string(BinaryUploadService::kMaxUploadSizeBytes + 1, 'a')}, &data);
  ASSERT_TRUE(ContentAnalysisDelegate::IsEnabled(
      browser()->profile(), GURL(kTestUrl), &data, FILE_ATTACHED));

  // The file should be reported as unscanned.
  safe_browsing::EventReportValidator validator(client());
  validator.ExpectUnscannedFileEvent(
      /*url*/ "about:blank",
      /*filename*/ created_file_paths()[0].AsUTF8Unsafe(),
      // python3 -c "print('a' * (50 * 1024 * 1024 + 1), end='')" | sha256sum |\
      // tr '[:lower:]' '[:upper:]'
      /*sha*/
      "9EB56DB30C49E131459FE735BA6B9D38327376224EC8D5A1233F43A5B4A25942",
      /*trigger*/ SafeBrowsingPrivateEventRouter::kTriggerFileUpload,
      /*reason*/ "FILE_TOO_LARGE",
      /*mimetypes*/ DocMimeTypes(),
      /*size*/ BinaryUploadService::kMaxUploadSizeBytes + 1,
      /*result*/
      expected_result() ? safe_browsing::EventResultToString(
                              safe_browsing::EventResult::ALLOWED)
                        : safe_browsing::EventResultToString(
                              safe_browsing::EventResult::BLOCKED),
      /*username*/ kUserName);

  bool called = false;
  base::RunLoop run_loop;
  SetQuitClosure(run_loop.QuitClosure());

  // Start test.
  ContentAnalysisDelegate::CreateForWebContents(
      browser()->tab_strip_model()->GetActiveWebContents(), std::move(data),
      base::BindLambdaForTesting(
          [this, &called](const ContentAnalysisDelegate::Data& data,
                          const ContentAnalysisDelegate::Result& result) {
            ASSERT_TRUE(result.text_results.empty());
            ASSERT_EQ(result.paths_results.size(), 1u);
            ASSERT_EQ(result.paths_results[0], expected_result());

            called = true;
          }),
      safe_browsing::DeepScanAccessPoint::UPLOAD);

  run_loop.Run();
  EXPECT_TRUE(called);
}

IN_PROC_BROWSER_TEST_P(ContentAnalysisDelegateBlockingSettingBrowserTest,
                       BlockUntilVerdict) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  // Set up delegate and upload service.
  EnableUploadsScanningAndReporting();
  constexpr char kBlockUntilVerdictPref[] = R"({
    "service_provider": "google",
    "enable": [
      {
        "url_list": ["*"],
        "tags": ["dlp", "malware"]
      }
    ],
    "block_until_verdict": %s
  })";
  safe_browsing::SetAnalysisConnector(
      browser()->profile()->GetPrefs(), FILE_ATTACHED,
      base::StringPrintf(kBlockUntilVerdictPref, int_setting_value()),
      machine_scope());

  ContentAnalysisDelegate::SetFactoryForTesting(
      base::BindRepeating(&MinimalFakeContentAnalysisDelegate::Create));

  FakeBinaryUploadServiceStorage()->SetAuthorized(true);
  FakeBinaryUploadServiceStorage()->SetShouldAutomaticallyAuthorize(true);

  // Create a file.
  ContentAnalysisDelegate::Data data;

  CreateFilesForTest({"foo.doc"}, {"foo content"}, &data);
  ASSERT_TRUE(ContentAnalysisDelegate::IsEnabled(
      browser()->profile(), GURL(kTestUrl), &data, FILE_ATTACHED));

  // The file should be reported as malware and sensitive content.
  safe_browsing::EventReportValidator validator(client());
  ContentAnalysisResponse response;
  response.set_request_token(kScanId1);

  auto* malware_result = response.add_results();
  malware_result->set_status(ContentAnalysisResponse::Result::SUCCESS);
  malware_result->set_tag("malware");
  auto* malware_rule = malware_result->add_triggered_rules();
  malware_rule->set_action(TriggeredRule::BLOCK);
  malware_rule->set_rule_name("malware");

  auto* dlp_result = response.add_results();
  dlp_result->set_status(ContentAnalysisResponse::Result::SUCCESS);
  dlp_result->set_tag("dlp");
  auto* dlp_rule = dlp_result->add_triggered_rules();
  dlp_rule->set_action(TriggeredRule::BLOCK);
  dlp_rule->set_rule_id("0");
  dlp_rule->set_rule_name("some_dlp_rule");

  FakeBinaryUploadServiceStorage()->SetResponseForFile(
      created_file_paths()[0].AsUTF8Unsafe(),
      BinaryUploadService::Result::SUCCESS, response);
  validator.ExpectDangerousDeepScanningResultAndSensitiveDataEvent(
      /*url*/ "about:blank",
      /*filename*/ created_file_paths()[0].AsUTF8Unsafe(),
      // printf "foo content" | sha256sum  |  tr '[:lower:]' '[:upper:]'
      /*sha*/
      "B3A2E2EDBAA3C798B4FC267792B1641B94793DE02D870124E5CBE663750B4CFC",
      /*threat_type*/ "DANGEROUS",
      /*trigger*/
      extensions::SafeBrowsingPrivateEventRouter::kTriggerFileUpload,
      /*dlp_verdict*/ *dlp_result,
      /*mimetypes*/ DocMimeTypes(),
      /*size*/ std::string("foo content").size(),
      // If the policy allows immediate delivery of the file, then the result is
      // ALLOWED even if the verdict obtained afterwards is BLOCKED.
      /*result*/
      safe_browsing::EventResultToString(
          expected_result() ? safe_browsing::EventResult::ALLOWED
                            : safe_browsing::EventResult::BLOCKED),
      /*username*/ kUserName,
      /*scan_id*/ kScanId1);

  bool called = false;
  base::RunLoop run_loop;

  // If the delivery is not delayed, put the quit closure right after the events
  // are reported instead of when the dialog closes.
  if (expected_result())
    validator.SetDoneClosure(run_loop.QuitClosure());
  else
    SetQuitClosure(run_loop.QuitClosure());

  // Start test.
  ContentAnalysisDelegate::CreateForWebContents(
      browser()->tab_strip_model()->GetActiveWebContents(), std::move(data),
      base::BindLambdaForTesting(
          [this, &called](const ContentAnalysisDelegate::Data& data,
                          const ContentAnalysisDelegate::Result& result) {
            ASSERT_TRUE(result.text_results.empty());
            ASSERT_EQ(result.paths_results.size(), 1u);
            ASSERT_EQ(result.paths_results[0], expected_result());

            called = true;
          }),
      safe_browsing::DeepScanAccessPoint::UPLOAD);

  run_loop.Run();
  EXPECT_TRUE(called);

  // Expect 1 request for initial authentication (unspecified type, to be
  // removed for crbug.com/1090088, then count should be 1), + 1 to scan the
  // file in all cases.
  ASSERT_EQ(FakeBinaryUploadServiceStorage()->requests_count(), 2);
}

// This class tests that ContentAnalysisDelegate is handled correctly when the
// requests are already unauthorized. The test parameter represents if the scan
// is set to be blocking through policy.
class ContentAnalysisDelegateUnauthorizedBrowserTest
    : public ContentAnalysisDelegateBrowserTestBase,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  ContentAnalysisDelegateUnauthorizedBrowserTest()
      : ContentAnalysisDelegateBrowserTestBase(machine_scope()) {}

  bool machine_scope() const { return std::get<0>(GetParam()); }
  bool blocking_scan() const { return std::get<1>(GetParam()); }

  const char* dm_token() const {
    return machine_scope() ? kBrowserDMToken : kProfileDMToken;
  }

  void SetUpScanning(bool file_scan) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    SetDMTokenForTesting(
        policy::DMToken::CreateValidTokenForTesting(dm_token()));
#else
    if (machine_scope()) {
      SetDMTokenForTesting(
          policy::DMToken::CreateValidTokenForTesting(dm_token()));
    } else {
      safe_browsing::SetProfileDMToken(browser()->profile(), dm_token());
    }
#endif

    std::string pref = base::StringPrintf(
        R"({
          "service_provider": "google",
          "enable": [
            {
              "url_list": ["*"],
              "tags": ["dlp", "malware"]
            }
          ],
          "block_until_verdict": %d
        })",
        blocking_scan() ? 1 : 0);

    safe_browsing::SetAnalysisConnector(
        browser()->profile()->GetPrefs(),
        file_scan ? FILE_ATTACHED : BULK_DATA_ENTRY, pref, machine_scope());
    file_scan_ = file_scan;
  }

  // The dialog should only appear on file scans since they need to be opened
  // asynchronously. This means that the dialog appears when scanning files and
  // that all the following overrides should be called. Text scans don't have an
  // asynchronous operation needed before being sent for scanning, so in the
  // unauthorized case the dialog doesn't need to appear and the assertions will
  // fail the test if they are reached.
  void ConstructorCalled(ContentAnalysisDialog* dialog,
                         base::TimeTicks timestamp) override {
    ASSERT_TRUE(file_scan_ && blocking_scan());
  }

  void ViewsFirstShown(ContentAnalysisDialog* dialog,
                       base::TimeTicks timestamp) override {
    ASSERT_TRUE(file_scan_ && blocking_scan());
  }

  void DialogUpdated(ContentAnalysisDialog* dialog,
                     ContentAnalysisDelegateBase::FinalResult result) override {
    ASSERT_TRUE(file_scan_ && blocking_scan());
  }

  void DestructorCalled(ContentAnalysisDialog* dialog) override {
    ASSERT_TRUE(file_scan_ && blocking_scan());
    CallQuitClosure();
  }

 protected:
  bool file_scan_ = false;
};

INSTANTIATE_TEST_SUITE_P(,
                         ContentAnalysisDelegateUnauthorizedBrowserTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

IN_PROC_BROWSER_TEST_P(ContentAnalysisDelegateUnauthorizedBrowserTest, Paste) {
  // The reading of the browser DM token is blocking and happens in this test
  // when checking if the browser is enrolled.
  base::ScopedAllowBlockingForTesting allow_blocking;

  SetUpScanning(/*file_scan*/ false);

  ContentAnalysisDelegate::SetFactoryForTesting(
      base::BindRepeating(&MinimalFakeContentAnalysisDelegate::Create));

  FakeBinaryUploadServiceStorage()->SetAuthForTesting(dm_token(), false);

  bool called = false;
  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();

  ContentAnalysisDelegate::Data data;
  data.text.emplace_back(text());
  ASSERT_TRUE(ContentAnalysisDelegate::IsEnabled(
      browser()->profile(), GURL(kTestUrl), &data, BULK_DATA_ENTRY));

  ContentAnalysisDelegate::CreateForWebContents(
      browser()->tab_strip_model()->GetActiveWebContents(), std::move(data),
      base::BindLambdaForTesting(
          [&quit_closure, &called](
              const ContentAnalysisDelegate::Data& data,
              const ContentAnalysisDelegate::Result& result) {
            ASSERT_EQ(result.text_results.size(), 1u);
            ASSERT_EQ(result.paths_results.size(), 0u);
            ASSERT_TRUE(result.text_results[0]);
            called = true;
            quit_closure.Run();
          }),
      safe_browsing::DeepScanAccessPoint::PASTE);

  run_loop.Run();
  EXPECT_TRUE(called);

  // No requests should be made since the DM token is unauthorized.
  ASSERT_EQ(FakeBinaryUploadServiceStorage()->requests_count(), 0);
}

IN_PROC_BROWSER_TEST_P(ContentAnalysisDelegateUnauthorizedBrowserTest, Files) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  SetUpScanning(/*file_scan*/ true);

  ContentAnalysisDelegate::SetFactoryForTesting(
      base::BindRepeating(&MinimalFakeContentAnalysisDelegate::Create));

  FakeBinaryUploadServiceStorage()->SetAuthForTesting(dm_token(), false);

  bool called = false;
  base::RunLoop run_loop;
  absl::optional<base::RepeatingClosure> quit_closure = absl::nullopt;

  // If the scan is blocking, we can call the quit closure when the dialog
  // closes. If it's not, call it at the end of the result callback.
  if (blocking_scan())
    SetQuitClosure(run_loop.QuitClosure());
  else
    quit_closure = run_loop.QuitClosure();

  ContentAnalysisDelegate::Data data;
  CreateFilesForTest({"file1.doc", "file2.doc"}, {"content1", "content2"},
                     &data);
  ASSERT_TRUE(ContentAnalysisDelegate::IsEnabled(
      browser()->profile(), GURL(kTestUrl), &data, FILE_ATTACHED));

  ContentAnalysisDelegate::CreateForWebContents(
      browser()->tab_strip_model()->GetActiveWebContents(), std::move(data),
      base::BindLambdaForTesting(
          [&quit_closure, &called](
              const ContentAnalysisDelegate::Data& data,
              const ContentAnalysisDelegate::Result& result) {
            ASSERT_EQ(result.text_results.size(), 0u);
            ASSERT_EQ(result.paths_results.size(), 2u);
            ASSERT_TRUE(result.paths_results[0]);
            ASSERT_TRUE(result.paths_results[1]);
            called = true;
            if (quit_closure.has_value())
              quit_closure.value().Run();
          }),
      safe_browsing::DeepScanAccessPoint::UPLOAD);

  run_loop.Run();
  EXPECT_TRUE(called);

  // No requests should be made since the DM token is unauthorized.
  ASSERT_EQ(FakeBinaryUploadServiceStorage()->requests_count(), 0);
}

}  // namespace enterprise_connectors
