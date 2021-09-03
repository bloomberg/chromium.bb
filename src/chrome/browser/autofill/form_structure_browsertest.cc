// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/data_driven_test.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/pattern_provider/pattern_configuration_parser.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/unique_ids.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "url/gurl.h"

#if defined(OS_MAC)
#include "base/mac/foundation_util.h"
#endif

namespace autofill {
namespace {

using net::test_server::BasicHttpResponse;
using net::test_server::HttpRequest;
using net::test_server::HttpResponse;

const base::FilePath::CharType kTestName[] = FILE_PATH_LITERAL("heuristics");

const std::set<base::FilePath::StringType>& GetFailingTestNames() {
  static auto* failing_test_names = new std::set<base::FilePath::StringType>{};
  return *failing_test_names;
}

const base::FilePath& GetTestDataDir() {
  static base::NoDestructor<base::FilePath> dir([]() {
    base::FilePath dir;
    base::PathService::Get(base::DIR_SOURCE_ROOT, &dir);
    dir = dir.AppendASCII("components").AppendASCII("test").AppendASCII("data");
    return dir;
  }());
  return *dir;
}

const base::FilePath GetInputDir() {
  static base::FilePath input_dir = GetTestDataDir()
                                        .AppendASCII("autofill")
                                        .Append(kTestName)
                                        .AppendASCII("input");
  return input_dir;
}

std::vector<base::FilePath> GetTestFiles() {
  base::FileEnumerator input_files(GetInputDir(), false,
                                   base::FileEnumerator::FILES);
  std::vector<base::FilePath> files;
  for (base::FilePath input_file = input_files.Next(); !input_file.empty();
       input_file = input_files.Next()) {
    files.push_back(input_file);
  }
  std::sort(files.begin(), files.end());

#if defined(OS_MAC)
  base::mac::ClearAmIBundledCache();
#endif  // defined(OS_MAC)

  return files;
}

std::string FormStructuresToString(
    const std::map<FormGlobalId, std::unique_ptr<FormStructure>>& forms) {
  std::string forms_string;
  // The forms are sorted by their global ID, which should make the order
  // deterministic.
  for (const auto& kv : forms) {
    const auto* form = kv.second.get();
    std::map<std::string, int> section_to_index;
    for (const auto& field : *form) {
      // Normalize the section by replacing the unique but platform-dependent
      // integers in |field->section| with consecutive unique integers.
      std::string section = field->section;
      size_t last_underscore = section.find_last_of('_');
      size_t second_last_underscore =
          section.find_last_of('_', last_underscore - 1);
      size_t next_dash = section.find_first_of('-', second_last_underscore);
      int new_section_index = static_cast<int>(section_to_index.size() + 1);
      int section_index =
          section_to_index.insert(std::make_pair(section, new_section_index))
              .first->second;
      if (second_last_underscore != std::string::npos &&
          next_dash != std::string::npos) {
        section = base::StringPrintf(
            "%s%d%s", section.substr(0, second_last_underscore + 1).c_str(),
            section_index, section.substr(next_dash).c_str());
      }

      forms_string += base::JoinString(
          {field->Type().ToString(), base::UTF16ToUTF8(field->name),
           base::UTF16ToUTF8(field->label), base::UTF16ToUTF8(field->value),
           section},
          base::StringPiece(" | "));
      forms_string += "\n";
    }
  }
  return forms_string;
}

}  // namespace

// A data-driven test for verifying Autofill heuristics. Each input is an HTML
// file that contains one or more forms. The corresponding output file lists the
// heuristically detected type for each field.
class FormStructureBrowserTest
    : public InProcessBrowserTest,
      public DataDrivenTest,
      public ::testing::WithParamInterface<base::FilePath> {
 protected:
  FormStructureBrowserTest();
  ~FormStructureBrowserTest() override;

  // InProcessBrowserTest
  void SetUpCommandLine(base::CommandLine* command_line) override;

  // BrowserTestBase
  void SetUpOnMainThread() override;

  // DataDrivenTest:
  void GenerateResults(const std::string& input, std::string* output) override;

 private:
  std::unique_ptr<HttpResponse> HandleRequest(const HttpRequest& request);

  base::test::ScopedFeatureList feature_list_;

  // The response content to be returned by the embedded test server. Note that
  // this is populated in the main thread as a part of the setup in the
  // GenerateResults method but it is consumed later in the IO thread by the
  // embedded test server to generate the response.
  std::string html_content_;
  DISALLOW_COPY_AND_ASSIGN(FormStructureBrowserTest);
};

FormStructureBrowserTest::FormStructureBrowserTest()
    : DataDrivenTest(GetTestDataDir()) {
  feature_list_.InitWithFeatures(
      // Enabled
      {// TODO(crbug.com/1098943): Remove once experiment is over.
       features::kAutofillEnableSupportForMoreStructureInNames,
       // TODO(crbug.com/1125978): Remove once launched.
       features::kAutofillEnableSupportForMoreStructureInAddresses,
       // TODO(crbug.com/896689): Remove once launched.
       features::kAutofillNameSectionsWithRendererIds,
       // TODO(crbug.com/1076175) Remove once launched.
       features::kAutofillUseNewSectioningMethod,
       // Remove once launched
       features::kAutofillEnableAugmentedPhoneCountryCode,
       // TODO(crbug.com/1157405) Remove once launched.
       features::kAutofillEnableDependentLocalityParsing,
       // TODO(crbug.com/1150895) Remove once launched.
       features::kAutofillParsingPatternsLanguageDetection,
       // TODO(crbug/1165780): Remove once shared labels are launched.
       features::kAutofillEnableSupportForParsingWithSharedLabels,
       // TODO(crbug/1190334): Remove once launched.
       features::kAutofillParseMerchantPromoCodeFields},
      // Disabled
      {});
}

FormStructureBrowserTest::~FormStructureBrowserTest() {}

void FormStructureBrowserTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  InProcessBrowserTest::SetUpCommandLine(command_line);
  // Suppress most output logs because we can't really control the output for
  // arbitrary test sites.
  command_line->AppendSwitchASCII(switches::kLoggingLevel, "2");
}

