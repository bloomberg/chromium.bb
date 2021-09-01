// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/credit_card_accessory_controller_impl.h"

#include <algorithm>
#include <iterator>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/cxx20_erase_vector.h"
#include "base/debug/dump_without_crashing.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/android/preferences/autofill/autofill_profile_bridge.h"
#include "chrome/browser/autofill/manual_filling_controller.h"
#include "chrome/browser/autofill/manual_filling_utils.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/vr/vr_tab_helper.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/autofill_browser_util.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

namespace {

// Return the card art url to displayed in the autofill suggestions. The card
// art is only supported for virtual cards. For other cards, we show the default
// network icon.
GURL GetCardArtUrl(const CreditCard& card) {
  return card.record_type() == CreditCard::VIRTUAL_CARD ? card.card_art_url()
                                                        : GURL();
}

std::u16string GetTitle(bool has_suggestions) {
  return l10n_util::GetStringUTF16(
      has_suggestions ? IDS_MANUAL_FILLING_CREDIT_CARD_SHEET_TITLE
                      : IDS_MANUAL_FILLING_CREDIT_CARD_SHEET_EMPTY_MESSAGE);
}

void AddSimpleField(const std::u16string& data,
                    UserInfo* user_info,
                    bool enabled) {
  user_info->add_field(AccessorySheetField(
      /*display_text=*/data, /*text_to_fill=*/data, /*a11y_description=*/data,
      /*id=*/std::string(),
      /*is_password=*/false, enabled));
}

void AddCardDetailsToUserInfo(const CreditCard& card,
                              UserInfo* user_info,
                              std::u16string cvc,
                              bool enabled) {
  if (card.HasValidExpirationDate()) {
    AddSimpleField(card.Expiration2DigitMonthAsString(), user_info, enabled);
    AddSimpleField(card.Expiration4DigitYearAsString(), user_info, enabled);
  } else {
    AddSimpleField(std::u16string(), user_info, enabled);
    AddSimpleField(std::u16string(), user_info, enabled);
  }

  if (card.HasNameOnCard()) {
    AddSimpleField(card.GetRawInfo(autofill::CREDIT_CARD_NAME_FULL), user_info,
                   enabled);
  } else {
    AddSimpleField(std::u16string(), user_info, enabled);
  }
  AddSimpleField(cvc, user_info, enabled);
}

UserInfo TranslateCard(const CreditCard* data, bool enabled) {
  DCHECK(data);

  UserInfo user_info(data->network(), GetCardArtUrl(*data));

  std::u16string obfuscated_number =
      data->CardIdentifierStringForManualFilling();
  // The `text_to_fill` field is set to an empty string as we're populating the
  // `id` of the `UserInfoField` which would be used to determine the type of
  // the card and fill the form accordingly.
  user_info.add_field(AccessorySheetField(
      obfuscated_number, /*text_to_fill=*/std::u16string(), obfuscated_number,
      data->guid(), /*is_password=*/false, enabled));
  AddCardDetailsToUserInfo(*data, &user_info, std::u16string(), enabled);

  return user_info;
}

UserInfo TranslateCachedCard(const CachedServerCardInfo* data, bool enabled) {
  DCHECK(data);

  const CreditCard& card = data->card;
  UserInfo user_info(card.network(), GetCardArtUrl(card));
  std::u16string card_number = card.GetRawInfo(autofill::CREDIT_CARD_NUMBER);
  user_info.add_field(AccessorySheetField(
      card.FullDigitsForDisplay(), card_number, card_number,
      /*id=*/std::string(), /*is_password=*/false, enabled));
  AddCardDetailsToUserInfo(card, &user_info, data->cvc, enabled);

  return user_info;
}

}  // namespace

CreditCardAccessoryControllerImpl::~CreditCardAccessoryControllerImpl() {
  if (personal_data_manager_)
    personal_data_manager_->RemoveObserver(this);
}

void CreditCardAccessoryControllerImpl::RegisterFillingSourceObserver(
    FillingSourceObserver observer) {
  source_observer_ = std::move(observer);
}

