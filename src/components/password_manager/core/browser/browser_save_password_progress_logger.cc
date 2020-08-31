// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"

#include <utility>

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/proto/server.pb.h"
#include "components/autofill/core/common/password_form.h"
#include "components/autofill/core/common/signatures.h"
#include "components/password_manager/core/browser/password_form_metrics_recorder.h"
#include "components/password_manager/core/browser/password_manager.h"

using autofill::AutofillUploadContents;
using autofill::FieldPropertiesFlags;
using autofill::FormStructure;
using autofill::PasswordAttribute;
using autofill::PasswordForm;
using autofill::ServerFieldType;
using base::NumberToString;

namespace password_manager {

namespace {

// Replaces all non-digits in |str| by spaces.
std::string ScrubNonDigit(std::string str) {
  std::replace_if(
      str.begin(), str.end(), [](char c) { return !base::IsAsciiDigit(c); },
      ' ');
  return str;
}

std::string GenerationTypeToString(
    AutofillUploadContents::Field::PasswordGenerationType type) {
  switch (type) {
    case AutofillUploadContents::Field::NO_GENERATION:
      return std::string();
    case AutofillUploadContents::Field::
        AUTOMATICALLY_TRIGGERED_GENERATION_ON_SIGN_UP_FORM:
      return "Generation on sign-up";
    case AutofillUploadContents::Field::
        AUTOMATICALLY_TRIGGERED_GENERATION_ON_CHANGE_PASSWORD_FORM:
      return "Generation on change password";
    case AutofillUploadContents::Field::
        MANUALLY_TRIGGERED_GENERATION_ON_SIGN_UP_FORM:
      return "Manual generation on sign-up";
    case AutofillUploadContents::Field::
        MANUALLY_TRIGGERED_GENERATION_ON_CHANGE_PASSWORD_FORM:
      return "Manual generation on change password";
    case AutofillUploadContents::Field::IGNORED_GENERATION_POPUP:
      return "Generation ignored";
    default:
      NOTREACHED();
  }
  return std::string();
}

std::string VoteTypeToString(
    AutofillUploadContents::Field::VoteType vote_type) {
  switch (vote_type) {
    case AutofillUploadContents::Field::NO_INFORMATION:
      return "No information";
    case AutofillUploadContents::Field::CREDENTIALS_REUSED:
      return "Credentials reused";
    case AutofillUploadContents::Field::USERNAME_OVERWRITTEN:
      return "Username overwritten";
    case AutofillUploadContents::Field::USERNAME_EDITED:
      return "Username edited";
    case AutofillUploadContents::Field::BASE_HEURISTIC:
      return "Base heuristic";
    case AutofillUploadContents::Field::HTML_CLASSIFIER:
      return "HTML classifier";
    case AutofillUploadContents::Field::FIRST_USE:
      return "First use";
  }
}

std::string FormSignatureToDebugString(autofill::FormSignature form_signature) {
  return base::StrCat(
      {NumberToString(form_signature.value()), " - ",
       NumberToString(
           PasswordFormMetricsRecorder::HashFormSignature(form_signature))});
}

BrowserSavePasswordProgressLogger::StringID FormSchemeToStringID(
    PasswordForm::Scheme scheme) {
  switch (scheme) {
    case PasswordForm::Scheme::kHtml:
      return BrowserSavePasswordProgressLogger::STRING_SCHEME_HTML;
    case PasswordForm::Scheme::kBasic:
      return BrowserSavePasswordProgressLogger::STRING_SCHEME_BASIC;
    case PasswordForm::Scheme::kDigest:
      return BrowserSavePasswordProgressLogger::STRING_SCHEME_DIGEST;
    case PasswordForm::Scheme::kOther:
      return BrowserSavePasswordProgressLogger::STRING_OTHER;
    case PasswordForm::Scheme::kUsernameOnly:
      return BrowserSavePasswordProgressLogger::STRING_SCHEME_USERNAME_ONLY;
  }
  NOTREACHED();
  return BrowserSavePasswordProgressLogger::STRING_INVALID;
}

}  // namespace

BrowserSavePasswordProgressLogger::BrowserSavePasswordProgressLogger(
    const autofill::LogManager* log_manager)
    : log_manager_(log_manager) {
  DCHECK(log_manager_);
}

BrowserSavePasswordProgressLogger::~BrowserSavePasswordProgressLogger() =
    default;

void BrowserSavePasswordProgressLogger::LogFormStructure(
    StringID label,
    const FormStructure& form_structure) {
  std::string message = GetStringFromID(label) + ": {\n";
  message += GetStringFromID(STRING_FORM_SIGNATURE) + ": " +
             FormSignatureToDebugString(form_structure.form_signature()) + "\n";
  message += GetStringFromID(STRING_ORIGIN) + ": " +
             ScrubURL(form_structure.source_url()) + "\n";
  message += GetStringFromID(STRING_ACTION) + ": " +
             ScrubURL(form_structure.target_url()) + "\n";
  message += FormStructureToFieldsLogString(form_structure);
  message += FormStructurePasswordAttributesLogString(form_structure);
  message += "}";
  SendLog(message);
}

void BrowserSavePasswordProgressLogger::LogSuccessiveOrigins(
    StringID label,
    const GURL& old_origin,
    const GURL& new_origin) {
  std::string message = GetStringFromID(label) + ": {\n";
  message +=
      GetStringFromID(STRING_ORIGIN) + ": " + ScrubURL(old_origin) + "\n";
  message +=
      GetStringFromID(STRING_ORIGIN) + ": " + ScrubURL(new_origin) + "\n";
  message += "}";
  SendLog(message);
}

std::string
BrowserSavePasswordProgressLogger::FormStructurePasswordAttributesLogString(
    const FormStructure& form) {
  const base::Optional<std::pair<PasswordAttribute, bool>> attribute_vote =
      form.get_password_attributes_vote();
  if (!attribute_vote.has_value())
    return std::string();
  std::string message;
  const PasswordAttribute attribute = std::get<0>(attribute_vote.value());
  const bool attribute_value = std::get<1>(attribute_vote.value());

  switch (attribute) {
    case PasswordAttribute::kHasLowercaseLetter:
      message += BinaryPasswordAttributeLogString(
          STRING_PASSWORD_REQUIREMENTS_VOTE_FOR_LOWERCASE, attribute_value);
      break;

    case PasswordAttribute::kHasSpecialSymbol:
      message += BinaryPasswordAttributeLogString(
          STRING_PASSWORD_REQUIREMENTS_VOTE_FOR_SPECIAL_SYMBOL,
          attribute_value);
      if (attribute_value) {
        std::string voted_symbol(
            1, static_cast<char>(form.get_password_symbol_vote()));
        message += PasswordAttributeLogString(
            STRING_PASSWORD_REQUIREMENTS_VOTE_FOR_SPECIFIC_SPECIAL_SYMBOL,
            voted_symbol);
      }
      break;

    case PasswordAttribute::kPasswordAttributesCount:
      break;
  }
  std::string password_length =
      base::NumberToString(form.get_password_length_vote());
  message += PasswordAttributeLogString(
      STRING_PASSWORD_REQUIREMENTS_VOTE_FOR_PASSWORD_LENGTH, password_length);

  return message;
}

std::string BrowserSavePasswordProgressLogger::FormStructureToFieldsLogString(
    const FormStructure& form_structure) {
  std::string result;
  result += GetStringFromID(STRING_FIELDS) + ": " + "\n";
  for (const auto& field : form_structure) {
    std::string field_info =
        ScrubElementID(field->name) + ": " +
        ScrubNonDigit(field->FieldSignatureAsStr()) +
        ", type=" + ScrubElementID(field->form_control_type);

    field_info +=
        ", renderer_id = " + NumberToString(field->unique_renderer_id.value());

    if (!field->autocomplete_attribute.empty())
      field_info +=
          ", autocomplete=" + ScrubElementID(field->autocomplete_attribute);

    if (field->server_type() != autofill::NO_SERVER_DATA) {
      field_info +=
          ", SERVER_PREDICTION: " +
          autofill::AutofillType::ServerFieldTypeToString(field->server_type());
    }

    for (ServerFieldType type : field->possible_types())
      field_info +=
          ", VOTE: " + autofill::AutofillType::ServerFieldTypeToString(type);

    if (field->vote_type())
      field_info += ", vote_type=" + VoteTypeToString(field->vote_type());

    if (field->properties_mask) {
      field_info += ", properties=";
      field_info += (field->properties_mask & FieldPropertiesFlags::kUserTyped)
                        ? "T"
                        : "_";
      field_info +=
          (field->properties_mask & FieldPropertiesFlags::kAutofilledOnPageLoad)
              ? "Ap"
              : "__";
      field_info += (field->properties_mask &
                     FieldPropertiesFlags::kAutofilledOnUserTrigger)
                        ? "Au"
                        : "__";
      field_info += (field->properties_mask & FieldPropertiesFlags::kHadFocus)
                        ? "F"
                        : "_";
      field_info += (field->properties_mask & FieldPropertiesFlags::kKnownValue)
                        ? "K"
                        : "_";
    }

    if (field->initial_value_hash().has_value()) {
      field_info += ", initial value hash=";
      field_info += field->initial_value_hash().value();
    }

    std::string generation = GenerationTypeToString(field->generation_type());
    if (!generation.empty())
      field_info += ", GENERATION_EVENT: " + generation;

    if (field->generated_password_changed())
      field_info += ", generated password changed";

    result += field_info + "\n";
  }

  return result;
}

void BrowserSavePasswordProgressLogger::LogString(StringID label,
                                                  const std::string& s) {
  LogValue(label, base::Value(s));
}

void BrowserSavePasswordProgressLogger::LogSuccessfulSubmissionIndicatorEvent(
    autofill::mojom::SubmissionIndicatorEvent event) {
  std::ostringstream submission_event_string_stream;
  submission_event_string_stream << event;
  std::string message =
      GetStringFromID(STRING_SUCCESSFUL_SUBMISSION_INDICATOR_EVENT) + ": " +
      submission_event_string_stream.str();
  SendLog(message);
}

void BrowserSavePasswordProgressLogger::LogPasswordForm(
    BrowserSavePasswordProgressLogger::StringID label,
    const PasswordForm& form) {
  base::DictionaryValue log;
  log.SetString(GetStringFromID(STRING_SCHEME_MESSAGE),
                GetStringFromID(FormSchemeToStringID(form.scheme)));
  log.SetString(GetStringFromID(STRING_SCHEME_MESSAGE),
                GetStringFromID(FormSchemeToStringID(form.scheme)));
  log.SetString(GetStringFromID(STRING_SIGNON_REALM),
                ScrubURL(GURL(form.signon_realm)));
  log.SetString(GetStringFromID(STRING_ORIGIN), ScrubURL(form.origin));
  log.SetString(GetStringFromID(STRING_ACTION), ScrubURL(form.action));
  log.SetString(GetStringFromID(STRING_USERNAME_ELEMENT),
                ScrubElementID(form.username_element));
  if (form.has_renderer_ids) {
    log.SetString(GetStringFromID(STRING_USERNAME_ELEMENT_RENDERER_ID),
                  NumberToString(form.username_element_renderer_id.value()));
  }
  log.SetString(GetStringFromID(STRING_PASSWORD_ELEMENT),
                ScrubElementID(form.password_element));
  if (form.has_renderer_ids) {
    log.SetString(GetStringFromID(STRING_PASSWORD_ELEMENT_RENDERER_ID),
                  NumberToString(form.password_element_renderer_id.value()));
  }
  log.SetString(GetStringFromID(STRING_NEW_PASSWORD_ELEMENT),
                ScrubElementID(form.new_password_element));
  if (form.has_renderer_ids) {
    log.SetString(
        GetStringFromID(STRING_NEW_PASSWORD_ELEMENT_RENDERER_ID),
        NumberToString(form.new_password_element_renderer_id.value()));
  }
  if (!form.confirmation_password_element.empty()) {
    log.SetString(GetStringFromID(STRING_CONFIRMATION_PASSWORD_ELEMENT),
                  ScrubElementID(form.confirmation_password_element));
    if (form.has_renderer_ids) {
      log.SetString(
          GetStringFromID(STRING_CONFIRMATION_PASSWORD_ELEMENT_RENDERER_ID),
          NumberToString(
              form.confirmation_password_element_renderer_id.value()));
    }
  }
  log.SetBoolean(GetStringFromID(STRING_PASSWORD_GENERATED),
                 form.type == PasswordForm::Type::kGenerated);
  log.SetInteger(GetStringFromID(STRING_TIMES_USED), form.times_used);
  log.SetBoolean(GetStringFromID(STRING_PSL_MATCH),
                 form.is_public_suffix_match);
  LogValue(label, log);
}

void BrowserSavePasswordProgressLogger::SendLog(const std::string& log) {
  log_manager_->LogTextMessage(log);
}

std::string BrowserSavePasswordProgressLogger::PasswordAttributeLogString(
    StringID string_id,
    const std::string& attribute_value) {
  return GetStringFromID(string_id) + ": " + attribute_value + "\n";
}

std::string BrowserSavePasswordProgressLogger::BinaryPasswordAttributeLogString(
    StringID string_id,
    bool attribute_value) {
  return PasswordAttributeLogString(string_id,
                                    (attribute_value ? "yes" : "no"));
}

}  // namespace password_manager
