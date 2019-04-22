// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/values_test_util.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/base/escape.h"
#include "net/base/url_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/ssl/ssl_server_config.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

namespace content {

namespace {

void GetKey(const base::DictionaryValue& dict,
            const std::string& key,
            std::string* out_value) {
  auto* value = dict.FindKeyOfType(key, base::Value::Type::STRING);
  ASSERT_TRUE(value);
  *out_value = value->GetString();
}

void GetKey(const base::DictionaryValue& dict,
            const std::string& key,
            int* out_value) {
  auto* value = dict.FindKeyOfType(key, base::Value::Type::INTEGER);
  ASSERT_TRUE(value);
  *out_value = value->GetInt();
}

// Helper since the default output of EXPECT_EQ isn't useful when debugging
// failures, it doesn't recurse into the dictionary.
void ExpectEqual(const base::DictionaryValue& expected,
                 const base::DictionaryValue& actual) {
  EXPECT_EQ(expected, actual)
      << "\nExpected: " << expected << "\nActual: " << actual;
}

const char kFileContent[] = "uploaded file content";
const size_t kFileSize = base::size(kFileContent) - 1;
}  // namespace

// Tests POST requests that include a file and are intercepted by a service
// worker. This is a browser test rather than a web test because as
// https://crbug.com/786510 describes, http tests involving file uploads usually
// need to be in the http/tests/local directory, which runs tests from file:
// URLs while serving http resources from the http server, but this trick can
// break the test when Site Isolation is enabled and content from different
// origins end up in different processes.
class ServiceWorkerFileUploadTest : public ContentBrowserTest {
 public:
  ServiceWorkerFileUploadTest() = default;

  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());

    ContentBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    // Make all hosts resolve to 127.0.0.1 so the same embedded test server can
    // be used for cross-origin URLs.
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->StartAcceptingConnections();
  }

  enum class TargetOrigin { kSameOrigin, kCrossOrigin };

  // Builds a target URL for the form to submit to. The URL has path
  // "|path|?|query|" so they can be adjusted to tell the service worker how to
  // handle the request.
  GURL BuildTargetUrl(const std::string& path, const std::string& query) {
    return embedded_test_server()->GetURL(path + "?" + query);
  }

  // Tests submitting a form that is intercepted by a service worker. The form
  // has several input elements including a file; this test creates a temp file
  // and uploads it. The service worker is expected to respond with JSON
  // describing the request data it saw.
  // - |target_url|: where to submit the form to.
  // - |target_origin|: whether to submit the form from a page that is
  //   cross-origin to the target.
  // - |out_file_name|: the name of the file this test uploaded via the form.
  // - |out_result|: the body of the resulting document.
  void RunTest(const GURL& target_url,
               TargetOrigin target_origin,
               std::string* out_file_name,
               std::string* out_result) {
    // Install the service worker. Use root scope since the network fallback
    // test needs it: the service worker will intercept "/echo", then fall back
    // to network, and the request gets handled by the default request handler
    // for that URL which echoes back the request.
    EXPECT_TRUE(NavigateToURL(
        shell(), embedded_test_server()->GetURL(
                     "/service_worker/create_service_worker.html")));
    EXPECT_EQ("DONE",
              EvalJs(shell(), "register('file_upload_worker.js', '/');"));

    // Generate the URL for the page with the file upload form.
    GURL page_url = embedded_test_server()->GetURL("/service_worker/form.html");
    // If |target_origin| says to test submitting to a cross-origin target, set
    // this page to a different origin. The |target_url| is expected to point
    // back to the original origin with the service worker.
    if (target_origin == TargetOrigin::kCrossOrigin) {
      GURL::Replacements replacements;
      replacements.SetHostStr("cross-origin.example.com");
      page_url = page_url.ReplaceComponents(replacements);
    }
    // Set the target to |target_url|.
    page_url = net::AppendQueryParameter(page_url, "target", target_url.spec());

    // Navigate to the page with a file upload form.
    EXPECT_TRUE(NavigateToURL(shell(), page_url));

    // Prepare a file for the upload form.
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::ScopedTempDir temp_dir;
    base::FilePath file_path;
    ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
    ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_dir.GetPath(), &file_path));
    ASSERT_EQ(static_cast<int>(kFileSize),
              base::WriteFile(file_path, kFileContent, kFileSize));

    // Fill out the form to refer to the test file.
    base::RunLoop run_loop;
    auto delegate = std::make_unique<FileChooserDelegate>(
        file_path, run_loop.QuitClosure());
    shell()->web_contents()->SetDelegate(delegate.get());
    EXPECT_TRUE(ExecJs(shell(), "fileInput.click();"));
    run_loop.Run();

    // Submit the form.
    TestNavigationObserver form_post_observer(shell()->web_contents(), 1);
    EXPECT_TRUE(ExecJs(shell(), "form.submit();"));
    form_post_observer.Wait();

    // Extract the body payload.
    EvalJsResult result = EvalJs(shell()->web_contents()->GetMainFrame(),
                                 "document.body.textContent");
    ASSERT_TRUE(result.error.empty());

    *out_file_name = file_path.BaseName().MaybeAsASCII();
    *out_result = result.ExtractString();
  }

  // Helper for tests where the service worker calls respondWith().
  void RunRespondWithTest(const std::string& target_query,
                          TargetOrigin target_origin,
                          std::string* out_filename,
                          std::unique_ptr<base::DictionaryValue>* out_result) {
    std::string result;
    RunTest(BuildTargetUrl("/service_worker/upload", target_query),
            TargetOrigin::kSameOrigin, out_filename, &result);
    *out_result =
        base::DictionaryValue::From(base::test::ParseJsonDeprecated(result));
    ASSERT_TRUE(*out_result);
  }

  // Helper for tests where the service worker falls back to network.
  void RunNetworkFallbackTest(TargetOrigin target_origin) {
    std::string filename;
    std::string result;
    // Use "/echo" so the request hits the echo default handler after falling
    // back to network.
    // Use "getAs=fallback" to tell the service worker to fall back.
    RunTest(BuildTargetUrl("/echo", "getAs=fallback"), target_origin, &filename,
            &result);

    // This isn't as rigorous as BuildExpectedBodyAsFormData(). The test author
    // couldn't get that to work maybe because \r\n get stripped somewhere.
    EXPECT_THAT(result, ::testing::HasSubstr(kFileContent));
    EXPECT_THAT(result, ::testing::HasSubstr(filename));
    EXPECT_THAT(result, ::testing::HasSubstr("form-data; name=\"file\""));
  }

  std::string BuildExpectedBodyAsText(const std::string& boundary,
                                      const std::string& filename) {
    return "--" + boundary + "\r\n" +
           "Content-Disposition: form-data; name=\"text1\"\r\n" + "\r\n" +
           "textValue1\r\n" + "--" + boundary + "\r\n" +
           "Content-Disposition: form-data; name=\"text2\"\r\n" + "\r\n" +
           "textValue2\r\n" + "--" + boundary + "\r\n" +
           "Content-Disposition: form-data; name=\"file\"; "
           "filename=\"" +
           filename + "\"\r\n" + "Content-Type: application/octet-stream\r\n" +
           "\r\n" + kFileContent + "\r\n" + "--" + boundary + "--\r\n";
  }

  std::unique_ptr<base::DictionaryValue> BuildExpectedBodyAsFormData(
      const std::string& filename) {
    std::string expectation = R"({
      "entries": [
        {
          "key": "text1",
          "value": {
            "type": "string",
            "data": "textValue1"
          }
        },
        {
          "key": "text2",
          "value": {
            "type": "string",
            "data": "textValue2"
          }
        },
        {
          "key": "file",
          "value": {
            "type": "file",
            "name": "@PATH@",
            "size": @SIZE@
          }
        }
      ]
    })";
    base::ReplaceFirstSubstringAfterOffset(&expectation, 0, "@PATH@", filename);
    base::ReplaceFirstSubstringAfterOffset(&expectation, 0, "@SIZE@",
                                           base::NumberToString(kFileSize));

    return base::DictionaryValue::From(
        base::test::ParseJsonDeprecated(expectation));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerFileUploadTest);
};

