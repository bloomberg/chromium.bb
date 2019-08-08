// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/details.h"

#include <unordered_set>

#include <base/strings/stringprintf.h>
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/geo/country_names.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill_assistant {

constexpr char kSpaceBetweenCardNumAndDate[] = "    ";

// static
bool Details::UpdateFromProto(const ShowDetailsProto& proto, Details* details) {
  if (!proto.has_details()) {
    return false;
  }

  ShowDetailsProto updated_proto = proto;
  // Legacy treatment for old proto fields. Can be removed once the backend
  // is updated to set the description_line_1/line_2 fields.
  if (updated_proto.details().has_description() &&
      !updated_proto.details().has_description_line_2()) {
    updated_proto.mutable_details()->set_description_line_2(
        updated_proto.details().description());
  }
  details->SetDetailsProto(updated_proto.details());
  details->SetDetailsChangesProto(updated_proto.change_flags());
  return true;
}

// static
bool Details::UpdateFromContactDetails(const ShowDetailsProto& proto,
                                       ClientMemory* client_memory,
                                       Details* details) {
  std::string contact_details = proto.contact_details();
  if (!client_memory->has_selected_address(contact_details)) {
    return false;
  }

  ShowDetailsProto updated_proto = proto;
  auto* profile = client_memory->selected_address(contact_details);
  auto* details_proto = updated_proto.mutable_details();
  // TODO(crbug.com/806868): Get the actual script locale.
  std::string app_locale = "en-US";
  details_proto->set_title(
      l10n_util::GetStringUTF8(IDS_PAYMENTS_CONTACT_DETAILS_LABEL));
  details_proto->set_description_line_1(
      base::UTF16ToUTF8(profile->GetInfo(autofill::NAME_FULL, app_locale)));
  details_proto->set_description_line_2(
      base::UTF16ToUTF8(profile->GetInfo(autofill::EMAIL_ADDRESS, app_locale)));
  details->SetDetailsProto(updated_proto.details());
  details->SetDetailsChangesProto(updated_proto.change_flags());
  return true;
}

// static
bool Details::UpdateFromShippingAddress(const ShowDetailsProto& proto,
                                        ClientMemory* client_memory,
                                        Details* details) {
  std::string shipping_address = proto.shipping_address();
  if (!client_memory->has_selected_address(shipping_address)) {
    return false;
  }

  ShowDetailsProto updated_proto = proto;
  auto* profile = client_memory->selected_address(shipping_address);
  auto* details_proto = updated_proto.mutable_details();
  // TODO(crbug.com/806868): Get the actual script locale.
  std::string app_locale = "en-US";
  autofill::CountryNames* country_names = autofill::CountryNames::GetInstance();
  details_proto->set_title(
      l10n_util::GetStringUTF8(IDS_PAYMENTS_SHIPPING_ADDRESS_LABEL));
  details_proto->set_description_line_1(
      base::UTF16ToUTF8(profile->GetInfo(autofill::NAME_FULL, app_locale)));
  details_proto->set_description_line_2(base::StrCat({
      base::UTF16ToUTF8(
          profile->GetInfo(autofill::ADDRESS_HOME_STREET_ADDRESS, app_locale)),
      " ",
      base::UTF16ToUTF8(
          profile->GetInfo(autofill::ADDRESS_HOME_ZIP, app_locale)),
      " ",
      base::UTF16ToUTF8(
          profile->GetInfo(autofill::ADDRESS_HOME_CITY, app_locale)),
      " ",
      country_names->GetCountryCode(
          profile->GetInfo(autofill::ADDRESS_HOME_COUNTRY, app_locale)),
  }));
  details->SetDetailsProto(updated_proto.details());
  details->SetDetailsChangesProto(updated_proto.change_flags());
  return true;
}

bool Details::UpdateFromSelectedCreditCard(const ShowDetailsProto& proto,
                                           ClientMemory* client_memory,
                                           Details* details) {
  if (!client_memory->has_selected_card() || !proto.credit_card()) {
    return false;
  }

  ShowDetailsProto updated_proto = proto;
  auto* card = client_memory->selected_card();
  auto* details_proto = updated_proto.mutable_details();
  details_proto->set_title(
      l10n_util::GetStringUTF8(IDS_PAYMENTS_METHOD_OF_PAYMENT_LABEL));
  details_proto->set_description_line_1(
      base::StrCat({base::UTF16ToUTF8(card->ObfuscatedLastFourDigits()),
                    kSpaceBetweenCardNumAndDate,
                    base::UTF16ToUTF8(card->AbbreviatedExpirationDateForDisplay(
                        /* with_prefix = */ false))}));
  details->SetDetailsProto(updated_proto.details());
  details->SetDetailsChangesProto(updated_proto.change_flags());
  return true;
}

base::Value Details::GetDebugContext() const {
  base::Value dict(base::Value::Type::DICTIONARY);
  if (!detailsProto().title().empty())
    dict.SetKey("title", base::Value(detailsProto().title()));

  if (!detailsProto().image_url().empty())
    dict.SetKey("image_url", base::Value(detailsProto().image_url()));

  if (!detailsProto().total_price().empty())
    dict.SetKey("total_price", base::Value(detailsProto().total_price()));

  if (!detailsProto().total_price_label().empty())
    dict.SetKey("total_price_label",
                base::Value(detailsProto().total_price_label()));

  if (!detailsProto().description_line_1().empty())
    dict.SetKey("description_line_1",
                base::Value(detailsProto().description_line_1()));

  if (!detailsProto().description_line_2().empty())
    dict.SetKey("description_line_2",
                base::Value(detailsProto().description_line_2()));

  if (!detailsProto().description_line_3().empty())
    dict.SetKey("description_line_3",
                base::Value(detailsProto().description_line_3()));

  if (detailsProto().has_datetime()) {
    dict.SetKey("datetime",
                base::Value(base::StringPrintf(
                    "%d-%02d-%02dT%02d:%02d:%02d",
                    static_cast<int>(detailsProto().datetime().date().year()),
                    detailsProto().datetime().date().month(),
                    detailsProto().datetime().date().day(),
                    detailsProto().datetime().time().hour(),
                    detailsProto().datetime().time().minute(),
                    detailsProto().datetime().time().second())));
  }
  if (!datetime_.empty())
    dict.SetKey("datetime_str", base::Value(datetime_));

  dict.SetKey("user_approval_required",
              base::Value(changes().user_approval_required()));
  dict.SetKey("highlight_title", base::Value(changes().highlight_title()));
  dict.SetKey("highlight_line1", base::Value(changes().highlight_line1()));
  dict.SetKey("highlight_line2", base::Value(changes().highlight_line2()));
  dict.SetKey("highlight_line3", base::Value(changes().highlight_line3()));
  dict.SetKey("highlight_line3", base::Value(changes().highlight_line3()));

  return dict;
}

bool Details::UpdateFromParameters(
    const std::map<std::string, std::string>& parameters) {
  const auto iter = parameters.find("DETAILS_SHOW_INITIAL");
  if (iter != parameters.end() && iter->second.compare("false") == 0) {
    return false;
  }
  // Whenever details are updated from parameters we want to animate missing
  // data.
  proto_.set_animate_placeholders(true);
  proto_.set_show_image_placeholder(true);
  if (MaybeUpdateFromDetailsParameters(parameters)) {
    return true;
  }

  // NOTE: The logic below is only needed for backward compatibility.
  // Remove once we always pass detail parameters.
  bool is_updated = false;
  for (const auto& iter : parameters) {
    std::string key = iter.first;
    if (key == "MOVIES_MOVIE_NAME") {
      proto_.set_title(iter.second);
      is_updated = true;
      continue;
    }

    if (key == "MOVIES_THEATER_NAME") {
      proto_.set_description_line_2(iter.second);
      is_updated = true;
      continue;
    }

    if (iter.first.compare("MOVIES_SCREENING_DATETIME") == 0) {
      // TODO(crbug.com/806868): Parse the string here and fill
      // proto.description_line_1, then get rid of datetime_ in Details.
      datetime_ = iter.second;
      is_updated = true;
      continue;
    }
  }
  return is_updated;
}

bool Details::MaybeUpdateFromDetailsParameters(
    const std::map<std::string, std::string>& parameters) {
  bool details_updated = false;
  for (const auto& iter : parameters) {
    std::string key = iter.first;
    if (key == "DETAILS_TITLE") {
      proto_.set_title(iter.second);
      details_updated = true;
      continue;
    }

    if (key == "DETAILS_DESCRIPTION_LINE_1") {
      proto_.set_description_line_1(iter.second);
      details_updated = true;
      continue;
    }

    if (key == "DETAILS_DESCRIPTION_LINE_2") {
      proto_.set_description_line_2(iter.second);
      details_updated = true;
      continue;
    }

    if (key == "DETAILS_DESCRIPTION_LINE_3") {
      proto_.set_description_line_3(iter.second);
      details_updated = true;
      continue;
    }

    if (key == "DETAILS_IMAGE_URL") {
      proto_.set_image_url(iter.second);
      details_updated = true;
      continue;
    }

    if (key == "DETAILS_IMAGE_CLICKTHROUGH_URL") {
      proto_.mutable_image_clickthrough_data()->set_allow_clickthrough(true);
      proto_.mutable_image_clickthrough_data()->set_clickthrough_url(
          iter.second);
      details_updated = true;
      continue;
    }

    if (key == "DETAILS_TOTAL_PRICE_LABEL") {
      proto_.set_total_price_label(iter.second);
      details_updated = true;
      continue;
    }

    if (key == "DETAILS_TOTAL_PRICE") {
      proto_.set_total_price(iter.second);
      details_updated = true;
      continue;
    }
  }
  return details_updated;
}

void Details::ClearChanges() {
  change_flags_.Clear();
}

}  // namespace autofill_assistant
