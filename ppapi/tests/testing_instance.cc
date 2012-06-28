// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/testing_instance.h"

#include <algorithm>
#include <cstring>
#include <sstream>
#include <vector>

#include "ppapi/cpp/module.h"
#include "ppapi/cpp/var.h"
#include "ppapi/cpp/view.h"
#include "ppapi/tests/test_case.h"

TestCaseFactory* TestCaseFactory::head_ = NULL;

// Cookie value we use to signal "we're still working." See the comment above
// the class declaration for how this works.
static const char kProgressSignal[] = "...";

// Returns a new heap-allocated test case for the given test, or NULL on
// failure.
TestingInstance::TestingInstance(PP_Instance instance)
#if (defined __native_client__)
    : pp::Instance(instance),
#else
    : pp::InstancePrivate(instance),
#endif
      current_case_(NULL),
      executed_tests_(false),
      number_tests_executed_(0),
      nacl_mode_(false),
      ssl_server_port_(-1),
      websocket_port_(-1),
      remove_plugin_(true) {
  callback_factory_.Initialize(this);
}

TestingInstance::~TestingInstance() {
  if (current_case_)
    delete current_case_;
}

bool TestingInstance::Init(uint32_t argc,
                           const char* argn[],
                           const char* argv[]) {
  for (uint32_t i = 0; i < argc; i++) {
    if (std::strcmp(argn[i], "mode") == 0) {
      if (std::strcmp(argv[i], "nacl") == 0)
        nacl_mode_ = true;
    } else if (std::strcmp(argn[i], "protocol") == 0) {
      protocol_ = argv[i];
    } else if (std::strcmp(argn[i], "websocket_port") == 0) {
      websocket_port_ = atoi(argv[i]);
    } else if (std::strcmp(argn[i], "ssl_server_port") == 0) {
      ssl_server_port_ = atoi(argv[i]);
    }
  }
  // Create the proper test case from the argument.
  for (uint32_t i = 0; i < argc; i++) {
    if (std::strcmp(argn[i], "testcase") == 0) {
      if (argv[i][0] == '\0')
        break;
      current_case_ = CaseForTestName(argv[i]);
      test_filter_ = FilterForTestName(argv[i]);
      if (!current_case_)
        errors_.append(std::string("Unknown test case ") + argv[i]);
      else if (!current_case_->Init())
        errors_.append(" Test case could not initialize.");
      return true;
    }
  }

  // In DidChangeView, we'll dump out a list of all available tests.
  return true;
}

#if !(defined __native_client__)
pp::Var TestingInstance::GetInstanceObject() {
  if (current_case_)
    return current_case_->GetTestObject();

  return pp::VarPrivate();
}
#endif

void TestingInstance::HandleMessage(const pp::Var& message_data) {
  if (current_case_)
    current_case_->HandleMessage(message_data);
}

void TestingInstance::DidChangeView(const pp::View& view) {
  if (!executed_tests_) {
    executed_tests_ = true;
    pp::Module::Get()->core()->CallOnMainThread(
        0,
        callback_factory_.NewCallback(&TestingInstance::ExecuteTests));
  }
  if (current_case_)
    current_case_->DidChangeView(view);
}

bool TestingInstance::HandleInputEvent(const pp::InputEvent& event) {
  if (current_case_)
    return current_case_->HandleInputEvent(event);
  return false;
}

void TestingInstance::EvalScript(const std::string& script) {
  SendTestCommand("EvalScript", script);
}

void TestingInstance::SetCookie(const std::string& name,
                                const std::string& value) {
  SendTestCommand("SetCookie", name + "=" + value);
}

void TestingInstance::LogTest(const std::string& test_name,
                              const std::string& error_message) {
  // Tell the browser we're still working.
  ReportProgress(kProgressSignal);

  number_tests_executed_++;

  std::string html;
  html.append("<div class=\"test_line\"><span class=\"test_name\">");
  html.append(test_name);
  html.append("</span> ");
  if (error_message.empty()) {
    html.append("<span class=\"pass\">PASS</span>");
  } else {
    html.append("<span class=\"fail\">FAIL</span>: <span class=\"err_msg\">");
    html.append(error_message);
    html.append("</span>");

    if (!errors_.empty())
      errors_.append(", ");  // Separator for different error messages.
    errors_.append(test_name + " FAIL: " + error_message);
  }
  html.append("</div>");
  LogHTML(html);
}