// Tests using Request.text().
IN_PROC_BROWSER_TEST_F(ServiceWorkerFileUploadTest, AsText) {
  std::string filename;
  std::unique_ptr<base::DictionaryValue> dict;
  RunRespondWithTest("getAs=text", TargetOrigin::kSameOrigin, &filename, &dict);

  std::string boundary;
  GetKey(*dict, "boundary", &boundary);
  std::string body;
  GetKey(*dict, "body", &body);
  std::string expected_body = BuildExpectedBodyAsText(boundary, filename);
  EXPECT_EQ(expected_body, body);
}

// Tests using Request.blob().
IN_PROC_BROWSER_TEST_F(ServiceWorkerFileUploadTest, AsBlob) {
  std::string filename;
  std::unique_ptr<base::DictionaryValue> dict;
  RunRespondWithTest("getAs=blob", TargetOrigin::kSameOrigin, &filename, &dict);

  std::string boundary;
  GetKey(*dict, "boundary", &boundary);
  int size;
  GetKey(*dict, "bodySize", &size);
  std::string expected_body = BuildExpectedBodyAsText(boundary, filename);
  EXPECT_EQ(base::MakeStrictNum(expected_body.size()), size);
}

// Tests using Request.formData().
IN_PROC_BROWSER_TEST_F(ServiceWorkerFileUploadTest, AsFormData) {
  std::string filename;
  std::unique_ptr<base::DictionaryValue> dict;
  RunRespondWithTest("getAs=formData", TargetOrigin::kSameOrigin, &filename,
                     &dict);

  ExpectEqual(*BuildExpectedBodyAsFormData(filename), *dict);
}

// Tests network fallback.
IN_PROC_BROWSER_TEST_F(ServiceWorkerFileUploadTest, NetworkFallback) {
  RunNetworkFallbackTest(TargetOrigin::kSameOrigin);
}

// Tests using Request.formData() when the form was submitted to a cross-origin
// target. Regression test for https://crbug.com/916070.
IN_PROC_BROWSER_TEST_F(ServiceWorkerFileUploadTest, AsFormData_CrossOrigin) {
  std::string filename;
  std::unique_ptr<base::DictionaryValue> dict;
  RunRespondWithTest("getAs=formData", TargetOrigin::kCrossOrigin, &filename,
                     &dict);

  ExpectEqual(*BuildExpectedBodyAsFormData(filename), *dict);
}

// Tests network fallback when the form was submitted to a cross-origin target.
IN_PROC_BROWSER_TEST_F(ServiceWorkerFileUploadTest,
                       NetworkFallback_CrossOrigin) {
  RunNetworkFallbackTest(TargetOrigin::kCrossOrigin);
}

}  // namespace content
