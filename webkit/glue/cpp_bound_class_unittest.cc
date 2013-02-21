// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests for CppBoundClass, in conjunction with CppBindingExample.  Binds
// a CppBindingExample class into JavaScript in a custom test shell and tests
// the binding from the outside by loading JS into the shell.

#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/Platform/chromium/public/Platform.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebData.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebSize.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebString.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebURL.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebURLRequest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrameClient.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebKit.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSettings.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebViewClient.h"
#include "webkit/glue/cpp_binding_example.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/user_agent/user_agent.h"
#include "webkit/user_agent/user_agent_util.h"

using WebKit::WebFrame;
using WebKit::WebView;
using webkit_glue::CppArgumentList;
using webkit_glue::CppBindingExample;
using webkit_glue::CppVariant;

namespace {

class CppBindingExampleSubObject : public CppBindingExample {
 public:
  CppBindingExampleSubObject() {
    sub_value_.Set("sub!");
    BindProperty("sub_value", &sub_value_);
  }
 private:
  CppVariant sub_value_;
};


class CppBindingExampleWithOptionalFallback : public CppBindingExample {
 public:
  CppBindingExampleWithOptionalFallback() {
    BindProperty("sub_object", sub_object_.GetAsCppVariant());
  }

  void set_fallback_method_enabled(bool state) {
    BindFallbackCallback(state ?
        base::Bind(&CppBindingExampleWithOptionalFallback::fallbackMethod,
                   base::Unretained(this))
        : CppBoundClass::Callback());
  }

  // The fallback method does nothing, but because of it the JavaScript keeps
  // running when a nonexistent method is called on an object.
  void fallbackMethod(const CppArgumentList& args, CppVariant* result) {
  }

 private:
  CppBindingExampleSubObject sub_object_;
};

class TestWebFrameClient : public WebKit::WebFrameClient {
 public:
  virtual void didClearWindowObject(WebKit::WebFrame* frame) OVERRIDE {
    example_bound_class_.BindToJavascript(frame, "example");
  }
  void set_fallback_method_enabled(bool use_fallback) {
    example_bound_class_.set_fallback_method_enabled(use_fallback);
  }
 private:
  CppBindingExampleWithOptionalFallback example_bound_class_;
};

class TestWebViewClient : public WebKit::WebViewClient {
};

class CppBoundClassTest : public testing::Test, public WebKit::WebFrameClient {
 public:
  CppBoundClassTest() : webview_(NULL) { }

  virtual void SetUp() OVERRIDE {
    webview_ = WebView::create(&webview_client_);
    webview_->settings()->setJavaScriptEnabled(true);
    webview_->initializeMainFrame(&webframe_client_);
    webframe_client_.set_fallback_method_enabled(useFallback());
    webkit_glue::SetUserAgent(webkit_glue::BuildUserAgentFromProduct(
        "TestShell/0.0.0.0"), false);

    WebKit::WebURLRequest urlRequest;
    urlRequest.initialize();
    urlRequest.setURL(GURL("about:blank"));
    webframe()->loadRequest(urlRequest);
  }

  virtual void TearDown() OVERRIDE {
    if (webview_)
      webview_->close();
  }

  WebFrame* webframe() {
    return webview_->mainFrame();
  }

  // Wraps the given JavaScript snippet in <html><body><script> tags, then
  // loads it into a webframe so it is executed.
  void ExecuteJavaScript(const std::string& javascript) {
    std::string html = "<html><body>";
    html.append("<script>");
    html.append(javascript);
    html.append("</script></body></html>");
    webframe()->loadHTMLString(html, GURL("about:blank"));
    MessageLoop::current()->RunUntilIdle();
  }

  // Executes the specified JavaScript and checks to be sure that the resulting
  // document text is exactly "SUCCESS".
  void CheckJavaScriptSuccess(const std::string& javascript) {
    ExecuteJavaScript(javascript);
    EXPECT_EQ("SUCCESS",
              UTF16ToASCII(webkit_glue::DumpDocumentText(webframe())));
  }

  // Executes the specified JavaScript and checks that the resulting document
  // text is empty.
  void CheckJavaScriptFailure(const std::string& javascript) {
    ExecuteJavaScript(javascript);
    EXPECT_EQ("", UTF16ToASCII(webkit_glue::DumpDocumentText(webframe())));
  }

  // Constructs a JavaScript snippet that evaluates and compares the left and
  // right expressions, printing "SUCCESS" to the page if they are equal and
  // printing their actual values if they are not.  Any strings in the
  // expressions should be enclosed in single quotes, and no double quotes
  // should appear in either expression (even if escaped). (If a test case
  // is added that needs fancier quoting, Json::valueToQuotedString could be
  // used here.  For now, it's not worth adding the dependency.)
  std::string BuildJSCondition(std::string left, std::string right) {
    return "var leftval = " + left + ";" +
           "var rightval = " + right + ";" +
           "if (leftval == rightval) {" +
           "  document.writeln('SUCCESS');" +
           "} else {" +
           "  document.writeln(\"" +
                left + " [\" + leftval + \"] != " +
                right + " [\" + rightval + \"]\");" +
           "}";
  }

 protected:
  virtual bool useFallback() {
    return false;
  }