absl::optional<autofill::AccessorySheetData>
CreditCardAccessoryControllerImpl::GetSheetData() const {
  // Note that also GetManager() can return nullptr.
  autofill::BrowserAutofillManager* autofill_manager =
      web_contents_->GetFocusedFrame() ? GetManager() : nullptr;
  std::vector<UserInfo> info_to_add;
  bool allow_filling =
      autofill_manager &&
      ShouldAllowCreditCardFallbacks(autofill_manager->client(),
                                     autofill_manager->last_query_form());

  if (!cached_server_cards_.empty()) {
    // Add the cached server cards first, so that they show up on the top of the
    // manual filling view.
    std::transform(cached_server_cards_.begin(), cached_server_cards_.end(),
                   std::back_inserter(info_to_add),
                   [allow_filling](const CachedServerCardInfo* data) {
                     return TranslateCachedCard(data, allow_filling);
                   });
  }
  // Only add cards that are not present in the cache. Otherwise, we might
  // show duplicates.
  bool add_all_cards = cached_server_cards_.empty() || !autofill_manager;
  for (auto* card : cards_cache_) {
    if (add_all_cards || !autofill_manager->credit_card_access_manager()
                              ->IsCardPresentInUnmaskedCache(*card)) {
      info_to_add.push_back(TranslateCard(card, allow_filling));
    }
  }

  const std::vector<FooterCommand> footer_commands = {FooterCommand(
      l10n_util::GetStringUTF16(
          IDS_MANUAL_FILLING_CREDIT_CARD_SHEET_ALL_ADDRESSES_LINK),
      AccessoryAction::MANAGE_CREDIT_CARDS)};

  bool has_suggestions = !info_to_add.empty();

  AccessorySheetData data = autofill::CreateAccessorySheetData(
      AccessoryTabType::CREDIT_CARDS, GetTitle(has_suggestions),
      std::move(info_to_add), std::move(footer_commands));
  if (has_suggestions && !allow_filling && autofill_manager) {
    data.set_warning(
        l10n_util::GetStringUTF16(IDS_AUTOFILL_WARNING_INSECURE_CONNECTION));
  }
  return data;
}

void CreditCardAccessoryControllerImpl::OnFillingTriggered(
    FieldGlobalId focused_field_id,
    const AccessorySheetField& selection) {
  content::RenderFrameHost* rfh = web_contents_->GetFocusedFrame();
  if (!rfh)
    return;  // Without focused frame, driver and manager will be undefined.
  if (!GetDriver() || !GetManager()) {
    // Even with a valid frame, driver or manager might be invalid. Log these
    // cases to check how we can recover and fail gracefully so users can retry.
    base::debug::DumpWithoutCrashing();
    return;
  }

  // Credit card number fields have a GUID populated to allow deobfuscation
  // before filling.
  if (selection.id().empty()) {
    GetDriver()->RendererShouldFillFieldWithValue(focused_field_id,
                                                  selection.text_to_fill());
    return;
  }

  auto card_iter = std::find_if(cards_cache_.begin(), cards_cache_.end(),
                                [&selection](const auto* card) {
                                  return card && card->guid() == selection.id();
                                });

  if (card_iter == cards_cache_.end()) {
    NOTREACHED() << "Tried to fill card with unknown GUID";
    return;
  }

  CreditCard* matching_card = *card_iter;
  switch (matching_card->record_type()) {
    case CreditCard::RecordType::MASKED_SERVER_CARD:
    case CreditCard::RecordType::VIRTUAL_CARD:
      last_focused_field_id_ = focused_field_id;
      GetManager()->credit_card_access_manager()->FetchCreditCard(matching_card,
                                                                  AsWeakPtr());
      break;
    case CreditCard::RecordType::LOCAL_CARD:
    case CreditCard::RecordType::FULL_SERVER_CARD:
      GetDriver()->RendererShouldFillFieldWithValue(focused_field_id,
                                                    matching_card->number());
      break;
  }
}

void CreditCardAccessoryControllerImpl::OnOptionSelected(
    AccessoryAction selected_action) {
  if (selected_action == AccessoryAction::MANAGE_CREDIT_CARDS) {
    autofill::ShowAutofillCreditCardSettings(web_contents_);
    return;
  }
  NOTREACHED() << "Unhandled selected action: "
               << static_cast<int>(selected_action);
}

