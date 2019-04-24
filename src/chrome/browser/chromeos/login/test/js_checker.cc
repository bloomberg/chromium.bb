// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/test/js_checker.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/login/test/test_predicate_waiter.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

std::string WrapSend(const std::string& expression) {
  return "window.domAutomationController.send(" + expression + ")";
}

bool CheckConditionIfOobeExists(const std::string& js_condition) {
  return !chromeos::LoginDisplayHost::default_host() ||
         chromeos::test::OobeJS().GetBool(js_condition);
}

}  // namespace

namespace chromeos {
namespace test {

JSChecker::JSChecker() = default;

JSChecker::JSChecker(content::WebContents* web_contents)
    : web_contents_(web_contents) {}

void JSChecker::Evaluate(const std::string& expression) {
  CHECK(web_contents_);
  ASSERT_TRUE(content::ExecuteScript(web_contents_, expression));
}

void JSChecker::ExecuteAsync(const std::string& expression) {
  CHECK(web_contents_);
  std::string new_script = expression + ";";
  web_contents_->GetMainFrame()->ExecuteJavaScriptWithUserGestureForTests(
      base::UTF8ToUTF16(new_script));
}

bool JSChecker::GetBool(const std::string& expression) {
  bool result;
  GetBoolImpl(expression, &result);
  return result;
}

int JSChecker::GetInt(const std::string& expression) {
  int result;
  GetIntImpl(expression, &result);
  return result;
}

std::string JSChecker::GetString(const std::string& expression) {
  std::string result;
  GetStringImpl(expression, &result);
  return result;
}

void JSChecker::ExpectTrue(const std::string& expression) {
  EXPECT_TRUE(GetBool(expression)) << expression;
}

void JSChecker::ExpectFalse(const std::string& expression) {
  EXPECT_FALSE(GetBool(expression)) << expression;
}

void JSChecker::ExpectEQ(const std::string& expression, int result) {
  EXPECT_EQ(GetInt(expression), result) << expression;
}

void JSChecker::ExpectNE(const std::string& expression, int result) {
  EXPECT_NE(GetInt(expression), result) << expression;
}

void JSChecker::ExpectEQ(const std::string& expression,
                         const std::string& result) {
  EXPECT_EQ(GetString(expression), result) << expression;
}

void JSChecker::ExpectNE(const std::string& expression,
                         const std::string& result) {
  EXPECT_NE(GetString(expression), result) << expression;
}

void JSChecker::ExpectEQ(const std::string& expression, bool result) {
  EXPECT_EQ(GetBool(expression), result) << expression;
}

void JSChecker::ExpectNE(const std::string& expression, bool result) {
  EXPECT_NE(GetBool(expression), result) << expression;
}

std::unique_ptr<TestConditionWaiter> JSChecker::CreateWaiter(
    const std::string& js_condition) {
  TestPredicateWaiter::PredicateCheck predicate = base::BindRepeating(
      &JSChecker::GetBool, base::Unretained(this), js_condition);
  return std::make_unique<TestPredicateWaiter>(predicate);
}

void JSChecker::GetBoolImpl(const std::string& expression, bool* result) {
  CHECK(web_contents_);
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents_, WrapSend("!!(" + expression + ")"), result));
}

void JSChecker::GetIntImpl(const std::string& expression, int* result) {
  CHECK(web_contents_);
  ASSERT_TRUE(content::ExecuteScriptAndExtractInt(
      web_contents_, WrapSend(expression), result));
}

void JSChecker::GetStringImpl(const std::string& expression,
                              std::string* result) {
  CHECK(web_contents_);
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents_, WrapSend(expression), result));
}

JSChecker OobeJS() {
  return JSChecker(LoginDisplayHost::default_host()->GetOobeWebContents());
}

void ExecuteOobeJS(const std::string& script) {
  ASSERT_TRUE(content::ExecuteScript(
      LoginDisplayHost::default_host()->GetOobeWebContents(), script));
}

void ExecuteOobeJSAsync(const std::string& script) {
  content::ExecuteScriptAsync(
      LoginDisplayHost::default_host()->GetOobeWebContents(), script);
}

std::unique_ptr<TestConditionWaiter> CreatePredicateOrOobeDestroyedWaiter(
    const std::string& js_condition) {
  TestPredicateWaiter::PredicateCheck predicate =
      base::BindRepeating(&CheckConditionIfOobeExists, js_condition);
  return std::make_unique<TestPredicateWaiter>(predicate);
}

}  // namespace test
}  // namespace chromeos
