// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_answers/quick_answers_controller_impl.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/quick_answers/quick_answers_state.h"
#include "ash/quick_answers/quick_answers_ui_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/metrics/histogram_functions.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_prefs.h"
#include "chromeos/components/quick_answers/quick_answers_notice.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace {
using chromeos::quick_answers::Context;
using chromeos::quick_answers::IntentType;
using chromeos::quick_answers::QuickAnswer;
using chromeos::quick_answers::QuickAnswersClient;
using chromeos::quick_answers::QuickAnswersExitPoint;
using chromeos::quick_answers::QuickAnswersRequest;
using chromeos::quick_answers::ResultType;

constexpr char kAssistantRelatedInfoUrl[] =
    "chrome://os-settings/googleAssistant";
constexpr char kQuickAnswersSettingsUrl[] =
    "chrome://os-settings/osSearch/search";
constexpr char kDogfoodUrl[] =
    "https://goto.google.com/quick-answers-dogfood-bugs";

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
          chromeos::quick_answers::IntentType::kUnknown);
}

bool ShouldShowQuickAnswers() {
  if (!ash::QuickAnswersState::Get()->is_eligible())
    return false;

  bool should_show_consent =
      ash::QuickAnswersState::Get()->consent_status() ==
      chromeos::quick_answers::prefs::ConsentStatus::kUnknown;
  bool settings_enabled = ash::QuickAnswersState::Get()->settings_enabled();

  return should_show_consent || settings_enabled;
}

}  // namespace

