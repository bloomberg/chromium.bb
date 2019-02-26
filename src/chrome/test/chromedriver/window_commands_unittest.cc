// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/values.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/stub_chrome.h"
#include "chrome/test/chromedriver/chrome/stub_web_view.h"
#include "chrome/test/chromedriver/commands.h"
#include "chrome/test/chromedriver/net/timeout.h"
#include "chrome/test/chromedriver/session.h"
#include "chrome/test/chromedriver/window_commands.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockChrome : public StubChrome {
 public:
  MockChrome() : web_view_("1") {}
  ~MockChrome() override {}

  Status GetWebViewById(const std::string& id, WebView** web_view) override {
    if (id == web_view_.GetId()) {
      *web_view = &web_view_;
      return Status(kOk);
    }
    return Status(kUnknownError);
  }

 private:
  // Using a StubWebView does not allow testing the functionality end-to-end,
  // more details in crbug.com/850703
  StubWebView web_view_;
};

}  // namespace

TEST(WindowCommandsTest, ExecuteFreeze) {
  MockChrome* chrome = new MockChrome();
  Session session("id", std::unique_ptr<Chrome>(chrome));
  base::DictionaryValue params;
  std::unique_ptr<base::Value> value;
  Timeout timeout;

  WebView* web_view = NULL;
  Status status = chrome->GetWebViewById("1", &web_view);
  ASSERT_EQ(kOk, status.code());
  status = ExecuteFreeze(&session, web_view, params, &value, &timeout);
}

TEST(WindowCommandsTest, ExecuteResume) {
  MockChrome* chrome = new MockChrome();
  Session session("id", std::unique_ptr<Chrome>(chrome));
  base::DictionaryValue params;
  std::unique_ptr<base::Value> value;
  Timeout timeout;

  WebView* web_view = NULL;
  Status status = chrome->GetWebViewById("1", &web_view);
  ASSERT_EQ(kOk, status.code());
  status = ExecuteResume(&session, web_view, params, &value, &timeout);
}

TEST(WindowCommandsTest, ProcessInputActionSequencePointerMouse) {
  Session session("1");
  std::unique_ptr<base::DictionaryValue> result(new base::DictionaryValue());
  std::unique_ptr<base::DictionaryValue> action_sequence(
      new base::DictionaryValue());
  std::unique_ptr<base::ListValue> actions(new base::ListValue());
  std::unique_ptr<base::DictionaryValue> action(new base::DictionaryValue());
  std::unique_ptr<base::DictionaryValue> parameters(
      new base::DictionaryValue());
  parameters->SetString("pointerType", "mouse");
  action->SetString("type", "pointerMove");
  action->SetInteger("x", 30);
  action->SetInteger("y", 60);
  actions->Append(std::move(action));
  action = std::make_unique<base::DictionaryValue>();
  action->SetString("type", "pointerDown");
  action->SetInteger("button", 0);
  actions->Append(std::move(action));
  action = std::make_unique<base::DictionaryValue>();
  action->SetString("type", "pointerUp");
  action->SetInteger("button", 0);
  actions->Append(std::move(action));

  // pointer properties
  action_sequence->SetString("type", "pointer");
  action_sequence->SetString("id", "pointer1");
  action_sequence->SetDictionary("parameters", std::move(parameters));
  action_sequence->SetList("actions", std::move(actions));
  const base::DictionaryValue* input_action_sequence = action_sequence.get();
  Status status =
      ProcessInputActionSequence(&session, input_action_sequence, &result);
  ASSERT_TRUE(status.IsOk());

  // check resulting action dictionary
  const base::ListValue* actions_result;
  const base::DictionaryValue* action_result;
  std::string pointer_type;
  std::string source_type;
  std::string id;
  std::string action_type;
  int x, y;
  std::string button;

  result->GetString("sourceType", &source_type);
  result->GetString("pointerType", &pointer_type);
  result->GetString("id", &id);
  ASSERT_EQ("pointer", source_type);
  ASSERT_EQ("mouse", pointer_type);
  ASSERT_EQ("pointer1", id);

  result->GetList("actions", &actions_result);
  ASSERT_EQ(3U, actions_result->GetSize());
  actions_result->GetDictionary(0, &action_result);
  action_result->GetString("type", &action_type);
  action_result->GetInteger("x", &x);
  action_result->GetInteger("y", &y);
  ASSERT_EQ("pointerMove", action_type);
  ASSERT_EQ(30, x);
  ASSERT_EQ(60, y);
  actions_result->GetDictionary(1, &action_result);
  action_result->GetString("type", &action_type);
  action_result->GetString("button", &button);
  ASSERT_EQ("pointerDown", action_type);
  ASSERT_EQ("left", button);
  actions_result->GetDictionary(2, &action_result);
  action_result->GetString("type", &action_type);
  action_result->GetString("button", &button);
  ASSERT_EQ("pointerUp", action_type);
  ASSERT_EQ("left", button);
}
