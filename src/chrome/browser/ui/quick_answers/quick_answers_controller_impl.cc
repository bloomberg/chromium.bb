// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/quick_answers/quick_answers_controller_impl.h"

#include "ash/public/cpp/new_window_delegate.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ui/quick_answers/quick_answers_ui_controller.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_prefs.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_state.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace {

using ::quick_answers::Context;
using ::quick_answers::IntentType;
using ::quick_answers::QuickAnswer;
using ::quick_answers::QuickAnswersClient;
using ::quick_answers::QuickAnswersExitPoint;
using ::quick_answers::QuickAnswersRequest;
using ::quick_answers::ResultType;

constexpr char kQuickAnswersSettingsUrl[] =
    "chrome://os-settings/osSearch/search";

constexpr char kQuickAnswersExitPoint[] = "QuickAnswers.ExitPoint";

std::u16string IntentTypeToString(IntentType intent_type) {
  switch (intent_type) {
    case IntentType::kUnit:
      return l10n_util::GetStringUTF16(
          IDS_ASH_QUICK_ANSWERS_UNIT_CONVERSION_INTENT);
    case IntentType::kDictionary:
      return l10n_util::GetStringUTF16(IDS_ASH_QUICK_ANSWERS_DEFINITION_INTENT);
    case IntentType::kTranslation:
      return l10n_util::GetStringUTF16(
          IDS_ASH_QUICK_ANSWERS_TRANSLATION_INTENT);
    case IntentType::kUnknown:
      return std::u16string();
  }
}

// Returns if the request has already been processed (by the text annotator).
bool IsProcessedRequest(const QuickAnswersRequest& request) {
  return (request.preprocessed_output.intent_info.intent_type !=
          quick_answers::IntentType::kUnknown);
}

bool ShouldShowQuickAnswers() {
  if (!QuickAnswersState::Get()->is_eligible())
    return false;

  bool settings_enabled = QuickAnswersState::Get()->settings_enabled();
  // Respect the managed settings.
  if (QuickAnswersState::Get()->IsSettingsEnforced())
    return settings_enabled;

  if (settings_enabled)
    return true;

  bool should_show_consent = QuickAnswersState::Get()->consent_status() ==
                             quick_answers::prefs::ConsentStatus::kUnknown;
  return should_show_consent;
}

}  // namespace

QuickAnswersControllerImpl::QuickAnswersControllerImpl()
    : quick_answers_ui_controller_(
          std::make_unique<QuickAnswersUiController>(this)),
      quick_answers_access_token_fetcher_(
          std::make_unique<QuickAnswersAccessTokenFetcher>()) {}

QuickAnswersControllerImpl::~QuickAnswersControllerImpl() = default;

void QuickAnswersControllerImpl::SetClient(
    std::unique_ptr<QuickAnswersClient> client) {
  quick_answers_client_ = std::move(client);
}

void QuickAnswersControllerImpl::MaybeShowQuickAnswers(
    const gfx::Rect& anchor_bounds,
    const std::string& title,
    const Context& context) {
  if (!ShouldShowQuickAnswers())
    return;

  if (visibility_ == QuickAnswersVisibility::kClosed)
    return;

  // Cache anchor-bounds and query.
  anchor_bounds_ = anchor_bounds;
  // Initially, title is same as query. Title and query can be overridden based
  // on text annotation result at |OnRequestPreprocessFinish|.
  title_ = title;
  query_ = title;
  context_ = context;
  quick_answer_.reset();

  QuickAnswersRequest request = BuildRequest();
  if (QuickAnswersState::Get()->ShouldUseQuickAnswersTextAnnotator()) {
    // Send the request for preprocessing. Only shows quick answers view if the
    // predicted intent is not |kUnknown| at |OnRequestPreprocessFinish|.
    quick_answers_client_->SendRequestForPreprocessing(request);
  } else {
    HandleQuickAnswerRequest(request);
  }
}

void QuickAnswersControllerImpl::HandleQuickAnswerRequest(
    const quick_answers::QuickAnswersRequest& request) {
  if (QuickAnswersState::Get()->consent_status() ==
      quick_answers::prefs::ConsentStatus::kUnknown) {
    ShowUserConsent(
        IntentTypeToString(request.preprocessed_output.intent_info.intent_type),
        base::UTF8ToUTF16(request.preprocessed_output.intent_info.intent_text));
  } else {
    visibility_ = QuickAnswersVisibility::kVisible;
    quick_answers_ui_controller_->CreateQuickAnswersView(
        anchor_bounds_, title_, query_,
        request.context.device_properties.is_internal);

    if (IsProcessedRequest(request))
      quick_answers_client_->FetchQuickAnswers(request);
    else
      quick_answers_client_->SendRequest(request);
  }
}

void QuickAnswersControllerImpl::DismissQuickAnswers(
    QuickAnswersExitPoint exit_point) {
  visibility_ = QuickAnswersVisibility::kClosed;
  MaybeDismissQuickAnswersConsent();
  bool closed = quick_answers_ui_controller_->CloseQuickAnswersView();
  // |quick_answer_| could be null before we receive the result from the server.
  // Do not send the signal since the quick answer is dismissed before ready.
  if (quick_answer_) {
    // For quick-answer rendered along with browser context menu, if user didn't
    // click on other context menu items, it is considered as active impression.
    bool is_active = exit_point != QuickAnswersExitPoint::kContextMenuClick;
    quick_answers_client_->OnQuickAnswersDismissed(quick_answer_->result_type,
                                                   is_active && closed);

    // Record Quick Answers exit point.
    // Make sure |closed| is true so that only the direct exit point is recorded
    // when multiple dissmiss requests are received (For example, dissmiss
    // request from context menu will also fire when the settings button is
    // pressed).
    if (closed)
      base::UmaHistogramEnumeration(kQuickAnswersExitPoint, exit_point);
  }
}

