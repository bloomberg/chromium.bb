// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/base/cef_bind.h"
#include "include/test/cef_test_helpers.h"
#include "include/wrapper/cef_closure_task.h"
#include "tests/ceftests/test_handler.h"
#include "tests/gtest/include/gtest/gtest.h"

namespace {

const char* kStartUrl = "http://tests/JSDialogTestHandler.Start";
const char* kEndUrl = "http://tests/JSDialogTestHandler.End?r=";

class JSDialogTestHandler : public TestHandler {
 public:
  enum TestType {
    TYPE_ALERT,
    TYPE_CONFIRM,
    TYPE_PROMPT,
    TYPE_ONBEFOREUNLOAD,
  };
  enum TestMode {
    MODE_SUPPRESS,
    MODE_RUN_IMMEDIATE,
    MODE_RUN_DELAYED,
  };

  JSDialogTestHandler(TestType type,
                      TestMode mode,
                      bool success,
                      const std::string& user_input,
                      const std::string& result)
      : type_(type),
        mode_(mode),
        success_(success),
        user_input_(user_input),
        result_(result) {}

  void RunTest() override {
    std::string content = "<html><head><body>START<script>";
    if (type_ == TYPE_ALERT) {
      content +=
          "alert('My alert message'); "
          "document.location='" +
          std::string(kEndUrl) + "';";
    } else if (type_ == TYPE_CONFIRM) {
      content +=
          "var r = confirm('My confirm message')?'ok':'cancel'; "
          "document.location='" +
          std::string(kEndUrl) + "'+r;";
    } else if (type_ == TYPE_PROMPT) {
      content +=
          "var r = prompt('My prompt message','my default'); "
          "document.location='" +
          std::string(kEndUrl) + "'+r;";
    } else if (type_ == TYPE_ONBEFOREUNLOAD) {
      content +=
          "window.onbeforeunload=function() {"
          " return 'My unload message'; };";
    }
    content += "</script></body></html>";

    AddResource(kStartUrl, content, "text/html");
    AddResource(kEndUrl, "<html><body>END</body></html>", "text/html");

    // Create the browser
    CreateBrowser(kStartUrl);

    // Time out the test after a reasonable period of time.
    SetTestTimeout();
  }

  void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int httpStatusCode) override {
    if (!frame->IsMain())
      return;

    std::string url = frame->GetURL();
    if (url.find(kEndUrl) == 0) {
      got_onloadend_.yes();

      std::string result = url.substr(strlen(kEndUrl));
      EXPECT_STREQ(result_.c_str(), result.c_str());

      DestroyTest();
    } else if (type_ == TYPE_ONBEFOREUNLOAD) {
      // Send the page a user gesture to enable firing of the onbefureunload
      // handler. See https://crbug.com/707007.
      CefExecuteJavaScriptWithUserGestureForTests(frame, CefString());

      // Trigger the onunload handler.
      frame->LoadURL(kEndUrl);
    }
  }

  void Continue(CefRefPtr<CefJSDialogCallback> callback) {
    callback->Continue(success_, user_input_);
  }

  bool OnJSDialog(CefRefPtr<CefBrowser> browser,
                  const CefString& origin_url,
                  JSDialogType dialog_type,
                  const CefString& message_text,
                  const CefString& default_prompt_text,
                  CefRefPtr<CefJSDialogCallback> callback,
                  bool& suppress_message) override {
    got_onjsdialog_.yes();

    EXPECT_STREQ(kStartUrl, origin_url.ToString().c_str());

    if (type_ == TYPE_ALERT) {
      EXPECT_EQ(JSDIALOGTYPE_ALERT, dialog_type);
      EXPECT_STREQ("My alert message", message_text.ToString().c_str());
      EXPECT_TRUE(default_prompt_text.empty());
    } else if (type_ == TYPE_CONFIRM) {
      EXPECT_EQ(JSDIALOGTYPE_CONFIRM, dialog_type);
      EXPECT_STREQ("My confirm message", message_text.ToString().c_str());
      EXPECT_TRUE(default_prompt_text.empty());
    } else if (type_ == TYPE_PROMPT) {
      EXPECT_EQ(JSDIALOGTYPE_PROMPT, dialog_type);
      EXPECT_STREQ("My prompt message", message_text.ToString().c_str());
      EXPECT_STREQ("my default", default_prompt_text.ToString().c_str());
    }

    EXPECT_FALSE(suppress_message);

    if (mode_ == MODE_SUPPRESS) {
      // Suppress the dialog.
      suppress_message = true;
      return false;
    } else if (mode_ == MODE_RUN_IMMEDIATE) {
      // Continue immediately.
      callback->Continue(success_, user_input_);
    } else if (mode_ == MODE_RUN_DELAYED) {
      // Continue asynchronously.
      CefPostTask(TID_UI,
                  base::Bind(&JSDialogTestHandler::Continue, this, callback));
    }

    return true;
  }

