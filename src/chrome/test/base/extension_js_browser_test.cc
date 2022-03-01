// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/extension_js_browser_test.h"

#include <memory>

#include "base/callback.h"
#include "base/json/json_reader.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/browsertest_util.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_host_test_helper.h"

ExtensionJSBrowserTest::ExtensionJSBrowserTest() {}

ExtensionJSBrowserTest::~ExtensionJSBrowserTest() {}

void ExtensionJSBrowserTest::WaitForExtension(const char* extension_id,
                                              base::OnceClosure load_cb) {
  extension_id_ = extension_id;
  extensions::ExtensionHostTestHelper host_helper(browser()->profile(),
                                                  extension_id);
  std::move(load_cb).Run();
  extensions::ExtensionHost* extension_host =
      host_helper.WaitForHostCompletedFirstLoad();
  ASSERT_TRUE(extension_host);
  extension_host_browser_context_ = extension_host->browser_context();
}

bool ExtensionJSBrowserTest::RunJavascriptTestF(bool is_async,
                                                const std::string& test_fixture,
                                                const std::string& test_name) {
  if (!extension_host_browser_context_) {
    ADD_FAILURE() << "ExtensionHost failed to start";
    return false;
  }
  std::vector<base::Value> args;
  args.push_back(base::Value(test_fixture));
  args.push_back(base::Value(test_name));
  std::vector<std::u16string> scripts;

  base::Value test_runner_params(base::Value::Type::DICTIONARY);
  if (embedded_test_server()->Started()) {
    test_runner_params.SetKey(
        "testServerBaseUrl",
        base::Value(embedded_test_server()->base_url().spec()));
  }

  if (!libs_loaded_) {
    BuildJavascriptLibraries(&scripts);
    libs_loaded_ = true;
  }

  scripts.push_back(base::UTF8ToUTF16(content::JsReplace(
      "const testRunnerParams = $1;", std::move(test_runner_params))));

  scripts.push_back(
      BuildRunTestJSCall(is_async, "RUN_TEST_F", std::move(args)));

  std::u16string script_16 = base::JoinString(scripts, u"\n");
  std::string script = base::UTF16ToUTF8(script_16);

  std::string result =
      extensions::browsertest_util::ExecuteScriptInBackgroundPage(
          Profile::FromBrowserContext(extension_host_browser_context_),
          extension_id_, script);

  std::unique_ptr<base::Value> value_result =
      base::JSONReader::ReadDeprecated(result);
  CHECK_EQ(base::Value::Type::DICTIONARY, value_result->type());
  base::DictionaryValue* dict_value =
      static_cast<base::DictionaryValue*>(value_result.get());

  std::string test_result_message;
  bool test_result = dict_value->FindBoolKey("result").value();
  CHECK(dict_value->GetString("message", &test_result_message));
  if (!test_result_message.empty())
    ADD_FAILURE() << test_result_message;
  return test_result;
}