void CreditCardAccessoryControllerImpl::OnToggleChanged(
    AccessoryAction toggled_action,
    bool enabled) {
  NOTREACHED() << "Unhandled toggled action: "
               << static_cast<int>(toggled_action);
}

// static
bool CreditCardAccessoryController::AllowedForWebContents(
    content::WebContents* web_contents) {
  DCHECK(web_contents) << "Need valid WebContents to attach controller to!";
  if (vr::VrTabHelper::IsInVr(web_contents)) {
    return false;  // TODO(crbug.com/902305): Re-enable if possible.
  }
  return features::IsAutofillManualFallbackEnabled();
}

// static
CreditCardAccessoryController* CreditCardAccessoryController::GetOrCreate(
    content::WebContents* web_contents) {
  DCHECK(CreditCardAccessoryController::AllowedForWebContents(web_contents));

  CreditCardAccessoryControllerImpl::CreateForWebContents(web_contents);
  return CreditCardAccessoryControllerImpl::FromWebContents(web_contents);
}

// static
CreditCardAccessoryController* CreditCardAccessoryController::GetIfExisting(
    content::WebContents* web_contents) {
  return CreditCardAccessoryControllerImpl::FromWebContents(web_contents);
}

void CreditCardAccessoryControllerImpl::RefreshSuggestions() {
  if (web_contents_->GetFocusedFrame() && GetManager()) {
    FetchSuggestions();
  } else {
    cards_cache_.clear();  // If cards cannot be filled, don't show them.
    cached_server_cards_.clear();
    virtual_cards_cache_.clear();
  }
  absl::optional<AccessorySheetData> data = GetSheetData();
  if (source_observer_) {
    source_observer_.Run(this, IsFillingSourceAvailable(data.has_value()));
  } else {
    // TODO(crbug.com/1169167): Remove once filling controller pulls this
    // information instead of waiting to get it pushed.
    DCHECK(data.has_value());
    GetManualFillingController()->RefreshSuggestions(std::move(data.value()));
  }
}

void CreditCardAccessoryControllerImpl::OnPersonalDataChanged() {
  RefreshSuggestions();
}

void CreditCardAccessoryControllerImpl::OnCreditCardFetched(
    CreditCardFetchResult result,
    const CreditCard* credit_card,
    const std::u16string& cvc) {
  if (result != CreditCardFetchResult::kSuccess)
    return;
  content::RenderFrameHost* rfh = web_contents_->GetFocusedFrame();
  if (!rfh || !last_focused_field_id_ ||
      last_focused_field_id_.frame_token !=
          autofill::LocalFrameToken(rfh->GetFrameToken().value())) {
    last_focused_field_id_ = {};
    return;  // If frame isn't focused anymore, don't attempt to fill.
  }
  DCHECK(credit_card);
  DCHECK(GetDriver());

  GetDriver()->RendererShouldFillFieldWithValue(last_focused_field_id_,
                                                credit_card->number());
  last_focused_field_id_ = {};
}

// static
void CreditCardAccessoryControllerImpl::CreateForWebContentsForTesting(
    content::WebContents* web_contents,
    base::WeakPtr<ManualFillingController> mf_controller,
    autofill::PersonalDataManager* personal_data_manager,
    autofill::BrowserAutofillManager* af_manager,
    autofill::AutofillDriver* af_driver) {
  DCHECK(web_contents) << "Need valid WebContents to attach controller to!";
  DCHECK(!FromWebContents(web_contents)) << "Controller already attached!";
  DCHECK(mf_controller);

  web_contents->SetUserData(
      UserDataKey(), base::WrapUnique(new CreditCardAccessoryControllerImpl(
                         web_contents, std::move(mf_controller),
                         personal_data_manager, af_manager, af_driver)));
}

CreditCardAccessoryControllerImpl::CreditCardAccessoryControllerImpl(
    content::WebContents* web_contents)
    : web_contents_(web_contents),
      personal_data_manager_(
          autofill::PersonalDataManagerFactory::GetForProfile(
              Profile::FromBrowserContext(
                  web_contents_->GetBrowserContext()))) {
  if (personal_data_manager_)
    personal_data_manager_->AddObserver(this);
}