  bool OnBeforeUnloadDialog(CefRefPtr<CefBrowser> browser,
                            const CefString& message_text,
                            bool is_reload,
                            CefRefPtr<CefJSDialogCallback> callback) override {
    got_onbeforeunloaddialog_.yes();

    if (type_ == TYPE_ONBEFOREUNLOAD) {
      // The message is no longer configurable via JavaScript.
      // See http://crbug.com/587940.
      EXPECT_STREQ("Is it OK to leave/reload this page?",
                   message_text.ToString().c_str());
      EXPECT_FALSE(is_reload);
    }

    if (mode_ == MODE_RUN_IMMEDIATE) {
      // Continue immediately.
      callback->Continue(success_, user_input_);
    } else if (mode_ == MODE_RUN_DELAYED) {
      // Continue asynchronously.
      CefPostTask(TID_UI,
                  base::Bind(&JSDialogTestHandler::Continue, this, callback));
    }

    return true;
  }

  void OnResetDialogState(CefRefPtr<CefBrowser> browser) override {
    got_onresetdialogstate_.yes();
  }

  TestType type_;
  TestMode mode_;
  bool success_;
  std::string user_input_;
  std::string result_;

  TrackCallback got_onjsdialog_;
  TrackCallback got_onbeforeunloaddialog_;
  TrackCallback got_onresetdialogstate_;
  TrackCallback got_onloadend_;

  IMPLEMENT_REFCOUNTING(JSDialogTestHandler);
};

}  // namespace

// Alert dialog with suppression.
TEST(JSDialogTest, AlertSuppress) {
  CefRefPtr<JSDialogTestHandler> handler = new JSDialogTestHandler(
      JSDialogTestHandler::TYPE_ALERT, JSDialogTestHandler::MODE_SUPPRESS,
      true,  // success
      "",    // user_input
      "");   // result
  handler->ExecuteTest();

  EXPECT_TRUE(handler->got_onjsdialog_);
  EXPECT_FALSE(handler->got_onbeforeunloaddialog_);
  EXPECT_TRUE(handler->got_onresetdialogstate_);
  EXPECT_TRUE(handler->got_onloadend_);

  ReleaseAndWaitForDestructor(handler);
}

// Alert dialog with immediate callback.
TEST(JSDialogTest, AlertRunImmediate) {
  CefRefPtr<JSDialogTestHandler> handler = new JSDialogTestHandler(
      JSDialogTestHandler::TYPE_ALERT, JSDialogTestHandler::MODE_RUN_IMMEDIATE,
      true,  // success
      "",    // user_input
      "");   // result
  handler->ExecuteTest();

  EXPECT_TRUE(handler->got_onjsdialog_);
  EXPECT_FALSE(handler->got_onbeforeunloaddialog_);
  EXPECT_TRUE(handler->got_onresetdialogstate_);
  EXPECT_TRUE(handler->got_onloadend_);

  ReleaseAndWaitForDestructor(handler);
}

// Alert dialog with delayed callback.
TEST(JSDialogTest, AlertRunDelayed) {
  CefRefPtr<JSDialogTestHandler> handler = new JSDialogTestHandler(
      JSDialogTestHandler::TYPE_ALERT, JSDialogTestHandler::MODE_RUN_DELAYED,
      true,  // success
      "",    // user_input
      "");   // result
  handler->ExecuteTest();

  EXPECT_TRUE(handler->got_onjsdialog_);
  EXPECT_FALSE(handler->got_onbeforeunloaddialog_);
  EXPECT_TRUE(handler->got_onresetdialogstate_);
  EXPECT_TRUE(handler->got_onloadend_);

  ReleaseAndWaitForDestructor(handler);
}