 private:
  WebView* webview_;
  TestWebFrameClient webframe_client_;
  TestWebViewClient webview_client_;
};

class CppBoundClassWithFallbackMethodTest : public CppBoundClassTest {
 protected:
  virtual bool useFallback() OVERRIDE {
    return true;
  }
};

// Ensures that the example object has been bound to JS.
TEST_F(CppBoundClassTest, ObjectExists) {
  std::string js = BuildJSCondition("typeof window.example", "'object'");
  CheckJavaScriptSuccess(js);

  // An additional check to test our test.
  js = BuildJSCondition("typeof window.invalid_object", "'undefined'");
  CheckJavaScriptSuccess(js);
}

TEST_F(CppBoundClassTest, PropertiesAreInitialized) {
  std::string js = BuildJSCondition("example.my_value", "10");
  CheckJavaScriptSuccess(js);

  js = BuildJSCondition("example.my_other_value", "'Reinitialized!'");
  CheckJavaScriptSuccess(js);
}

TEST_F(CppBoundClassTest, SubOject) {
  std::string js = BuildJSCondition("typeof window.example.sub_object",
                                    "'object'");
  CheckJavaScriptSuccess(js);

  js = BuildJSCondition("example.sub_object.sub_value", "'sub!'");
  CheckJavaScriptSuccess(js);
}

TEST_F(CppBoundClassTest, SetAndGetProperties) {
  // The property on the left will be set to the value on the right, then
  // checked to make sure it holds that same value.
  static const std::string tests[] = {
    "example.my_value", "7",
    "example.my_value", "'test'",
    "example.my_other_value", "3.14",
    "example.my_other_value", "false",
    "" // Array end marker: insert additional test pairs before this.
  };

  for (int i = 0; tests[i] != ""; i += 2) {
    std::string left = tests[i];
    std::string right = tests[i + 1];
    // left = right;
    std::string js = left;
    js.append(" = ");
    js.append(right);
    js.append(";");
    js.append(BuildJSCondition(left, right));
    CheckJavaScriptSuccess(js);
  }
}

TEST_F(CppBoundClassTest, SetAndGetPropertiesWithCallbacks) {
  // TODO(dglazkov): fix NPObject issues around failing property setters and
  // getters and add tests for situations when GetProperty or SetProperty fail.
  std::string js = "var result = 'SUCCESS';\n"
    "example.my_value_with_callback = 10;\n"
    "if (example.my_value_with_callback != 10)\n"
    "  result = 'FAIL: unable to set property.';\n"
    "example.my_value_with_callback = 11;\n"
    "if (example.my_value_with_callback != 11)\n"
    "  result = 'FAIL: unable to set property again';\n"
    "if (example.same != 42)\n"
    "  result = 'FAIL: same property should always be 42';\n"
    "example.same = 24;\n"
    "if (example.same != 42)\n"
    "  result = 'FAIL: same property should always be 42';\n"
    "document.writeln(result);\n";
  CheckJavaScriptSuccess(js);
}

TEST_F(CppBoundClassTest, InvokeMethods) {
  // The expression on the left is expected to return the value on the right.
  static const std::string tests[] = {
    "example.echoValue(true)", "true",
    "example.echoValue(13)", "13",
    "example.echoValue(2.718)", "2.718",
    "example.echoValue('yes')", "'yes'",
    "example.echoValue()", "null",     // Too few arguments

    "example.echoType(false)", "true",
    "example.echoType(19)", "3.14159",
    "example.echoType(9.876)", "3.14159",
    "example.echoType('test string')", "'Success!'",
    "example.echoType()", "null",      // Too few arguments

    // Comparing floats that aren't integer-valued is usually problematic due
    // to rounding, but exact powers of 2 should also be safe.
    "example.plus(2.5, 18.0)", "20.5",
    "example.plus(2, 3.25)", "5.25",
    "example.plus(2, 3)", "5",
    "example.plus()", "null",             // Too few arguments
    "example.plus(1)", "null",            // Too few arguments
    "example.plus(1, 'test')", "null",    // Wrong argument type
    "example.plus('test', 2)", "null",    // Wrong argument type
    "example.plus('one', 'two')", "null", // Wrong argument type
    "" // Array end marker: insert additional test pairs before this.
  };

  for (int i = 0; tests[i] != ""; i+= 2) {
    std::string left = tests[i];
    std::string right = tests[i + 1];
    std::string js = BuildJSCondition(left, right);
    CheckJavaScriptSuccess(js);
  }

  std::string js = "example.my_value = 3.25; example.my_other_value = 1.25;";
  js.append(BuildJSCondition(
      "example.plus(example.my_value, example.my_other_value)", "4.5"));
  CheckJavaScriptSuccess(js);
}

// Tests that invoking a nonexistent method with no fallback method stops the
// script's execution
TEST_F(CppBoundClassTest,
       InvokeNonexistentMethodNoFallback) {
  std::string js = "example.nonExistentMethod();document.writeln('SUCCESS');";
  CheckJavaScriptFailure(js);
}

// Ensures existent methods can be invoked successfully when the fallback method
// is used
TEST_F(CppBoundClassWithFallbackMethodTest,
       InvokeExistentMethodsWithFallback) {
  std::string js = BuildJSCondition("example.echoValue(34)", "34");
  CheckJavaScriptSuccess(js);
}

}  // namespace