void TestingInstance::AppendError(const std::string& message) {
  if (!errors_.empty())
    errors_.append(", ");
  errors_.append(message);
}

void TestingInstance::ExecuteTests(int32_t unused) {
  ReportProgress(kProgressSignal);

  // Clear the console.
  SendTestCommand("ClearConsole");

  if (!errors_.empty()) {
    // Catch initialization errors and output the current error string to
    // the console.
    LogError("Plugin initialization failed: " + errors_);
  } else if (!current_case_) {
    LogAvailableTests();
    errors_.append("FAIL: Only listed tests");
  } else {
    current_case_->RunTests(test_filter_);

    if (number_tests_executed_ == 0) {
      errors_.append("No tests executed. The test filter might be too "
                     "restrictive: '" + test_filter_ + "'.");
      LogError(errors_);
    }
    else {
      // Automated PyAuto tests rely on finding the exact strings below.
      LogHTML(errors_.empty() ?
              "<span class=\"pass\">[SHUTDOWN]</span> All tests passed." :
              "<span class=\"fail\">[SHUTDOWN]</span> Some tests failed.");
    }
  }

  // Declare we're done by setting a cookie to either "PASS" or the errors.
  ReportProgress(errors_.empty() ? "PASS" : errors_);
  if (remove_plugin_)
    SendTestCommand("DidExecuteTests");
  // Note, DidExecuteTests unloads the plugin. We can't really do anthing after
  // this point.
}

TestCase* TestingInstance::CaseForTestName(const std::string& name) {
  std::string case_name = name.substr(0, name.find_first_of('_'));
  TestCaseFactory* iter = TestCaseFactory::head_;
  while (iter != NULL) {
    if (case_name == iter->name_)
      return iter->method_(this);
    iter = iter->next_;
  }
  return NULL;
}

std::string TestingInstance::FilterForTestName(const std::string& name) {
  size_t delim = name.find_first_of('_');
  if (delim != std::string::npos)
    return name.substr(delim+1);
  return "";
}

void TestingInstance::SendTestCommand(const std::string& command) {
  std::string msg("TESTING_MESSAGE:");
  msg += command;
  PostMessage(pp::Var(msg));
}

void TestingInstance::SendTestCommand(const std::string& command,
                                      const std::string& params) {
  SendTestCommand(command + ":" + params);
}


void TestingInstance::LogAvailableTests() {
  // Print out a listing of all tests.
  std::vector<std::string> test_cases;
  TestCaseFactory* iter = TestCaseFactory::head_;
  while (iter != NULL) {
    test_cases.push_back(iter->name_);
    iter = iter->next_;
  }
  std::sort(test_cases.begin(), test_cases.end());

  std::string html;
  html.append("Available test cases: <dl>");
  for (size_t i = 0; i < test_cases.size(); ++i) {
    html.append("<dd><a href='?testcase=");
    html.append(test_cases[i]);
    if (nacl_mode_)
       html.append("&mode=nacl");
    html.append("'>");
    html.append(test_cases[i]);
    html.append("</a></dd>");
  }
  html.append("</dl>");
  html.append("<button onclick='RunAll()'>Run All Tests</button>");

  LogHTML(html);
}

void TestingInstance::LogError(const std::string& text) {
  std::string html;
  html.append("<span class=\"fail\">FAIL</span>: <span class=\"err_msg\">");
  html.append(text);
  html.append("</span>");
  LogHTML(html);
}

void TestingInstance::LogHTML(const std::string& html) {
  SendTestCommand("LogHTML", html);
}

void TestingInstance::ReportProgress(const std::string& progress_value) {
  // Use streams since nacl doesn't compile base yet (for StringPrintf).
  std::ostringstream script;
  script << "window.domAutomationController.setAutomationId(0);" <<
            "window.domAutomationController.send(\"" << progress_value << "\")";
  EvalScript(script.str());
}

void TestingInstance::AddPostCondition(const std::string& script) {
  SendTestCommand("AddPostCondition", script);
}

class Module : public pp::Module {
 public:
  Module() : pp::Module() {}
  virtual ~Module() {}

  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new TestingInstance(instance);
  }
};

namespace pp {

Module* CreateModule() {
  return new ::Module();
}

}  // namespace pp