// Confirm dialog with suppression.
TEST(JSDialogTest, ConfirmSuppress) {
  CefRefPtr<JSDialogTestHandler> handler = new JSDialogTestHandler(
      JSDialogTestHandler::TYPE_CONFIRM, JSDialogTestHandler::MODE_SUPPRESS,
      true,       // success
      "",         // user_input
      "cancel");  // result
  handler->ExecuteTest();

  EXPECT_TRUE(handler->got_onjsdialog_);
  EXPECT_FALSE(handler->got_onbeforeunloaddialog_);
  EXPECT_TRUE(handler->got_onresetdialogstate_);
  EXPECT_TRUE(handler->got_onloadend_);

  ReleaseAndWaitForDestructor(handler);
}

// Confirm dialog run immediately return OK.
TEST(JSDialogTest, ConfirmRunImmediateOk) {
  CefRefPtr<JSDialogTestHandler> handler =
      new JSDialogTestHandler(JSDialogTestHandler::TYPE_CONFIRM,
                              JSDialogTestHandler::MODE_RUN_IMMEDIATE,
                              true,   // success
                              "",     // user_input
                              "ok");  // result
  handler->ExecuteTest();

  EXPECT_TRUE(handler->got_onjsdialog_);
  EXPECT_FALSE(handler->got_onbeforeunloaddialog_);
  EXPECT_TRUE(handler->got_onresetdialogstate_);
  EXPECT_TRUE(handler->got_onloadend_);

  ReleaseAndWaitForDestructor(handler);
}

// Confirm dialog run immediately return Cancel.
TEST(JSDialogTest, ConfirmRunImmediateCancel) {
  CefRefPtr<JSDialogTestHandler> handler =
      new JSDialogTestHandler(JSDialogTestHandler::TYPE_CONFIRM,
                              JSDialogTestHandler::MODE_RUN_IMMEDIATE,
                              false,      // success
                              "",         // user_input
                              "cancel");  // result
  handler->ExecuteTest();

  EXPECT_TRUE(handler->got_onjsdialog_);
  EXPECT_FALSE(handler->got_onbeforeunloaddialog_);
  EXPECT_TRUE(handler->got_onresetdialogstate_);
  EXPECT_TRUE(handler->got_onloadend_);

  ReleaseAndWaitForDestructor(handler);
}

// Confirm dialog run delayed return OK.
TEST(JSDialogTest, ConfirmRunDelayedOk) {
  CefRefPtr<JSDialogTestHandler> handler = new JSDialogTestHandler(
      JSDialogTestHandler::TYPE_CONFIRM, JSDialogTestHandler::MODE_RUN_DELAYED,
      true,   // success
      "",     // user_input
      "ok");  // result
  handler->ExecuteTest();

  EXPECT_TRUE(handler->got_onjsdialog_);
  EXPECT_FALSE(handler->got_onbeforeunloaddialog_);
  EXPECT_TRUE(handler->got_onresetdialogstate_);
  EXPECT_TRUE(handler->got_onloadend_);

  ReleaseAndWaitForDestructor(handler);
}

// Confirm dialog run delayed return Cancel.
TEST(JSDialogTest, ConfirmRunDelayedCancel) {
  CefRefPtr<JSDialogTestHandler> handler = new JSDialogTestHandler(
      JSDialogTestHandler::TYPE_CONFIRM, JSDialogTestHandler::MODE_RUN_DELAYED,
      false,      // success
      "",         // user_input
      "cancel");  // result
  handler->ExecuteTest();

  EXPECT_TRUE(handler->got_onjsdialog_);
  EXPECT_FALSE(handler->got_onbeforeunloaddialog_);
  EXPECT_TRUE(handler->got_onresetdialogstate_);
  EXPECT_TRUE(handler->got_onloadend_);

  ReleaseAndWaitForDestructor(handler);
}

// Prompt dialog with suppression.
TEST(JSDialogTest, PromptSuppress) {
  CefRefPtr<JSDialogTestHandler> handler = new JSDialogTestHandler(
      JSDialogTestHandler::TYPE_PROMPT, JSDialogTestHandler::MODE_SUPPRESS,
      true,          // success
      "some_value",  // user_input
      "null");       // result
  handler->ExecuteTest();

  EXPECT_TRUE(handler->got_onjsdialog_);
  EXPECT_FALSE(handler->got_onbeforeunloaddialog_);
  EXPECT_TRUE(handler->got_onresetdialogstate_);
  EXPECT_TRUE(handler->got_onloadend_);

  ReleaseAndWaitForDestructor(handler);
}