quick_answers::QuickAnswersDelegate*
QuickAnswersControllerImpl::GetQuickAnswersDelegate() {
  return this;
}

QuickAnswersVisibility QuickAnswersControllerImpl::GetVisibilityForTesting()
    const {
  return visibility_;
}

void QuickAnswersControllerImpl::OnQuickAnswerReceived(
    std::unique_ptr<QuickAnswer> quick_answer) {
  if (visibility_ != QuickAnswersVisibility::kVisible)
    return;

  if (quick_answer) {
    if (quick_answer->title.empty()) {
      quick_answer->title.push_back(
          std::make_unique<quick_answers::QuickAnswerText>(title_));
    }
    quick_answers_ui_controller_->RenderQuickAnswersViewWithResult(
        anchor_bounds_, *quick_answer);
  } else {
    quick_answers::QuickAnswer quick_answer_with_no_result;
    quick_answer_with_no_result.title.push_back(
        std::make_unique<quick_answers::QuickAnswerText>(title_));
    quick_answer_with_no_result.first_answer_row.push_back(
        std::make_unique<quick_answers::QuickAnswerResultText>(
            l10n_util::GetStringUTF8(IDS_ASH_QUICK_ANSWERS_VIEW_NO_RESULT_V2)));
    quick_answers_ui_controller_->RenderQuickAnswersViewWithResult(
        anchor_bounds_, quick_answer_with_no_result);
    // Fallback query to title if no result is available.
    query_ = title_;
    quick_answers_ui_controller_->SetActiveQuery(query_);
  }

  quick_answer_ = std::move(quick_answer);
}

void QuickAnswersControllerImpl::OnNetworkError() {
  if (visibility_ != QuickAnswersVisibility::kVisible)
    return;

  // Notify quick_answers_ui_controller_ to show retry UI.
  quick_answers_ui_controller_->ShowRetry();
}

void QuickAnswersControllerImpl::OnRequestPreprocessFinished(
    const QuickAnswersRequest& processed_request) {
  if (!QuickAnswersState::Get()->ShouldUseQuickAnswersTextAnnotator()) {
    // Ignore preprocessing result if text annotator is not enabled.
    return;
  }

  auto intent_type =
      processed_request.preprocessed_output.intent_info.intent_type;

  if (intent_type == quick_answers::IntentType::kUnknown) {
    return;
  }

  if (visibility_ == QuickAnswersVisibility::kClosed)
    return;

  query_ = processed_request.preprocessed_output.query;
  title_ = processed_request.preprocessed_output.intent_info.intent_text;

  HandleQuickAnswerRequest(processed_request);
}

void QuickAnswersControllerImpl::RequestAccessToken(
    AccessTokenCallback callback) {
  quick_answers_access_token_fetcher_->RequestAccessToken(std::move(callback));
}

void QuickAnswersControllerImpl::OnRetryQuickAnswersRequest() {
  QuickAnswersRequest request = BuildRequest();
  if (QuickAnswersState::Get()->ShouldUseQuickAnswersTextAnnotator()) {
    quick_answers_client_->SendRequestForPreprocessing(request);
  } else {
    quick_answers_client_->SendRequest(request);
  }
}

void QuickAnswersControllerImpl::OnQuickAnswerClick() {
  quick_answers_client_->OnQuickAnswerClick(
      quick_answer_ ? quick_answer_->result_type : ResultType::kNoResult);
}

void QuickAnswersControllerImpl::UpdateQuickAnswersAnchorBounds(
    const gfx::Rect& anchor_bounds) {
  anchor_bounds_ = anchor_bounds;
  quick_answers_ui_controller_->UpdateQuickAnswersBounds(anchor_bounds);
}

void QuickAnswersControllerImpl::SetPendingShowQuickAnswers() {
  visibility_ = QuickAnswersVisibility::kPending;
}

void QuickAnswersControllerImpl::OnUserConsentResult(bool consented) {
  quick_answers_ui_controller_->CloseUserConsentView();

  QuickAnswersState::Get()->OnConsentResult(
      consented ? ConsentResultType::kAllow : ConsentResultType::kNoThanks);

  if (consented) {
    // Display Quick-Answer for the cached query when user consent has
    // been granted.
    MaybeShowQuickAnswers(anchor_bounds_, title_, context_);
  }
}

void QuickAnswersControllerImpl::OpenQuickAnswersSettings() {
  ash::NewWindowDelegate::GetInstance()->OpenUrl(
      GURL(kQuickAnswersSettingsUrl),
      /*from_user_interaction=*/true);
}

void QuickAnswersControllerImpl::MaybeDismissQuickAnswersConsent() {
  if (quick_answers_ui_controller_->is_showing_user_consent_view())
    QuickAnswersState::Get()->OnConsentResult(ConsentResultType::kDismiss);
  quick_answers_ui_controller_->CloseUserConsentView();
}

void QuickAnswersControllerImpl::ShowUserConsent(
    const std::u16string& intent_type,
    const std::u16string& intent_text) {
  // Show consent informing user about the feature if required.
  if (!quick_answers_ui_controller_->is_showing_user_consent_view()) {
    quick_answers_ui_controller_->CreateUserConsentView(
        anchor_bounds_, intent_type, intent_text);
    QuickAnswersState::Get()->StartConsent();
  }
}

QuickAnswersRequest QuickAnswersControllerImpl::BuildRequest() {
  QuickAnswersRequest request;
  request.selected_text = title_;
  request.context = context_;
  return request;
}
