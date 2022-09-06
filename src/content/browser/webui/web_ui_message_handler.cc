// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/web_ui_message_handler.h"

#include "base/check.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

void WebUIMessageHandler::AllowJavascriptForTesting() {
  AllowJavascript();
}

void WebUIMessageHandler::AllowJavascript() {
  if (javascript_allowed_)
    return;

  javascript_allowed_ = true;
  CHECK(IsJavascriptAllowed());

  OnJavascriptAllowed();
}

void WebUIMessageHandler::DisallowJavascript() {
  if (!javascript_allowed_)
    return;

  javascript_allowed_ = false;
  DCHECK(!IsJavascriptAllowed());

  OnJavascriptDisallowed();
}

bool WebUIMessageHandler::IsJavascriptAllowed() {
  return javascript_allowed_ && web_ui() && web_ui()->CanCallJavascript();
}

void WebUIMessageHandler::ResolveJavascriptCallback(
    const base::Value& callback_id,
    const base::Value& response) {
  // cr.webUIResponse is a global JS function exposed from cr.js.
  CallJavascriptFunction("cr.webUIResponse", callback_id, base::Value(true),
                         response);
}

void WebUIMessageHandler::RejectJavascriptCallback(
    const base::Value& callback_id,
    const base::Value& response) {
  // cr.webUIResponse is a global JS function exposed from cr.js.
  CallJavascriptFunction("cr.webUIResponse", callback_id, base::Value(false),
                         response);
}

}  // namespace content