// Prompt dialog run immediately return OK.
TEST(JSDialogTest, PromptRunImmediateOk) {
  CefRefPtr<JSDialogTestHandler> handler = new JSDialogTestHandler(
      JSDialogTestHandler::TYPE_PROMPT, JSDialogTestHandler::MODE_RUN_IMMEDIATE,
      true,           // success
      "some_value",   // user_input
      "some_value");  // result
  handler->ExecuteTest();

  EXPECT_TRUE(handler->got_onjsdialog_);
  EXPECT_FALSE(handler->got_onbeforeunloaddialog_);
  EXPECT_TRUE(handler->got_onresetdialogstate_);
  EXPECT_TRUE(handler->got_onloadend_);

  ReleaseAndWaitForDestructor(handler);
}

// Prompt dialog run immediately return Cancel.
TEST(JSDialogTest, PromptRunImmediateCancel) {
  CefRefPtr<JSDialogTestHandler> handler = new JSDialogTestHandler(
      JSDialogTestHandler::TYPE_PROMPT, JSDialogTestHandler::MODE_RUN_IMMEDIATE,
      false,         // success
      "some_value",  // user_input
      "null");       // result
  handler->ExecuteTest();

  EXPECT_TRUE(handler->got_onjsdialog_);
  EXPECT_FALSE(handler->got_onbeforeunloaddialog_);
  EXPECT_TRUE(handler->got_onresetdialogstate_);
  EXPECT_TRUE(handler->got_onloadend_);

  ReleaseAndWaitForDestructor(handler);
}

// Prompt dialog run delayed return OK.
TEST(JSDialogTest, PromptRunDelayedOk) {
  CefRefPtr<JSDialogTestHandler> handler = new JSDialogTestHandler(
      JSDialogTestHandler::TYPE_PROMPT, JSDialogTestHandler::MODE_RUN_DELAYED,
      true,           // success
      "some_value",   // user_input
      "some_value");  // result
  handler->ExecuteTest();

  EXPECT_TRUE(handler->got_onjsdialog_);
  EXPECT_FALSE(handler->got_onbeforeunloaddialog_);
  EXPECT_TRUE(handler->got_onresetdialogstate_);
  EXPECT_TRUE(handler->got_onloadend_);

  ReleaseAndWaitForDestructor(handler);
}

// Prompt dialog run delayed return Cancel.
TEST(JSDialogTest, PromptRunDelayedCancel) {
  CefRefPtr<JSDialogTestHandler> handler = new JSDialogTestHandler(
      JSDialogTestHandler::TYPE_PROMPT, JSDialogTestHandler::MODE_RUN_DELAYED,
      false,         // success
      "some_value",  // user_input
      "null");       // result
  handler->ExecuteTest();

  EXPECT_TRUE(handler->got_onjsdialog_);
  EXPECT_FALSE(handler->got_onbeforeunloaddialog_);
  EXPECT_TRUE(handler->got_onresetdialogstate_);
  EXPECT_TRUE(handler->got_onloadend_);

  ReleaseAndWaitForDestructor(handler);
}

// OnBeforeUnload dialog with immediate callback.
TEST(JSDialogTest, OnBeforeUnloadRunImmediate) {
  CefRefPtr<JSDialogTestHandler> handler =
      new JSDialogTestHandler(JSDialogTestHandler::TYPE_ONBEFOREUNLOAD,
                              JSDialogTestHandler::MODE_RUN_IMMEDIATE,
                              true,  // success
                              "",    // user_input
                              "");   // result
  handler->ExecuteTest();

  EXPECT_FALSE(handler->got_onjsdialog_);
  EXPECT_TRUE(handler->got_onbeforeunloaddialog_);
  EXPECT_TRUE(handler->got_onresetdialogstate_);
  EXPECT_TRUE(handler->got_onloadend_);

  ReleaseAndWaitForDestructor(handler);
}

// OnBeforeUnload dialog with delayed callback.
TEST(JSDialogTest, OnBeforeUnloadRunDelayed) {
  CefRefPtr<JSDialogTestHandler> handler =
      new JSDialogTestHandler(JSDialogTestHandler::TYPE_ONBEFOREUNLOAD,
                              JSDialogTestHandler::MODE_RUN_DELAYED,
                              true,  // success
                              "",    // user_input
                              "");   // result
  handler->ExecuteTest();

  EXPECT_FALSE(handler->got_onjsdialog_);
  EXPECT_TRUE(handler->got_onbeforeunloaddialog_);
  EXPECT_TRUE(handler->got_onresetdialogstate_);
  EXPECT_TRUE(handler->got_onloadend_);

  ReleaseAndWaitForDestructor(handler);
}