void FormStructureBrowserTest::SetUpOnMainThread() {
  InProcessBrowserTest::SetUpOnMainThread();

  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      &FormStructureBrowserTest::HandleRequest, base::Unretained(this)));
  ASSERT_TRUE(embedded_test_server()->Start());
}

void FormStructureBrowserTest::GenerateResults(const std::string& input,
                                               std::string* output) {
  // Cache the content to be returned by the embedded test server. This data
  // is readonly after this point.
  html_content_.clear();
  html_content_.reserve(input.length());
  for (const char c : input) {
    // Strip `\n`, `\t`, `\r` from |html| to match old `data:` URL behavior.
    // TODO(crbug/239819): the tests expect weird concatenation behavior based
    //   legacy data URL behavior. Fix this so the the tests better represent
    //   the parsing being done in the wild.
    if (c != '\r' && c != '\n' && c != '\t')
      html_content_.push_back(c);
  }

  // Navigate to the test html content.
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/test.html")));

  // Dump the form fields (and their inferred field types).
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ContentAutofillDriver* autofill_driver =
      ContentAutofillDriverFactory::FromWebContents(web_contents)
          ->DriverForFrame(web_contents->GetMainFrame());
  ASSERT_NE(nullptr, autofill_driver);
  BrowserAutofillManager* autofill_manager =
      autofill_driver->browser_autofill_manager();
  ASSERT_NE(nullptr, autofill_manager);
  *output = FormStructuresToString(autofill_manager->form_structures());
}

std::unique_ptr<HttpResponse> FormStructureBrowserTest::HandleRequest(
    const HttpRequest& request) {
  auto response = std::make_unique<BasicHttpResponse>();
  response->set_code(net::HTTP_OK);
  response->set_content(html_content_);
  response->set_content_type("text/html; charset=utf-8");
  return std::move(response);
}

IN_PROC_BROWSER_TEST_P(FormStructureBrowserTest, DataDrivenHeuristics) {
  // Prints the path of the test to be executed.
  LOG(INFO) << GetParam().MaybeAsASCII();
  bool is_expected_to_pass =
      !base::Contains(GetFailingTestNames(), GetParam().BaseName().value());
  RunOneDataDrivenTest(GetParam(), GetOutputDirectory(kTestName),
                       is_expected_to_pass);
}

INSTANTIATE_TEST_SUITE_P(AllForms,
                         FormStructureBrowserTest,
                         testing::ValuesIn(GetTestFiles()));

}  // namespace autofill