namespace ash {

QuickAnswersControllerImpl::QuickAnswersControllerImpl()
    : quick_answers_ui_controller_(
          std::make_unique<QuickAnswersUiController>(this)) {}

QuickAnswersControllerImpl::~QuickAnswersControllerImpl() = default;

void QuickAnswersControllerImpl::SetClient(
    std::unique_ptr<QuickAnswersClient> client) {
  quick_answers_client_ = std::move(client);
  notice_controller_ =
      std::make_unique<chromeos::quick_answers::QuickAnswersNotice>(
          Shell::Get()->session_controller()->GetPrimaryUserPrefService());
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
  if (chromeos::features::ShouldUseQuickAnswersTextAnnotator()) {
    // Send the request for preprocessing. Only shows quick answers view if the
    // predicted intent is not |kUnknown| at |OnRequestPreprocessFinish|.
    quick_answers_client_->SendRequestForPreprocessing(request);
  } else {
    HandleQuickAnswerRequest(request);
  }
}

void QuickAnswersControllerImpl::HandleQuickAnswerRequest(
    const chromeos::quick_answers::QuickAnswersRequest& request) {
  if (chromeos::features::IsQuickAnswersV2Enabled() &&
      QuickAnswersState::Get()->consent_status() ==
          chromeos::quick_answers::prefs::ConsentStatus::kUnknown) {
    ShowUserConsent(
        IntentTypeToString(request.preprocessed_output.intent_info.intent_type),
        base::UTF8ToUTF16(request.preprocessed_output.intent_info.intent_text));
  } else if (!chromeos::features::IsQuickAnswersV2Enabled() &&
             ShouldShowUserNotice()) {
    ShowUserNotice(
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
  MaybeDismissQuickAnswersNotice();
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

chromeos::quick_answers::QuickAnswersDelegate*
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
          std::make_unique<chromeos::quick_answers::QuickAnswerText>(title_));
    }
    quick_answers_ui_controller_->RenderQuickAnswersViewWithResult(
        anchor_bounds_, *quick_answer);
  } else {
    chromeos::quick_answers::QuickAnswer quick_answer_with_no_result;
    quick_answer_with_no_result.title.push_back(
        std::make_unique<chromeos::quick_answers::QuickAnswerText>(title_));
    quick_answer_with_no_result.first_answer_row.push_back(
        std::make_unique<chromeos::quick_answers::QuickAnswerResultText>(
            l10n_util::GetStringUTF8(
                chromeos::features::IsQuickAnswersV2Enabled()
                    ? IDS_ASH_QUICK_ANSWERS_VIEW_NO_RESULT_V2
                    : IDS_ASH_QUICK_ANSWERS_VIEW_NO_RESULT)));
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
  if (!chromeos::features::ShouldUseQuickAnswersTextAnnotator()) {
    // Ignore preprocessing result if text annotator is not enabled.
    return;
  }

  auto intent_type =
      processed_request.preprocessed_output.intent_info.intent_type;

  if (intent_type == chromeos::quick_answers::IntentType::kUnknown) {
    return;
  }

  if (visibility_ == QuickAnswersVisibility::kClosed)
    return;

  query_ = processed_request.preprocessed_output.query;
  title_ = processed_request.preprocessed_output.intent_info.intent_text;

  HandleQuickAnswerRequest(processed_request);
}

void QuickAnswersControllerImpl::OnRetryQuickAnswersRequest() {
  QuickAnswersRequest request = BuildRequest();
  if (chromeos::features::ShouldUseQuickAnswersTextAnnotator()) {
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

void QuickAnswersControllerImpl::OnUserNoticeAccepted() {
  quick_answers_ui_controller_->CloseUserNoticeView();
  notice_controller_->AcceptNotice(
      chromeos::quick_answers::NoticeInteractionType::kAccept);

  // Display Quick-Answer for the cached query when user dismisses the
  // notice.
  MaybeShowQuickAnswers(anchor_bounds_, title_, context_);
}

void QuickAnswersControllerImpl::OnNoticeSettingsRequestedByUser() {
  quick_answers_ui_controller_->CloseUserNoticeView();
  notice_controller_->AcceptNotice(
      chromeos::quick_answers::NoticeInteractionType::kManageSettings);
  NewWindowDelegate::GetInstance()->NewTabWithUrl(
      GURL(kAssistantRelatedInfoUrl), /*from_user_interaction=*/true);
}

void QuickAnswersControllerImpl::OnUserConsentResult(bool consented) {
  quick_answers_ui_controller_->CloseUserConsentView();

  ash::QuickAnswersState::Get()->OnConsentResult(
      consented ? ConsentResultType::kAllow : ConsentResultType::kNoThanks);

  if (consented) {
    // Display Quick-Answer for the cached query when user consent has
    // been granted.
    MaybeShowQuickAnswers(anchor_bounds_, title_, context_);
  }
}

void QuickAnswersControllerImpl::OpenQuickAnswersDogfoodLink() {
  NewWindowDelegate::GetInstance()->NewTabWithUrl(
      GURL(kDogfoodUrl), /*from_user_interaction=*/true);
}

void QuickAnswersControllerImpl::OpenQuickAnswersSettings() {
  NewWindowDelegate::GetInstance()->NewTabWithUrl(
      GURL(kQuickAnswersSettingsUrl), /*from_user_interaction=*/true);
}

void QuickAnswersControllerImpl::MaybeDismissQuickAnswersNotice() {
  if (quick_answers_ui_controller_->is_showing_user_notice_view())
    notice_controller_->DismissNotice();
  quick_answers_ui_controller_->CloseUserNoticeView();
}

void QuickAnswersControllerImpl::MaybeDismissQuickAnswersConsent() {
  if (quick_answers_ui_controller_->is_showing_user_consent_view())
    ash::QuickAnswersState::Get()->OnConsentResult(ConsentResultType::kDismiss);
  quick_answers_ui_controller_->CloseUserConsentView();
}

bool QuickAnswersControllerImpl::ShouldShowUserNotice() const {
  return notice_controller_->ShouldShowNotice();
}

void QuickAnswersControllerImpl::ShowUserNotice(
    const std::u16string& intent_type,
    const std::u16string& intent_text) {
  // Show notice informing user about the feature if required.
  if (!quick_answers_ui_controller_->is_showing_user_notice_view()) {
    quick_answers_ui_controller_->CreateUserNoticeView(
        anchor_bounds_, intent_type, intent_text);
    notice_controller_->StartNotice();
  }
}

void QuickAnswersControllerImpl::ShowUserConsent(
    const std::u16string& intent_type,
    const std::u16string& intent_text) {
  // Show notice informing user about the feature if required.
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
}  // namespace ash
