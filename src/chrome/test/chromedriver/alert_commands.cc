// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/alert_commands.h"

#include "base/callback.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/chrome.h"
#include "chrome/test/chromedriver/chrome/devtools_client.h"
#include "chrome/test/chromedriver/chrome/javascript_dialog_manager.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/web_view.h"
#include "chrome/test/chromedriver/net/timeout.h"
#include "chrome/test/chromedriver/session.h"

Status ExecuteAlertCommand(const AlertCommand& alert_command,
                           Session* session,
                           const base::DictionaryValue& params,
                           std::unique_ptr<base::Value>* value) {
  WebView* web_view = NULL;
  Status status = session->GetTargetWindow(&web_view);
  if (status.IsError())
    return status;

  status = web_view->ConnectIfNecessary();
  if (status.IsError())
    return status;

  status = web_view->HandleReceivedEvents();
  if (status.IsError())
    return status;

  status = web_view->WaitForPendingNavigations(
      session->GetCurrentFrameId(), Timeout(session->page_load_timeout), true);
  if (status.IsError() && status.code() != kUnexpectedAlertOpen)
    return status;

  return alert_command.Run(session, web_view, params, value);
}

Status ExecuteGetAlert(Session* session,
                       WebView* web_view,
                       const base::DictionaryValue& params,
                       std::unique_ptr<base::Value>* value) {
  value->reset(
      new base::Value(web_view->GetJavaScriptDialogManager()->IsDialogOpen()));
  return Status(kOk);
}

Status ExecuteGetAlertText(Session* session,
                           WebView* web_view,
                           const base::DictionaryValue& params,
                           std::unique_ptr<base::Value>* value) {
  std::string message;
  Status status =
      web_view->GetJavaScriptDialogManager()->GetDialogMessage(&message);
  if (status.IsError())
    return status;
  value->reset(new base::Value(message));
  return Status(kOk);
}

Status ExecuteSetAlertText(Session* session,
                           WebView* web_view,
                           const base::DictionaryValue& params,
                           std::unique_ptr<base::Value>* value) {
  std::string text;
  if (!params.GetString("text", &text))
    return Status(kInvalidArgument, "missing or invalid 'text'");

  JavaScriptDialogManager* dialog_manager =
      web_view->GetJavaScriptDialogManager();

  if (!dialog_manager->IsDialogOpen())
    return Status(kNoSuchAlert);

  std::string type;
  Status status = dialog_manager->GetTypeOfDialog(&type);
  if (status.IsError())
    return status;

  if (type == "prompt")
    session->prompt_text.reset(new std::string(text));
  else if (type == "alert" || type == "confirm")
    return Status(kElementNotInteractable,
                  "User dialog does not have a text box input field.");
  else
    return Status(kUnsupportedOperation,
                  "Text can only be sent to window.prompt dialogs.");
  return Status(kOk);
}

Status ExecuteAcceptAlert(Session* session,
                          WebView* web_view,
                          const base::DictionaryValue& params,
                          std::unique_ptr<base::Value>* value) {
  Status status = web_view->GetJavaScriptDialogManager()
      ->HandleDialog(true, session->prompt_text.get());
  session->prompt_text.reset();
  return status;
}

Status ExecuteDismissAlert(Session* session,
                           WebView* web_view,
                           const base::DictionaryValue& params,
                           std::unique_ptr<base::Value>* value) {
  Status status = web_view->GetJavaScriptDialogManager()
      ->HandleDialog(false, session->prompt_text.get());
  session->prompt_text.reset();
  return status;
}