CreditCardAccessoryControllerImpl::CreditCardAccessoryControllerImpl(
    content::WebContents* web_contents,
    base::WeakPtr<ManualFillingController> mf_controller,
    PersonalDataManager* personal_data_manager,
    autofill::BrowserAutofillManager* af_manager,
    autofill::AutofillDriver* af_driver)
    : web_contents_(web_contents),
      mf_controller_(mf_controller),
      personal_data_manager_(personal_data_manager),
      af_manager_for_testing_(af_manager),
      af_driver_for_testing_(af_driver) {
  if (personal_data_manager_)
    personal_data_manager_->AddObserver(this);
}

void CreditCardAccessoryControllerImpl::FetchSuggestions() {
  DCHECK(web_contents_->GetFocusedFrame());
  autofill::BrowserAutofillManager* autofill_manager = GetManager();
  DCHECK(autofill_manager);
  if (!personal_data_manager_) {
    cards_cache_.clear();  // No data available.
    virtual_cards_cache_.clear();
  } else {
    cards_cache_ = personal_data_manager_->GetCreditCardsToSuggest(
        /*include_server_cards=*/true);
    // If any of the server cards are enrolled for virtual cards, then insert a
    // virtual card suggestion right before the actual server card.
    if (base::FeatureList::IsEnabled(
            autofill::features::kAutofillEnableMerchantBoundVirtualCards)) {
      std::vector<CreditCard*> cards_to_suggest_with_virtual_cards;
      for (CreditCard* card : cards_cache_) {
        if (card->virtual_card_enrollment_state() == CreditCard::ENROLLED) {
          std::unique_ptr<CreditCard> virtual_card =
              CreditCard::CreateVirtualCard(*card);
          cards_to_suggest_with_virtual_cards.push_back(virtual_card.get());
          virtual_cards_cache_.push_back(std::move(virtual_card));
        }
        cards_to_suggest_with_virtual_cards.push_back(card);
      }
      cards_cache_ = std::move(cards_to_suggest_with_virtual_cards);
    }
  }
  if (!autofill_manager->credit_card_access_manager()) {
    cached_server_cards_.clear();  // No data available.
    return;
  }
  cached_server_cards_ =
      autofill_manager->credit_card_access_manager()->GetCachedUnmaskedCards();
  // If the feature to show unmasked cached cards in manual filling view is
  // enabled, show all cached cards in the view. Even if not, still show
  // virtual cards in the manual filling view if they exist. All other cards
  // are dropped.
  if (base::FeatureList::IsEnabled(
          autofill::features::
              kAutofillShowUnmaskedCachedCardInManualFillingView)) {
    return;
  }
  auto not_virtual_card = [](const CachedServerCardInfo* card_info) {
    return card_info->card.record_type() != CreditCard::VIRTUAL_CARD;
  };
  base::EraseIf(cached_server_cards_, not_virtual_card);
}

base::WeakPtr<ManualFillingController>
CreditCardAccessoryControllerImpl::GetManualFillingController() {
  if (!mf_controller_)
    mf_controller_ = ManualFillingController::GetOrCreate(web_contents_);
  DCHECK(mf_controller_);
  return mf_controller_;
}

autofill::AutofillDriver* CreditCardAccessoryControllerImpl::GetDriver() {
  DCHECK(web_contents_->GetFocusedFrame());
  return af_driver_for_testing_
             ? af_driver_for_testing_
             : autofill::ContentAutofillDriver::GetForRenderFrameHost(
                   web_contents_->GetFocusedFrame());
}

autofill::BrowserAutofillManager*
CreditCardAccessoryControllerImpl::GetManager() const {
  DCHECK(web_contents_->GetFocusedFrame());
  if (af_manager_for_testing_)
    return af_manager_for_testing_;
  autofill::ContentAutofillDriver* driver =
      autofill::ContentAutofillDriver::GetForRenderFrameHost(
          web_contents_->GetFocusedFrame());
  return driver ? driver->browser_autofill_manager() : nullptr;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(CreditCardAccessoryControllerImpl)

}  // namespace autofill
