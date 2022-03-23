// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/ui/login_feedback.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/feedback/feedback_dialog.h"
#include "extensions/browser/api/feedback_private/feedback_private_api.h"

namespace ash {

////////////////////////////////////////////////////////////////////////////////
// LoginFeedback

LoginFeedback::LoginFeedback(Profile* signin_profile)
    : profile_(signin_profile) {}

LoginFeedback::~LoginFeedback() {}

void LoginFeedback::Request(const std::string& description) {
  description_ = description;

  extensions::FeedbackPrivateAPI* api =
      extensions::FeedbackPrivateAPI::GetFactoryInstance()->Get(profile_);

  auto info = api->CreateFeedbackInfo(
      description_, std::string(), "Login", std::string(), GURL(),
      extensions::api::feedback_private::FeedbackFlow::FEEDBACK_FLOW_LOGIN,
      /*from_assistant=*/false,
      /*include_bluetooth_logs=*/false,
      /*show_questionnaire=*/false,
      /*from_chrome_labs_or_kaleidoscope=*/false);

  FeedbackDialog::CreateOrShow(profile_, *info);
}

}  // namespace ash
