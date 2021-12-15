// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_metrics.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/fixed_flat_map.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_piece_forward.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/autofill_offer_data.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/form_types.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/autofill_tick_clock.h"
#include "components/autofill/core/common/form_data.h"
#include "components/language/core/browser/language_usage_metrics.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"

namespace autofill {

using mojom::SubmissionSource;

namespace {

// Exponential bucket spacing for UKM event data.
constexpr double kAutofillEventDataBucketSpacing = 2.0;

// Translates structured name types into simple names that are used for
// naming histograms.
constexpr auto kStructuredNameTypeToNameMap =
    base::MakeFixedFlatMap<ServerFieldType, base::StringPiece>(
        {{NAME_FULL, "Full"},
         {NAME_FIRST, "First"},
         {NAME_MIDDLE, "Middle"},
         {NAME_LAST, "Last"},
         {NAME_LAST_FIRST, "FirstLast"},
         {NAME_LAST_SECOND, "SecondLast"}});

// Translates structured address types into simple names that are used for
// naming histograms.
constexpr auto kStructuredAddressTypeToNameMap =
    base::MakeFixedFlatMap<ServerFieldType, base::StringPiece>(
        {{ADDRESS_HOME_STREET_ADDRESS, "StreetAddress"},
         {ADDRESS_HOME_STREET_NAME, "StreetName"},
         {ADDRESS_HOME_HOUSE_NUMBER, "HouseNumber"},
         {ADDRESS_HOME_FLOOR, "FloorNumber"},
         {ADDRESS_HOME_APT_NUM, "ApartmentNumber"},
         {ADDRESS_HOME_PREMISE_NAME, "Premise"},
         {ADDRESS_HOME_SUBPREMISE, "SubPremise"}});

// Note: if adding an enum value here, update the corresponding description for
// AutofillFieldPredictionQualityByFieldType in
// tools/metrics/histograms/enums.xml.
enum FieldTypeGroupForMetrics {
  GROUP_AMBIGUOUS = 0,
  GROUP_NAME,
  GROUP_COMPANY,
  GROUP_ADDRESS_LINE_1,
  GROUP_ADDRESS_LINE_2,
  GROUP_ADDRESS_CITY,
  GROUP_ADDRESS_STATE,
  GROUP_ADDRESS_ZIP,
  GROUP_ADDRESS_COUNTRY,
  GROUP_ADDRESS_HOME_STREET_NAME,
  GROUP_ADDRESS_HOME_DEPENDENT_STREET_NAME,
  GROUP_ADDRESS_HOME_HOUSE_NUMBER,
  GROUP_ADDRESS_HOME_PREMISE_NAME,
  GROUP_ADDRESS_HOME_SUBPREMISE,
  GROUP_PHONE,
  GROUP_FAX,  // Deprecated.
  GROUP_EMAIL,
  GROUP_CREDIT_CARD_NAME,
  GROUP_CREDIT_CARD_NUMBER,
  GROUP_CREDIT_CARD_DATE,
  GROUP_CREDIT_CARD_TYPE,
  GROUP_PASSWORD,
  GROUP_ADDRESS_LINE_3,
  GROUP_USERNAME,
  GROUP_STREET_ADDRESS,
  GROUP_CREDIT_CARD_VERIFICATION,
  GROUP_UNFILLABLE,
  GROUP_ADDRESS_HOME_APT_NUM,
  GROUP_ADDRESS_HOME_SORTING_CODE,
  GROUP_ADDRESS_HOME_DEPENDENT_LOCALITY,
  GROUP_ADDRESS_HOME_STREET_AND_DEPENDENT_STREET_NAME,
  GROUP_ADDRESS_HOME_OTHER_SUBUNIT,
  GROUP_ADDRESS_HOME_ADDRESS,
  GROUP_ADDRESS_HOME_ADDRESS_WITH_NAME,
  GROUP_ADDRESS_HOME_FLOOR,
  GROUP_UNKNOWN_TYPE,
  // Add new entries here and update enums.xml.
  NUM_FIELD_TYPE_GROUPS_FOR_METRICS
};

// Converts a server field type that can be edited in the settings to an enum
// used for metrics.
AutofillMetrics::SettingsVisibleFieldTypeForMetrics
ConvertSettingsVisibleFieldTypeForMetrics(ServerFieldType field_type) {
  switch (field_type) {
    case ServerFieldType::NAME_FULL:
      return AutofillMetrics::SettingsVisibleFieldTypeForMetrics::kName;

    case ServerFieldType::EMAIL_ADDRESS:
      return AutofillMetrics::SettingsVisibleFieldTypeForMetrics::kEmailAddress;

    case ServerFieldType::PHONE_HOME_WHOLE_NUMBER:
      return AutofillMetrics::SettingsVisibleFieldTypeForMetrics::kPhoneNumber;

    case ServerFieldType::ADDRESS_HOME_CITY:
      return AutofillMetrics::SettingsVisibleFieldTypeForMetrics::kCity;

    case ServerFieldType::ADDRESS_HOME_COUNTRY:
      return AutofillMetrics::SettingsVisibleFieldTypeForMetrics::kCountry;

    case ServerFieldType::ADDRESS_HOME_ZIP:
      return AutofillMetrics::SettingsVisibleFieldTypeForMetrics::kZip;

    case ServerFieldType::ADDRESS_HOME_STATE:
      return AutofillMetrics::SettingsVisibleFieldTypeForMetrics::kState;

    case ServerFieldType::ADDRESS_HOME_STREET_ADDRESS:
      return AutofillMetrics::SettingsVisibleFieldTypeForMetrics::
          kStreetAddress;

    case ServerFieldType::ADDRESS_HOME_DEPENDENT_LOCALITY:
      return AutofillMetrics::SettingsVisibleFieldTypeForMetrics::
          kDependentLocality;

    case ServerFieldType::NAME_HONORIFIC_PREFIX:
      return AutofillMetrics::SettingsVisibleFieldTypeForMetrics::
          kHonorificPrefix;

    default:
      return AutofillMetrics::SettingsVisibleFieldTypeForMetrics::kUndefined;
  }
}

const char* GetSaveAndUpdatePromptDecisionMetricsSuffix(
    AutofillClient::SaveAddressProfileOfferUserDecision decision) {
  switch (decision) {
    case AutofillClient::SaveAddressProfileOfferUserDecision::kUndefined:
      return ".Undefined";
    case AutofillClient::SaveAddressProfileOfferUserDecision::kUserNotAsked:
      return ".UserNotAsked";
    case AutofillClient::SaveAddressProfileOfferUserDecision::kAccepted:
      return ".Accepted";
    case AutofillClient::SaveAddressProfileOfferUserDecision::kDeclined:
      return ".Declined";
    case AutofillClient::SaveAddressProfileOfferUserDecision::kEditAccepted:
      return ".EditAccepted";
    case AutofillClient::SaveAddressProfileOfferUserDecision::kEditDeclined:
      return ".EditDeclined";
    case AutofillClient::SaveAddressProfileOfferUserDecision::kNever:
      return ".Never";
    case AutofillClient::SaveAddressProfileOfferUserDecision::kIgnored:
      return ".Ignored";
    case AutofillClient::SaveAddressProfileOfferUserDecision::kMessageTimeout:
      return ".MessageTimeout";
    case AutofillClient::SaveAddressProfileOfferUserDecision::kMessageDeclined:
      return ".MessageDeclined";
    case AutofillClient::SaveAddressProfileOfferUserDecision::kAutoDeclined:
      return ".AutoDeclined";
  }
  NOTREACHED();
  return "";
}

std::string GetCreditCardTypeSuffix(
    AutofillClient::PaymentsRpcCardType card_type) {
  switch (card_type) {
    case AutofillClient::PaymentsRpcCardType::kServerCard:
      return ".ServerCard";
    case AutofillClient::PaymentsRpcCardType::kVirtualCard:
      return ".VirtualCard";
    case AutofillClient::PaymentsRpcCardType::kUnknown:
      NOTREACHED();
      return std::string();
  }
}

}  // namespace

// First, translates |field_type| to the corresponding logical |group| from
// |FieldTypeGroupForMetrics|.  Then, interpolates this with the given |metric|,
// which should be in the range [0, |num_possible_metrics|).
// Returns the interpolated index.
//
// The interpolation maps the pair (|group|, |metric|) to a single index, so
// that all the indices for a given group are adjacent.  In particular, with
// the groups {AMBIGUOUS, NAME, ...} combining with the metrics
// {UNKNOWN, MATCH, MISMATCH}, we create this set of mapped indices:
// {
//   AMBIGUOUS+UNKNOWN,
//   AMBIGUOUS+MATCH,
//   AMBIGUOUS+MISMATCH,
//   NAME+UNKNOWN,
//   NAME+MATCH,
//   NAME+MISMATCH,
//   ...
// }.
//
// Clients must ensure that |field_type| is one of the types Chrome supports
// natively, e.g. |field_type| must not be a billng address.
// NOTE: This is defined outside of the anonymous namespace so that it can be
// accessed from the unit test file. It is not exposed in the header file,
// however, because it is not intended for consumption outside of the metrics
// implementation.
int GetFieldTypeGroupPredictionQualityMetric(
    ServerFieldType field_type,
    AutofillMetrics::FieldTypeQualityMetric metric) {
  DCHECK_LT(metric, AutofillMetrics::NUM_FIELD_TYPE_QUALITY_METRICS);

  FieldTypeGroupForMetrics group = GROUP_AMBIGUOUS;
  switch (AutofillType(field_type).group()) {
    case FieldTypeGroup::kNoGroup:
      group = GROUP_AMBIGUOUS;
      break;

    case FieldTypeGroup::kName:
    case FieldTypeGroup::kNameBilling:
      group = GROUP_NAME;
      break;

    case FieldTypeGroup::kCompany:
      group = GROUP_COMPANY;
      break;

    case FieldTypeGroup::kAddressHome:
    case FieldTypeGroup::kAddressBilling:
      switch (AutofillType(field_type).GetStorableType()) {
        case ADDRESS_HOME_LINE1:
          group = GROUP_ADDRESS_LINE_1;
          break;
        case ADDRESS_HOME_LINE2:
          group = GROUP_ADDRESS_LINE_2;
          break;
        case ADDRESS_HOME_LINE3:
          group = GROUP_ADDRESS_LINE_3;
          break;
        case ADDRESS_HOME_APT_NUM:
          group = GROUP_ADDRESS_HOME_APT_NUM;
          break;
        case ADDRESS_HOME_STREET_ADDRESS:
          group = GROUP_STREET_ADDRESS;
          break;
        case ADDRESS_HOME_CITY:
          group = GROUP_ADDRESS_CITY;
          break;
        case ADDRESS_HOME_STATE:
          group = GROUP_ADDRESS_STATE;
          break;
        case ADDRESS_HOME_ZIP:
          group = GROUP_ADDRESS_ZIP;
          break;
        case ADDRESS_HOME_COUNTRY:
          group = GROUP_ADDRESS_COUNTRY;
          break;
        case ADDRESS_HOME_STREET_NAME:
          group = GROUP_ADDRESS_HOME_STREET_NAME;
          break;
        case ADDRESS_HOME_SORTING_CODE:
          group = GROUP_ADDRESS_HOME_SORTING_CODE;
          break;
        case ADDRESS_HOME_DEPENDENT_LOCALITY:
          group = GROUP_ADDRESS_HOME_DEPENDENT_LOCALITY;
          break;
        case ADDRESS_HOME_DEPENDENT_STREET_NAME:
          group = GROUP_ADDRESS_HOME_DEPENDENT_STREET_NAME;
          break;
        case ADDRESS_HOME_HOUSE_NUMBER:
          group = GROUP_ADDRESS_HOME_HOUSE_NUMBER;
          break;
        case ADDRESS_HOME_PREMISE_NAME:
          group = GROUP_ADDRESS_HOME_PREMISE_NAME;
          break;
        case ADDRESS_HOME_STREET_AND_DEPENDENT_STREET_NAME:
          group = GROUP_ADDRESS_HOME_STREET_AND_DEPENDENT_STREET_NAME;
          break;
        case ADDRESS_HOME_SUBPREMISE:
          group = GROUP_ADDRESS_HOME_SUBPREMISE;
          break;
        case ADDRESS_HOME_OTHER_SUBUNIT:
          group = GROUP_ADDRESS_HOME_OTHER_SUBUNIT;
          break;
        case ADDRESS_HOME_ADDRESS:
          group = GROUP_ADDRESS_HOME_ADDRESS;
          break;
        case ADDRESS_HOME_ADDRESS_WITH_NAME:
          group = GROUP_ADDRESS_HOME_ADDRESS_WITH_NAME;
          break;
        case ADDRESS_HOME_FLOOR:
          group = GROUP_ADDRESS_HOME_FLOOR;
          break;
        case UNKNOWN_TYPE:
          group = GROUP_UNKNOWN_TYPE;
          break;
        case NO_SERVER_DATA:
        case EMPTY_TYPE:
        case NAME_FIRST:
        case NAME_MIDDLE:
        case NAME_LAST:
        case NAME_MIDDLE_INITIAL:
        case NAME_FULL:
        case NAME_SUFFIX:
        case EMAIL_ADDRESS:
        case PHONE_HOME_NUMBER:
        case PHONE_HOME_CITY_CODE:
        case PHONE_HOME_COUNTRY_CODE:
        case PHONE_HOME_CITY_AND_NUMBER:
        case PHONE_HOME_WHOLE_NUMBER:
        case PHONE_FAX_NUMBER:
        case PHONE_FAX_CITY_CODE:
        case PHONE_FAX_COUNTRY_CODE:
        case PHONE_FAX_CITY_AND_NUMBER:
        case PHONE_FAX_WHOLE_NUMBER:
        case ADDRESS_BILLING_LINE1:
        case ADDRESS_BILLING_LINE2:
        case ADDRESS_BILLING_APT_NUM:
        case ADDRESS_BILLING_CITY:
        case ADDRESS_BILLING_STATE:
        case ADDRESS_BILLING_ZIP:
        case ADDRESS_BILLING_COUNTRY:
        case CREDIT_CARD_NAME_FULL:
        case CREDIT_CARD_NUMBER:
        case CREDIT_CARD_EXP_MONTH:
        case CREDIT_CARD_EXP_2_DIGIT_YEAR:
        case CREDIT_CARD_EXP_4_DIGIT_YEAR:
        case CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR:
        case CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR:
        case CREDIT_CARD_TYPE:
        case CREDIT_CARD_VERIFICATION_CODE:
        case COMPANY_NAME:
        case FIELD_WITH_DEFAULT_VALUE:
        case PHONE_BILLING_NUMBER:
        case PHONE_BILLING_CITY_CODE:
        case PHONE_BILLING_COUNTRY_CODE:
        case PHONE_BILLING_CITY_AND_NUMBER:
        case PHONE_BILLING_WHOLE_NUMBER:
        case NAME_BILLING_FIRST:
        case NAME_BILLING_MIDDLE:
        case NAME_BILLING_LAST:
        case NAME_BILLING_MIDDLE_INITIAL:
        case NAME_BILLING_FULL:
        case NAME_BILLING_SUFFIX:
        case MERCHANT_EMAIL_SIGNUP:
        case MERCHANT_PROMO_CODE:
        case PASSWORD:
        case ACCOUNT_CREATION_PASSWORD:
        case ADDRESS_BILLING_STREET_ADDRESS:
        case ADDRESS_BILLING_SORTING_CODE:
        case ADDRESS_BILLING_DEPENDENT_LOCALITY:
        case ADDRESS_BILLING_LINE3:
        case NOT_ACCOUNT_CREATION_PASSWORD:
        case USERNAME:
        case USERNAME_AND_EMAIL_ADDRESS:
        case NEW_PASSWORD:
        case PROBABLY_NEW_PASSWORD:
        case NOT_NEW_PASSWORD:
        case CREDIT_CARD_NAME_FIRST:
        case CREDIT_CARD_NAME_LAST:
        case PHONE_HOME_EXTENSION:
        case CONFIRMATION_PASSWORD:
        case AMBIGUOUS_TYPE:
        case SEARCH_TERM:
        case PRICE:
        case NOT_PASSWORD:
        case SINGLE_USERNAME:
        case NOT_USERNAME:
        case UPI_VPA:
        case NAME_LAST_FIRST:
        case NAME_LAST_CONJUNCTION:
        case NAME_LAST_SECOND:
        case NAME_HONORIFIC_PREFIX:
        case NAME_FULL_WITH_HONORIFIC_PREFIX:
        case MAX_VALID_FIELD_TYPE:
          NOTREACHED() << field_type << " type is not in that group.";
          group = GROUP_AMBIGUOUS;
          break;
      }
      break;

    case FieldTypeGroup::kEmail:
      group = GROUP_EMAIL;
      break;

    case FieldTypeGroup::kPhoneHome:
    case FieldTypeGroup::kPhoneBilling:
      group = GROUP_PHONE;
      break;

    case FieldTypeGroup::kCreditCard:
      switch (field_type) {
        case CREDIT_CARD_NAME_FULL:
        case CREDIT_CARD_NAME_FIRST:
        case CREDIT_CARD_NAME_LAST:
          group = GROUP_CREDIT_CARD_NAME;
          break;
        case CREDIT_CARD_NUMBER:
          group = GROUP_CREDIT_CARD_NUMBER;
          break;
        case CREDIT_CARD_TYPE:
          group = GROUP_CREDIT_CARD_TYPE;
          break;
        case CREDIT_CARD_EXP_MONTH:
        case CREDIT_CARD_EXP_2_DIGIT_YEAR:
        case CREDIT_CARD_EXP_4_DIGIT_YEAR:
        case CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR:
        case CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR:
          group = GROUP_CREDIT_CARD_DATE;
          break;
        case CREDIT_CARD_VERIFICATION_CODE:
          group = GROUP_CREDIT_CARD_VERIFICATION;
          break;
        default:
          NOTREACHED() << field_type << " has no group assigned (ambiguous)";
          group = GROUP_AMBIGUOUS;
          break;
      }
      break;

    case FieldTypeGroup::kPasswordField:
      group = GROUP_PASSWORD;
      break;

    case FieldTypeGroup::kUsernameField:
      group = GROUP_USERNAME;
      break;

    case FieldTypeGroup::kUnfillable:
      group = GROUP_UNFILLABLE;
      break;

    case FieldTypeGroup::kTransaction:
      NOTREACHED();
      break;
  }

  // Use bits 8-15 for the group and bits 0-7 for the metric.
  static_assert(AutofillMetrics::NUM_FIELD_TYPE_QUALITY_METRICS <= UINT8_MAX,
                "maximum field type quality metric must fit into 8 bits");
  static_assert(NUM_FIELD_TYPE_GROUPS_FOR_METRICS <= UINT8_MAX,
                "number of field type groups must fit into 8 bits");
  return (group << 8) | metric;
}

// This function encodes the integer value of a |ServerFieldType| and the
// metric value of an |AutofilledFieldUserEdtingStatus| into a 16 bit integer.
// The lower four bits are used to encode the editing status and the higher
// 12 bits are used to encode the field type.
int GetFieldTypeUserEditStatusMetric(
    ServerFieldType server_type,
    AutofillMetrics::AutofilledFieldUserEditingStatusMetric metric) {
  static_assert(ServerFieldType::MAX_VALID_FIELD_TYPE <= (UINT16_MAX >> 4),
                "Autofill::ServerTypes value needs more than 12 bits.");

  static_assert(
      static_cast<int>(
          AutofillMetrics::AutofilledFieldUserEditingStatusMetric::kMaxValue) <=
          (UINT16_MAX >> 12),
      "AutofillMetrics::AutofilledFieldUserEditingStatusMetric value needs "
      "more than 4 bits");

  return (server_type << 4) | static_cast<int>(metric);
}

namespace {

const char* GetQualityMetricPredictionSource(
    AutofillMetrics::QualityMetricPredictionSource source) {
  switch (source) {
    default:
    case AutofillMetrics::PREDICTION_SOURCE_UNKNOWN:
      NOTREACHED();
      return "Unknown";

    case AutofillMetrics::PREDICTION_SOURCE_HEURISTIC:
      return "Heuristic";
    case AutofillMetrics::PREDICTION_SOURCE_SERVER:
      return "Server";
    case AutofillMetrics::PREDICTION_SOURCE_OVERALL:
      return "Overall";
  }
}

const char* GetQualityMetricTypeSuffix(
    AutofillMetrics::QualityMetricType metric_type) {
  switch (metric_type) {
    default:
      NOTREACHED();
      FALLTHROUGH;
    case AutofillMetrics::TYPE_SUBMISSION:
      return "";
    case AutofillMetrics::TYPE_NO_SUBMISSION:
      return ".NoSubmission";
    case AutofillMetrics::TYPE_AUTOCOMPLETE_BASED:
      return ".BasedOnAutocomplete";
  }
}

// Given a set of |possible_types| for a field, select the best type to use as
// the "actual" field type when calculating metrics. If the |predicted_type| is
// among the |possible_types] then use that as the best type (i.e., the
// prediction is deemed to have been correct).
ServerFieldType GetActualFieldType(const ServerFieldTypeSet& possible_types,
                                   ServerFieldType predicted_type) {
  DCHECK_NE(possible_types.size(), 0u);

  if (possible_types.count(EMPTY_TYPE)) {
    DCHECK_EQ(possible_types.size(), 1u);
    return EMPTY_TYPE;
  }

  if (possible_types.count(UNKNOWN_TYPE)) {
    DCHECK_EQ(possible_types.size(), 1u);
    return UNKNOWN_TYPE;
  }

  if (possible_types.count(predicted_type))
    return predicted_type;

  // Collapse field types that Chrome treats as identical, e.g. home and
  // billing address fields.
  ServerFieldTypeSet collapsed_field_types;
  for (auto type : possible_types) {
    DCHECK_NE(type, EMPTY_TYPE);
    DCHECK_NE(type, UNKNOWN_TYPE);

    // A phone number that's only missing its country code is (for metrics
    // purposes) the same as the whole phone number.
    if (type == PHONE_HOME_CITY_AND_NUMBER)
      collapsed_field_types.insert(PHONE_HOME_WHOLE_NUMBER);
    else
      collapsed_field_types.insert(AutofillType(type).GetStorableType());
  }

  // Capture the field's type, if it is unambiguous.
  ServerFieldType actual_type = AMBIGUOUS_TYPE;
  if (collapsed_field_types.size() == 1)
    actual_type = *collapsed_field_types.begin();

  DVLOG(2) << "Inferred Type: "
           << AutofillType::ServerFieldTypeToString(actual_type);
  return actual_type;
}

// Check if the value of |field| is same as one of the previously autofilled
// values. This indicates a bad rationalization if |field| has
// only_fill_when_focued set to true.
bool DuplicatedFilling(const FormStructure& form, const AutofillField& field) {
  for (const auto& form_field : form) {
    if (field.value == form_field->value && form_field->is_autofilled)
      return true;
  }
  return false;
}

void LogPredictionQualityMetricsForFieldsOnlyFilledWhenFocused(
    const std::string& aggregate_histogram,
    const std::string& type_specific_histogram,
    const std::string& rationalization_quality_histogram,
    ServerFieldType predicted_type,
    ServerFieldType actual_type,
    bool is_empty,
    bool is_ambiguous,
    bool log_rationalization_metrics,
    const FormStructure& form,
    const AutofillField& field) {
  // If it is filled with values unknown, it is a true negative.
  if (actual_type == UNKNOWN_TYPE) {
    // Only log aggregate true negative; do not log type specific metrics
    // for UNKNOWN/EMPTY.
    DVLOG(2) << "TRUE NEGATIVE";
    base::UmaHistogramSparse(
        aggregate_histogram,
        (is_empty ? AutofillMetrics::TRUE_NEGATIVE_EMPTY
                  : (is_ambiguous ? AutofillMetrics::TRUE_NEGATIVE_AMBIGUOUS
                                  : AutofillMetrics::TRUE_NEGATIVE_UNKNOWN)));
    if (log_rationalization_metrics) {
      base::UmaHistogramSparse(
          rationalization_quality_histogram,
          (is_empty ? AutofillMetrics::RATIONALIZATION_GOOD
                    : AutofillMetrics::RATIONALIZATION_OK));
    }
    return;
  }

  // If it is filled with same type as predicted, it is a true positive. We
  // also log an RATIONALIZATION_BAD by checking if the filled value is filled
  // already in previous fields, this means autofill could have filled it
  // automatically if there has been no rationalization.
  if (predicted_type == actual_type) {
    DVLOG(2) << "TRUE POSITIVE";
    base::UmaHistogramSparse(aggregate_histogram,
                             AutofillMetrics::TRUE_POSITIVE);
    base::UmaHistogramSparse(type_specific_histogram,
                             GetFieldTypeGroupPredictionQualityMetric(
                                 actual_type, AutofillMetrics::TRUE_POSITIVE));
    if (log_rationalization_metrics) {
      bool duplicated_filling = DuplicatedFilling(form, field);
      base::UmaHistogramSparse(
          rationalization_quality_histogram,
          (duplicated_filling ? AutofillMetrics::RATIONALIZATION_BAD
                              : AutofillMetrics::RATIONALIZATION_OK));
    }
    return;
  }

  DVLOG(2) << "MISMATCH";
  // Here the prediction is wrong, but user has to provide some value still.
  // This should be a false negative.
  base::UmaHistogramSparse(aggregate_histogram,
                           AutofillMetrics::FALSE_NEGATIVE_MISMATCH);
  // Log FALSE_NEGATIVE_MISMATCH for predicted type if it did predicted
  // something but actual type is different.
  if (predicted_type != UNKNOWN_TYPE)
    base::UmaHistogramSparse(
        type_specific_histogram,
        GetFieldTypeGroupPredictionQualityMetric(
            predicted_type, AutofillMetrics::FALSE_NEGATIVE_MISMATCH));
  if (log_rationalization_metrics) {
    // Logging RATIONALIZATION_OK despite of type mismatch here because autofill
    // would have got it wrong with or without rationalization. Rationalization
    // here does not help, neither does it do any harm.
    base::UmaHistogramSparse(rationalization_quality_histogram,
                             AutofillMetrics::RATIONALIZATION_OK);
  }
  return;
}

void LogPredictionQualityMetricsForCommonFields(
    const std::string& aggregate_histogram,
    const std::string& type_specific_histogram,
    ServerFieldType predicted_type,
    ServerFieldType actual_type,
    bool is_empty,
    bool is_ambiguous) {
  // If the predicted and actual types match then it's either a true positive
  // or a true negative (if they are both unknown). Do not log type specific
  // true negatives (instead log a true positive for the "Ambiguous" type).
  if (predicted_type == actual_type) {
    if (actual_type == UNKNOWN_TYPE) {
      // Only log aggregate true negative; do not log type specific metrics
      // for UNKNOWN/EMPTY.
      DVLOG(2) << "TRUE NEGATIVE";
      base::UmaHistogramSparse(
          aggregate_histogram,
          (is_empty ? AutofillMetrics::TRUE_NEGATIVE_EMPTY
                    : (is_ambiguous ? AutofillMetrics::TRUE_NEGATIVE_AMBIGUOUS
                                    : AutofillMetrics::TRUE_NEGATIVE_UNKNOWN)));
      return;
    }

    DVLOG(2) << "TRUE POSITIVE";
    // Log both aggregate and type specific true positive if we correctly
    // predict that type with which the field was filled.
    base::UmaHistogramSparse(aggregate_histogram,
                             AutofillMetrics::TRUE_POSITIVE);
    base::UmaHistogramSparse(type_specific_histogram,
                             GetFieldTypeGroupPredictionQualityMetric(
                                 actual_type, AutofillMetrics::TRUE_POSITIVE));
    return;
  }

  // Note: At this point predicted_type != actual type
  // If actual type is UNKNOWN_TYPE then the prediction is a false positive.
  // Further specialize the type of false positive by whether the field was
  // empty or contained an unknown value.
  if (actual_type == UNKNOWN_TYPE) {
    DVLOG(2) << "FALSE POSITIVE";
    auto metric =
        (is_empty ? AutofillMetrics::FALSE_POSITIVE_EMPTY
                  : (is_ambiguous ? AutofillMetrics::FALSE_POSITIVE_AMBIGUOUS
                                  : AutofillMetrics::FALSE_POSITIVE_UNKNOWN));
    base::UmaHistogramSparse(aggregate_histogram, metric);
    base::UmaHistogramSparse(
        type_specific_histogram,
        GetFieldTypeGroupPredictionQualityMetric(predicted_type, metric));
    return;
  }

  // Note: At this point predicted_type != actual type, actual_type != UNKNOWN.
  // If predicted type is UNKNOWN_TYPE then the prediction is a false negative
  // unknown.
  if (predicted_type == UNKNOWN_TYPE) {
    DVLOG(2) << "FALSE NEGATIVE";
    base::UmaHistogramSparse(aggregate_histogram,
                             AutofillMetrics::FALSE_NEGATIVE_UNKNOWN);
    base::UmaHistogramSparse(
        type_specific_histogram,
        GetFieldTypeGroupPredictionQualityMetric(
            actual_type, AutofillMetrics::FALSE_NEGATIVE_UNKNOWN));
    return;
  }

  DVLOG(2) << "MISMATCH";

  // Note: At this point predicted_type != actual type, actual_type != UNKNOWN,
  //       predicted_type != UNKNOWN.
  // This is a mismatch. From the reference of the actual type, this is a false
  // negative (it was T, but predicted U). From the reference of the prediction,
  // this is a false positive (predicted it was T, but it was U).
  base::UmaHistogramSparse(aggregate_histogram,
                           AutofillMetrics::FALSE_NEGATIVE_MISMATCH);
  base::UmaHistogramSparse(
      type_specific_histogram,
      GetFieldTypeGroupPredictionQualityMetric(
          actual_type, AutofillMetrics::FALSE_NEGATIVE_MISMATCH));
  base::UmaHistogramSparse(
      type_specific_histogram,
      GetFieldTypeGroupPredictionQualityMetric(
          predicted_type, AutofillMetrics::FALSE_POSITIVE_MISMATCH));
}

// Logs field type prediction quality metrics.  The primary histogram name is
// constructed based on |prediction_source| The field-specific histogram names
// also incorporates the possible and predicted types for |field|. A suffix may
// be appended to the metric name, depending on |metric_type|.
void LogPredictionQualityMetrics(
    AutofillMetrics::QualityMetricPredictionSource prediction_source,
    ServerFieldType predicted_type,
    AutofillMetrics::FormInteractionsUkmLogger* form_interactions_ukm_logger,
    const FormStructure& form,
    const AutofillField& field,
    AutofillMetrics::QualityMetricType metric_type,
    bool log_rationalization_metrics) {
  // Generate histogram names.
  const char* source = GetQualityMetricPredictionSource(prediction_source);
  const char* suffix = GetQualityMetricTypeSuffix(metric_type);
  std::string raw_data_histogram =
      base::JoinString({"Autofill.FieldPrediction.", source, suffix}, "");
  std::string aggregate_histogram = base::JoinString(
      {"Autofill.FieldPredictionQuality.Aggregate.", source, suffix}, "");
  std::string type_specific_histogram = base::JoinString(
      {"Autofill.FieldPredictionQuality.ByFieldType.", source, suffix}, "");
  std::string rationalization_quality_histogram = base::JoinString(
      {"Autofill.RationalizationQuality.PhoneNumber", suffix}, "");

  const ServerFieldTypeSet& possible_types =
      metric_type == AutofillMetrics::TYPE_AUTOCOMPLETE_BASED
          ? ServerFieldTypeSet{AutofillType(field.html_type(),
                                            field.html_mode())
                                   .GetStorableType()}
          : field.possible_types();

  // Get the best type classification we can for the field.
  ServerFieldType actual_type =
      GetActualFieldType(possible_types, predicted_type);

  DVLOG(2) << "Predicted: "
           << AutofillType::ServerFieldTypeToString(predicted_type) << "; "
           << "Actual: " << AutofillType::ServerFieldTypeToString(actual_type);

  DCHECK_LE(predicted_type, UINT16_MAX);
  DCHECK_LE(actual_type, UINT16_MAX);
  base::UmaHistogramSparse(raw_data_histogram,
                           (predicted_type << 16) | actual_type);

  form_interactions_ukm_logger->LogFieldType(
      form.form_parsed_timestamp(), form.form_signature(),
      field.GetFieldSignature(), prediction_source, metric_type, predicted_type,
      actual_type);

  // NO_SERVER_DATA is the equivalent of predicting UNKNOWN.
  if (predicted_type == NO_SERVER_DATA)
    predicted_type = UNKNOWN_TYPE;

  // The actual type being EMPTY_TYPE is the same as UNKNOWN_TYPE for comparison
  // purposes, but remember whether or not it was empty for more precise logging
  // later.
  bool is_empty = (actual_type == EMPTY_TYPE);
  bool is_ambiguous = (actual_type == AMBIGUOUS_TYPE);
  if (is_empty || is_ambiguous)
    actual_type = UNKNOWN_TYPE;

  // Log metrics for a field that is |only_fill_when_focused|==true. Basically
  // autofill might have a field prediction but it also thinks it should not
  // be filled automatically unless user focused on the field. This requires
  // different metrics logging than normal fields.
  if (field.only_fill_when_focused()) {
    LogPredictionQualityMetricsForFieldsOnlyFilledWhenFocused(
        aggregate_histogram, type_specific_histogram,
        rationalization_quality_histogram, predicted_type, actual_type,
        is_empty, is_ambiguous, log_rationalization_metrics, form, field);
    return;
  }

  LogPredictionQualityMetricsForCommonFields(
      aggregate_histogram, type_specific_histogram, predicted_type, actual_type,
      is_empty, is_ambiguous);
}

}  // namespace

const int kMaxBucketsCount = 50;

// static
void AutofillMetrics::LogProfileSuggestionsMadeWithFormatter(
    bool made_with_formatter) {
  UMA_HISTOGRAM_BOOLEAN("Autofill.ProfileSuggestionsMadeWithFormatter",
                        made_with_formatter);
}

// static
void AutofillMetrics::LogSubmittedCardStateMetric(
    SubmittedCardStateMetric metric) {
  DCHECK_LT(metric, NUM_SUBMITTED_CARD_STATE_METRICS);
  UMA_HISTOGRAM_ENUMERATION("Autofill.SubmittedCardState", metric,
                            NUM_SUBMITTED_CARD_STATE_METRICS);
}

// static
void AutofillMetrics::LogSubmittedServerCardExpirationStatusMetric(
    SubmittedServerCardExpirationStatusMetric metric) {
  DCHECK_LT(metric, NUM_SUBMITTED_SERVER_CARD_EXPIRATION_STATUS_METRICS);
  UMA_HISTOGRAM_ENUMERATION(
      "Autofill.SubmittedServerCardExpirationStatus", metric,
      NUM_SUBMITTED_SERVER_CARD_EXPIRATION_STATUS_METRICS);
}

// static
void AutofillMetrics::LogCreditCardSaveNotOfferedDueToMaxStrikesMetric(
    SaveTypeMetric metric) {
  UMA_HISTOGRAM_ENUMERATION(
      "Autofill.StrikeDatabase.CreditCardSaveNotOfferedDueToMaxStrikes",
      metric);
}

// static
void AutofillMetrics::LogLocalCardMigrationNotOfferedDueToMaxStrikesMetric(
    SaveTypeMetric metric) {
  UMA_HISTOGRAM_ENUMERATION(
      "Autofill.StrikeDatabase.LocalCardMigrationNotOfferedDueToMaxStrikes",
      metric);
}

// static
void AutofillMetrics::LogUploadOfferedCardOriginMetric(
    UploadOfferedCardOriginMetric metric) {
  DCHECK_LT(metric, NUM_UPLOAD_OFFERED_CARD_ORIGIN_METRICS);
  UMA_HISTOGRAM_ENUMERATION("Autofill.UploadOfferedCardOrigin", metric,
                            NUM_UPLOAD_OFFERED_CARD_ORIGIN_METRICS);
}

// static
void AutofillMetrics::LogUploadAcceptedCardOriginMetric(
    UploadAcceptedCardOriginMetric metric) {
  DCHECK_LT(metric, NUM_UPLOAD_ACCEPTED_CARD_ORIGIN_METRICS);
  UMA_HISTOGRAM_ENUMERATION("Autofill.UploadAcceptedCardOrigin", metric,
                            NUM_UPLOAD_ACCEPTED_CARD_ORIGIN_METRICS);
}

// static
void AutofillMetrics::LogSaveCardCardholderNamePrefilled(bool prefilled) {
  UMA_HISTOGRAM_BOOLEAN("Autofill.SaveCardCardholderNamePrefilled", prefilled);
}

// static
void AutofillMetrics::LogSaveCardCardholderNameWasEdited(bool edited) {
  UMA_HISTOGRAM_BOOLEAN("Autofill.SaveCardCardholderNameWasEdited", edited);
}

// static
void AutofillMetrics::LogCardUnmaskAuthenticationSelectionDialogResultMetric(
    CardUnmaskAuthenticationSelectionDialogResultMetric metric) {
  DCHECK_LE(metric,
            CardUnmaskAuthenticationSelectionDialogResultMetric::kMaxValue);
  base::UmaHistogramEnumeration(
      "Autofill.CardUnmaskAuthenticationSelectionDialog.Result", metric);
}

// static
void AutofillMetrics::LogCardUnmaskAuthenticationSelectionDialogShown() {
  base::UmaHistogramBoolean(
      "Autofill.CardUnmaskAuthenticationSelectionDialog.Shown", true);
}

// static
void AutofillMetrics::LogCardUploadDecisionMetrics(
    int upload_decision_metrics) {
  DCHECK(upload_decision_metrics);
  DCHECK_LT(upload_decision_metrics, 1 << kNumCardUploadDecisionMetrics);

  for (int metric = 0; metric < kNumCardUploadDecisionMetrics; ++metric)
    if (upload_decision_metrics & (1 << metric))
      UMA_HISTOGRAM_ENUMERATION("Autofill.CardUploadDecisionMetric", metric,
                                kNumCardUploadDecisionMetrics);
}

// static
void AutofillMetrics::LogCreditCardInfoBarMetric(
    InfoBarMetric metric,
    bool is_uploading,
    AutofillClient::SaveCreditCardOptions options) {
  DCHECK_LT(metric, NUM_INFO_BAR_METRICS);

  std::string destination = is_uploading ? ".Server" : ".Local";
  base::UmaHistogramEnumeration("Autofill.CreditCardInfoBar" + destination,
                                metric, NUM_INFO_BAR_METRICS);
  if (options.should_request_name_from_user) {
    base::UmaHistogramEnumeration("Autofill.CreditCardInfoBar" + destination +
                                      ".RequestingCardholderName",
                                  metric, NUM_INFO_BAR_METRICS);
  }

  if (options.should_request_expiration_date_from_user) {
    base::UmaHistogramEnumeration("Autofill.CreditCardInfoBar" + destination +
                                      ".RequestingExpirationDate",
                                  metric, NUM_INFO_BAR_METRICS);
  }

  if (options.from_dynamic_change_form) {
    base::UmaHistogramEnumeration(
        "Autofill.CreditCardInfoBar" + destination + ".FromDynamicChangeForm",
        metric, NUM_INFO_BAR_METRICS);
  }

  if (options.has_non_focusable_field) {
    base::UmaHistogramEnumeration(
        "Autofill.CreditCardInfoBar" + destination + ".FromNonFocusableForm",
        metric, NUM_INFO_BAR_METRICS);
  }
}

// static
void AutofillMetrics::LogCreditCardFillingInfoBarMetric(InfoBarMetric metric) {
  DCHECK_LT(metric, NUM_INFO_BAR_METRICS);
  UMA_HISTOGRAM_ENUMERATION("Autofill.CreditCardFillingInfoBar", metric,
                            NUM_INFO_BAR_METRICS);
}

// static
void AutofillMetrics::LogSaveCardRequestExpirationDateReasonMetric(
    SaveCardRequestExpirationDateReasonMetric reason) {
  DCHECK_LE(reason, SaveCardRequestExpirationDateReasonMetric::kMaxValue);
  UMA_HISTOGRAM_ENUMERATION("Autofill.SaveCardRequestExpirationDateReason",
                            reason);
}

// static
void AutofillMetrics::LogSaveCardPromptOfferMetric(
    SaveCardPromptOfferMetric metric,
    bool is_uploading,
    bool is_reshow,
    AutofillClient::SaveCreditCardOptions options,
    security_state::SecurityLevel security_level,
    AutofillSyncSigninState sync_state) {
  DCHECK_LT(metric, NUM_SAVE_CARD_PROMPT_OFFER_METRICS);
  std::string base_histogram_name = "Autofill.SaveCreditCardPromptOffer";
  std::string destination = is_uploading ? ".Upload" : ".Local";
  std::string show = is_reshow ? ".Reshows" : ".FirstShow";
  std::string metric_with_destination_and_show =
      base_histogram_name + destination + show;
  base::UmaHistogramEnumeration(metric_with_destination_and_show, metric,
                                NUM_SAVE_CARD_PROMPT_OFFER_METRICS);

  base::UmaHistogramEnumeration(
      metric_with_destination_and_show + GetMetricsSyncStateSuffix(sync_state),
      metric, NUM_SAVE_CARD_PROMPT_OFFER_METRICS);

  if (options.should_request_name_from_user) {
    base::UmaHistogramEnumeration(
        metric_with_destination_and_show + ".RequestingCardholderName", metric,
        NUM_SAVE_CARD_PROMPT_OFFER_METRICS);
  }
  if (options.should_request_expiration_date_from_user) {
    base::UmaHistogramEnumeration(
        metric_with_destination_and_show + ".RequestingExpirationDate", metric,
        NUM_SAVE_CARD_PROMPT_OFFER_METRICS);
  }
  if (options.has_non_focusable_field) {
    base::UmaHistogramEnumeration(
        metric_with_destination_and_show + ".FromNonFocusableForm", metric,
        NUM_SAVE_CARD_PROMPT_OFFER_METRICS);
  }
  if (options.from_dynamic_change_form) {
    base::UmaHistogramEnumeration(
        metric_with_destination_and_show + ".FromDynamicChangeForm", metric,
        NUM_SAVE_CARD_PROMPT_OFFER_METRICS);
  }

  if (security_level != security_state::SecurityLevel::SECURITY_LEVEL_COUNT) {
    base::UmaHistogramEnumeration(
        security_state::GetSecurityLevelHistogramName(
            base_histogram_name + destination, security_level),
        metric, NUM_SAVE_CARD_PROMPT_OFFER_METRICS);
  }
}

// static
void AutofillMetrics::LogSaveCardPromptResultMetric(
    SaveCardPromptResultMetric metric,
    bool is_uploading,
    bool is_reshow,
    AutofillClient::SaveCreditCardOptions options,
    security_state::SecurityLevel security_level,
    AutofillSyncSigninState sync_state) {
  DCHECK_LT(metric, NUM_SAVE_CARD_PROMPT_RESULT_METRICS);
  std::string base_histogram_name = "Autofill.SaveCreditCardPromptResult";
  std::string destination = is_uploading ? ".Upload" : ".Local";
  std::string show = is_reshow ? ".Reshows" : ".FirstShow";
  std::string metric_with_destination_and_show =
      base_histogram_name + destination + show;

  base::UmaHistogramEnumeration(metric_with_destination_and_show, metric,
                                NUM_SAVE_CARD_PROMPT_RESULT_METRICS);

  base::UmaHistogramEnumeration(
      metric_with_destination_and_show + GetMetricsSyncStateSuffix(sync_state),
      metric, NUM_SAVE_CARD_PROMPT_RESULT_METRICS);

  if (options.should_request_name_from_user) {
    base::UmaHistogramEnumeration(
        metric_with_destination_and_show + ".RequestingCardholderName", metric,
        NUM_SAVE_CARD_PROMPT_RESULT_METRICS);
  }
  if (options.should_request_expiration_date_from_user) {
    base::UmaHistogramEnumeration(
        metric_with_destination_and_show + ".RequestingExpirationDate", metric,
        NUM_SAVE_CARD_PROMPT_RESULT_METRICS);
  }
  if (options.has_non_focusable_field) {
    base::UmaHistogramEnumeration(
        metric_with_destination_and_show + ".FromNonFocusableForm", metric,
        NUM_SAVE_CARD_PROMPT_RESULT_METRICS);
  }
  if (options.from_dynamic_change_form) {
    base::UmaHistogramEnumeration(
        metric_with_destination_and_show + ".FromDynamicChangeForm", metric,
        NUM_SAVE_CARD_PROMPT_RESULT_METRICS);
  }

  if (security_level != security_state::SecurityLevel::SECURITY_LEVEL_COUNT) {
    base::UmaHistogramEnumeration(
        security_state::GetSecurityLevelHistogramName(
            base_histogram_name + destination, security_level),
        metric, NUM_SAVE_CARD_PROMPT_RESULT_METRICS);
  }
}

// static
void AutofillMetrics::LogCreditCardUploadLegalMessageLinkClicked() {
  base::RecordAction(base::UserMetricsAction(
      "Autofill_CreditCardUpload_LegalMessageLinkClicked"));
}

// static
void AutofillMetrics::LogCreditCardUploadFeedbackMetric(
    CreditCardUploadFeedbackMetric metric) {
  DCHECK_LT(metric, NUM_CREDIT_CARD_UPLOAD_FEEDBACK_METRICS);
  UMA_HISTOGRAM_ENUMERATION("Autofill.CreditCardUploadFeedback", metric,
                            NUM_CREDIT_CARD_UPLOAD_FEEDBACK_METRICS);
}

// static
void AutofillMetrics::LogManageCardsPromptMetric(ManageCardsPromptMetric metric,
                                                 bool is_upload_save) {
  DCHECK_LT(metric, NUM_MANAGE_CARDS_PROMPT_METRICS);
  std::string destination = is_upload_save ? ".Upload" : ".Local";
  std::string metric_with_destination =
      "Autofill.ManageCardsPrompt" + destination;
  base::UmaHistogramEnumeration(metric_with_destination, metric,
                                NUM_MANAGE_CARDS_PROMPT_METRICS);
}

// static
void AutofillMetrics::LogScanCreditCardPromptMetric(
    ScanCreditCardPromptMetric metric) {
  DCHECK_LT(metric, NUM_SCAN_CREDIT_CARD_PROMPT_METRICS);
  UMA_HISTOGRAM_ENUMERATION("Autofill.ScanCreditCardPrompt", metric,
                            NUM_SCAN_CREDIT_CARD_PROMPT_METRICS);
}

// static
void AutofillMetrics::LogScanCreditCardCompleted(
    const base::TimeDelta& duration,
    bool completed) {
  std::string suffix = completed ? "Completed" : "Cancelled";
  base::UmaHistogramLongTimes("Autofill.ScanCreditCard.Duration_" + suffix,
                              duration);
  UMA_HISTOGRAM_BOOLEAN("Autofill.ScanCreditCard.Completed", completed);
}

// static
void AutofillMetrics::LogLocalCardMigrationDecisionMetric(
    LocalCardMigrationDecisionMetric metric) {
  UMA_HISTOGRAM_ENUMERATION("Autofill.LocalCardMigrationDecision", metric);
}

// static
void AutofillMetrics::LogLocalCardMigrationBubbleOfferMetric(
    LocalCardMigrationBubbleOfferMetric metric,
    bool is_reshow) {
  DCHECK_LT(metric, NUM_LOCAL_CARD_MIGRATION_BUBBLE_OFFER_METRICS);
  std::string histogram_name = "Autofill.LocalCardMigrationBubbleOffer.";
  histogram_name += is_reshow ? "Reshows" : "FirstShow";
  base::UmaHistogramEnumeration(histogram_name, metric,
                                NUM_LOCAL_CARD_MIGRATION_BUBBLE_OFFER_METRICS);
}

// static
void AutofillMetrics::LogLocalCardMigrationBubbleResultMetric(
    LocalCardMigrationBubbleResultMetric metric,
    bool is_reshow) {
  DCHECK_LT(metric, NUM_LOCAL_CARD_MIGRATION_BUBBLE_RESULT_METRICS);
  std::string suffix = is_reshow ? ".Reshows" : ".FirstShow";
  base::UmaHistogramEnumeration(
      "Autofill.LocalCardMigrationBubbleResult" + suffix, metric,
      NUM_LOCAL_CARD_MIGRATION_BUBBLE_RESULT_METRICS);
}

// static
void AutofillMetrics::LogLocalCardMigrationDialogOfferMetric(
    LocalCardMigrationDialogOfferMetric metric) {
  DCHECK_LT(metric, NUM_LOCAL_CARD_MIGRATION_DIALOG_OFFER_METRICS);
  std::string histogram_name = "Autofill.LocalCardMigrationDialogOffer";
  base::UmaHistogramEnumeration(histogram_name, metric,
                                NUM_LOCAL_CARD_MIGRATION_DIALOG_OFFER_METRICS);
}

// static
void AutofillMetrics::LogLocalCardMigrationDialogUserInteractionMetric(
    const base::TimeDelta& duration,
    LocalCardMigrationDialogUserInteractionMetric metric) {
  DCHECK_LT(metric, NUM_LOCAL_CARD_MIGRATION_DIALOG_USER_INTERACTION_METRICS);
  base::UmaHistogramEnumeration(
      "Autofill.LocalCardMigrationDialogUserInteraction", metric,
      NUM_LOCAL_CARD_MIGRATION_DIALOG_USER_INTERACTION_METRICS);

  // Do not log duration metrics for
  // LOCAL_CARD_MIGRATION_DIALOG_DELETE_CARD_ICON_CLICKED, as it can happen
  // multiple times in one dialog.
  std::string suffix;
  switch (metric) {
    case LOCAL_CARD_MIGRATION_DIALOG_CLOSED_SAVE_BUTTON_CLICKED:
      suffix = "Accepted";
      break;
    case LOCAL_CARD_MIGRATION_DIALOG_CLOSED_CANCEL_BUTTON_CLICKED:
      suffix = "Denied";
      break;
    case LOCAL_CARD_MIGRATION_DIALOG_CLOSED_VIEW_CARDS_BUTTON_CLICKED:
    case LOCAL_CARD_MIGRATION_DIALOG_CLOSED_DONE_BUTTON_CLICKED:
      suffix = "Closed";
      break;
    default:
      return;
  }

  base::UmaHistogramLongTimes(
      "Autofill.LocalCardMigrationDialogActiveDuration." + suffix, duration);
}

// static
void AutofillMetrics::LogLocalCardMigrationDialogUserSelectionPercentageMetric(
    int selected,
    int total) {
  UMA_HISTOGRAM_PERCENTAGE(
      "Autofill.LocalCardMigrationDialogUserSelectionPercentage",
      100 * selected / total);
}

// static
void AutofillMetrics::LogLocalCardMigrationPromptMetric(
    LocalCardMigrationOrigin local_card_migration_origin,
    LocalCardMigrationPromptMetric metric) {
  DCHECK_LT(metric, NUM_LOCAL_CARD_MIGRATION_PROMPT_METRICS);
  std::string histogram_name = "Autofill.LocalCardMigrationOrigin.";
  // Switch to different sub-histogram depending on local card migration origin.
  switch (local_card_migration_origin) {
    case LocalCardMigrationOrigin::UseOfLocalCard:
      histogram_name += "UseOfLocalCard";
      break;
    case LocalCardMigrationOrigin::UseOfServerCard:
      histogram_name += "UseOfServerCard";
      break;
    case LocalCardMigrationOrigin::SettingsPage:
      histogram_name += "SettingsPage";
      break;
    default:
      NOTREACHED();
      return;
  }
  base::UmaHistogramEnumeration(histogram_name, metric,
                                NUM_LOCAL_CARD_MIGRATION_PROMPT_METRICS);
}

// static
void AutofillMetrics::LogOfferNotificationBubbleOfferMetric(
    AutofillOfferData::OfferType offer_type,
    bool is_reshow) {
  std::string histogram_name = "Autofill.OfferNotificationBubbleOffer.";
  // Switch to different sub-histogram depending on offer type being displayed.
  switch (offer_type) {
    case AutofillOfferData::OfferType::GPAY_CARD_LINKED_OFFER:
      histogram_name += "CardLinkedOffer";
      break;
    case AutofillOfferData::OfferType::FREE_LISTING_COUPON_OFFER:
      histogram_name += "FreeListingCouponOffer";
      break;
    case AutofillOfferData::OfferType::UNKNOWN:
      NOTREACHED();
      return;
  }
  base::UmaHistogramBoolean(histogram_name, is_reshow);
}

// static
void AutofillMetrics::LogOfferNotificationBubbleResultMetric(
    AutofillOfferData::OfferType offer_type,
    OfferNotificationBubbleResultMetric metric,
    bool is_reshow) {
  DCHECK_LE(metric, OfferNotificationBubbleResultMetric::kMaxValue);
  std::string histogram_name = "Autofill.OfferNotificationBubbleResult.";
  // Switch to different sub-histogram depending on offer type being displayed.
  switch (offer_type) {
    case AutofillOfferData::OfferType::GPAY_CARD_LINKED_OFFER:
      histogram_name += "CardLinkedOffer.";
      break;
    case AutofillOfferData::OfferType::FREE_LISTING_COUPON_OFFER:
      histogram_name += "FreeListingCouponOffer.";
      break;
    case AutofillOfferData::OfferType::UNKNOWN:
      NOTREACHED();
      return;
  }
  // Add subhistogram for |is_reshow| decision.
  histogram_name += is_reshow ? "Reshows" : "FirstShow";
  base::UmaHistogramEnumeration(histogram_name, metric);
}

// static
void AutofillMetrics::LogOfferNotificationBubblePromoCodeButtonClicked(
    AutofillOfferData::OfferType offer_type) {
  std::string histogram_name =
      "Autofill.OfferNotificationBubblePromoCodeButtonClicked.";
  // Switch to different sub-histogram depending on offer type being displayed.
  // Card-linked offers do not have a promo code button.
  switch (offer_type) {
    case AutofillOfferData::OfferType::FREE_LISTING_COUPON_OFFER:
      histogram_name += "FreeListingCouponOffer";
      break;
    case AutofillOfferData::OfferType::GPAY_CARD_LINKED_OFFER:
    case AutofillOfferData::OfferType::UNKNOWN:
      NOTREACHED();
      return;
  }
  base::UmaHistogramBoolean(histogram_name, true);
}

// static
void AutofillMetrics::LogOfferNotificationBubbleSuppressed(
    AutofillOfferData::OfferType offer_type) {
  std::string histogram_name = "Autofill.OfferNotificationBubbleSuppressed.";
  // Switch to different sub-histogram depending on offer type being suppressed.
  // Card-linked offers will not be suppressed.
  switch (offer_type) {
    case AutofillOfferData::OfferType::FREE_LISTING_COUPON_OFFER:
      histogram_name += "FreeListingCouponOffer";
      break;
    case AutofillOfferData::OfferType::GPAY_CARD_LINKED_OFFER:
    case AutofillOfferData::OfferType::UNKNOWN:
      NOTREACHED();
      return;
  }
  base::UmaHistogramBoolean(histogram_name, true);
}

// static
void AutofillMetrics::LogOfferNotificationInfoBarDeepLinkClicked() {
  base::RecordAction(base::UserMetricsAction(
      "Autofill_OfferNotificationInfoBar_DeepLinkClicked"));
}

// static
void AutofillMetrics::LogOfferNotificationInfoBarResultMetric(
    OfferNotificationInfoBarResultMetric metric) {
  DCHECK_LE(metric, OfferNotificationInfoBarResultMetric::kMaxValue);
  base::UmaHistogramEnumeration(
      "Autofill.OfferNotificationInfoBarResult.CardLinkedOffer", metric);
}

void AutofillMetrics::LogOfferNotificationInfoBarShown() {
  base::UmaHistogramBoolean(
      "Autofill.OfferNotificationInfoBarOffer.CardLinkedOffer", true);
}

void AutofillMetrics::LogProgressDialogResultMetric(bool is_canceled_by_user) {
  base::UmaHistogramBoolean("Autofill.ProgressDialog.CardUnmask.Result",
                            is_canceled_by_user);
}

void AutofillMetrics::LogProgressDialogShown() {
  base::UmaHistogramBoolean("Autofill.ProgressDialog.CardUnmask.Shown", true);
}

// static
void AutofillMetrics::LogVirtualCardManualFallbackBubbleShown(bool is_reshow) {
  base::UmaHistogramBoolean("Autofill.VirtualCardManualFallbackBubble.Shown",
                            is_reshow);
}

// static
void AutofillMetrics::LogVirtualCardManualFallbackBubbleResultMetric(
    VirtualCardManualFallbackBubbleResultMetric metric,
    bool is_reshow) {
  static const char first_show[] =
      "Autofill.VirtualCardManualFallbackBubble.Result.FirstShow";
  static const char reshows[] =
      "Autofill.VirtualCardManualFallbackBubble.Result.Reshows";
  base::UmaHistogramEnumeration(is_reshow ? reshows : first_show, metric);
}

// static
void AutofillMetrics::LogVirtualCardManualFallbackBubbleFieldClicked(
    VirtualCardManualFallbackBubbleFieldClickedMetric metric) {
  base::UmaHistogramEnumeration(
      "Autofill.VirtualCardManualFallbackBubble.FieldClicked", metric);
}

// static
void AutofillMetrics::LogSaveCardWithFirstAndLastNameOffered(bool is_local) {
  std::string histogram_name = "Autofill.SaveCardWithFirstAndLastNameOffered.";
  histogram_name += is_local ? "Local" : "Server";
  base::UmaHistogramBoolean(histogram_name, true);
}

// static
void AutofillMetrics::LogSaveCardWithFirstAndLastNameComplete(bool is_local) {
  std::string histogram_name = "Autofill.SaveCardWithFirstAndLastNameComplete.";
  histogram_name += is_local ? "Local" : "Server";
  base::UmaHistogramBoolean(histogram_name, true);
}

// static
void AutofillMetrics::LogCardUnmaskDurationAfterWebauthn(
    const base::TimeDelta& duration,
    AutofillClient::PaymentsRpcResult result,
    AutofillClient::PaymentsRpcCardType card_type) {
  std::string result_suffix;

  switch (result) {
    case AutofillClient::PaymentsRpcResult::kSuccess:
      result_suffix = "Success";
      break;
    case AutofillClient::PaymentsRpcResult::kTryAgainFailure:
    case AutofillClient::PaymentsRpcResult::kPermanentFailure:
      result_suffix = "Failure";
      break;
    case AutofillClient::PaymentsRpcResult::kNetworkError:
      result_suffix = "NetworkError";
      break;
    case AutofillClient::PaymentsRpcResult::kVcnRetrievalTryAgainFailure:
    case AutofillClient::PaymentsRpcResult::kVcnRetrievalPermanentFailure:
      result_suffix = "VcnRetrievalFailure";
      break;
    case AutofillClient::PaymentsRpcResult::kNone:
      NOTREACHED();
      return;
  }

  base::UmaHistogramLongTimes("Autofill.BetterAuth.CardUnmaskDuration.Fido",
                              duration);
  base::UmaHistogramLongTimes("Autofill.BetterAuth.CardUnmaskDuration.Fido" +
                                  GetCreditCardTypeSuffix(card_type) + "." +
                                  result_suffix,
                              duration);
}

// static
void AutofillMetrics::LogCardUnmaskPreflightCalled() {
  UMA_HISTOGRAM_BOOLEAN("Autofill.BetterAuth.CardUnmaskPreflightCalled", true);
}

// static
void AutofillMetrics::LogCardUnmaskPreflightDuration(
    const base::TimeDelta& duration) {
  base::UmaHistogramLongTimes("Autofill.BetterAuth.CardUnmaskPreflightDuration",
                              duration);
}

// static
void AutofillMetrics::LogServerCardUnmaskAttempt(
    AutofillClient::PaymentsRpcCardType card_type) {
  base::UmaHistogramBoolean("Autofill.ServerCardUnmask" +
                                GetCreditCardTypeSuffix(card_type) + ".Attempt",
                            true);
}

// static
void AutofillMetrics::LogServerCardUnmaskResult(
    ServerCardUnmaskResult unmask_result,
    AutofillClient::PaymentsRpcCardType card_type,
    VirtualCardUnmaskFlowType flow_type) {
  std::string flow_type_suffix;
  switch (flow_type) {
    case VirtualCardUnmaskFlowType::kUnspecified:
      flow_type_suffix = ".UnspecifiedFlowType";
      break;
    case VirtualCardUnmaskFlowType::kFidoOnly:
      flow_type_suffix = ".Fido";
      break;
    case VirtualCardUnmaskFlowType::kOtpOnly:
      flow_type_suffix = ".Otp";
      break;
    case VirtualCardUnmaskFlowType::kOtpFallbackFromFido:
      flow_type_suffix = ".OtpFallbackFromFido";
      break;
  }

  base::UmaHistogramEnumeration("Autofill.ServerCardUnmask" +
                                    GetCreditCardTypeSuffix(card_type) +
                                    ".Result" + flow_type_suffix,
                                unmask_result);
}

// static
void AutofillMetrics::LogServerCardUnmaskFormSubmission(
    AutofillClient::PaymentsRpcCardType card_type) {
  base::UmaHistogramBoolean("Autofill.ServerCardUnmask" +
                                GetCreditCardTypeSuffix(card_type) +
                                ".FormSubmission",
                            true);
}

// static
void AutofillMetrics::LogWebauthnOptChangeCalled(
    bool request_to_opt_in,
    bool is_checkout_flow,
    WebauthnOptInParameters metric) {
  if (!request_to_opt_in) {
    DCHECK(!is_checkout_flow);
    base::UmaHistogramBoolean(
        "Autofill.BetterAuth.OptOutCalled.FromSettingsPage", true);
    return;
  }

  std::string histogram_name = "Autofill.BetterAuth.OptInCalled.";
  histogram_name += is_checkout_flow ? "FromCheckoutFlow" : "FromSettingsPage";
  base::UmaHistogramEnumeration(histogram_name, metric);
}

// static
void AutofillMetrics::LogWebauthnOptInPromoShown(bool is_checkout_flow) {
  std::string suffix =
      is_checkout_flow ? "FromCheckoutFlow" : "FromSettingsPage";
  base::UmaHistogramBoolean("Autofill.BetterAuth.OptInPromoShown." + suffix,
                            true);
}

// static
void AutofillMetrics::LogWebauthnOptInPromoUserDecision(
    bool is_checkout_flow,
    WebauthnOptInPromoUserDecisionMetric metric) {
  std::string suffix =
      (is_checkout_flow ? "FromCheckoutFlow" : "FromSettingsPage");
  base::UmaHistogramEnumeration(
      "Autofill.BetterAuth.OptInPromoUserDecision." + suffix, metric);
}

// static
void AutofillMetrics::LogCardUnmaskTypeDecision(
    CardUnmaskTypeDecisionMetric metric) {
  base::UmaHistogramEnumeration("Autofill.BetterAuth.CardUnmaskTypeDecision",
                                metric);
}

// static
void AutofillMetrics::LogUserPerceivedLatencyOnCardSelection(
    PreflightCallEvent event,
    bool fido_auth_enabled) {
  std::string histogram_name =
      "Autofill.BetterAuth.UserPerceivedLatencyOnCardSelection.";
  histogram_name += fido_auth_enabled ? "OptedIn" : "OptedOut";
  base::UmaHistogramEnumeration(histogram_name, event);
}

// static
void AutofillMetrics::LogUserPerceivedLatencyOnCardSelectionDuration(
    const base::TimeDelta duration) {
  base::UmaHistogramLongTimes(
      "Autofill.BetterAuth.UserPerceivedLatencyOnCardSelection.OptedIn."
      "Duration",
      duration);
}

// static
void AutofillMetrics::LogUserPerceivedLatencyOnCardSelectionTimedOut(
    bool did_time_out) {
  base::UmaHistogramBoolean(
      "Autofill.BetterAuth.UserPerceivedLatencyOnCardSelection.OptedIn."
      "TimedOutCvcFallback",
      did_time_out);
}

void AutofillMetrics::LogUserVerifiabilityCheckDuration(
    const base::TimeDelta& duration) {
  base::UmaHistogramLongTimes(
      "Autofill.BetterAuth.UserVerifiabilityCheckDuration", duration);
}

// static
void AutofillMetrics::LogWebauthnResult(WebauthnFlowEvent event,
                                        WebauthnResultMetric metric) {
  std::string histogram_name = "Autofill.BetterAuth.WebauthnResult.";
  switch (event) {
    case WebauthnFlowEvent::kImmediateAuthentication:
      histogram_name += "ImmediateAuthentication";
      break;
    case WebauthnFlowEvent::kAuthenticationAfterCvc:
      histogram_name += "AuthenticationAfterCVC";
      break;
    case WebauthnFlowEvent::kCheckoutOptIn:
      histogram_name += "CheckoutOptIn";
      break;
    case WebauthnFlowEvent::kSettingsPageOptIn:
      histogram_name += "SettingsPageOptIn";
      break;
  }
  base::UmaHistogramEnumeration(histogram_name, metric);
}

// static
void AutofillMetrics::LogUnmaskPromptEvent(UnmaskPromptEvent event,
                                           bool has_valid_nickname) {
  base::UmaHistogramEnumeration("Autofill.UnmaskPrompt.Events", event,
                                NUM_UNMASK_PROMPT_EVENTS);
  if (has_valid_nickname) {
    base::UmaHistogramEnumeration("Autofill.UnmaskPrompt.Events.WithNickname",
                                  event, NUM_UNMASK_PROMPT_EVENTS);
  }
}

// static
void AutofillMetrics::LogCardholderNameFixFlowPromptEvent(
    CardholderNameFixFlowPromptEvent event) {
  UMA_HISTOGRAM_ENUMERATION("Autofill.CardholderNameFixFlowPrompt.Events",
                            event, NUM_CARDHOLDER_NAME_FIXFLOW_PROMPT_EVENTS);
}

// static
void AutofillMetrics::LogExpirationDateFixFlowPromptEvent(
    ExpirationDateFixFlowPromptEvent event) {
  UMA_HISTOGRAM_ENUMERATION("Autofill.ExpirationDateFixFlowPrompt.Events",
                            event);
}

// static
void AutofillMetrics::LogExpirationDateFixFlowPromptShown() {
  UMA_HISTOGRAM_BOOLEAN("Autofill.ExpirationDateFixFlowPromptShown", true);
}

// static
void AutofillMetrics::LogUnmaskPromptEventDuration(
    const base::TimeDelta& duration,
    UnmaskPromptEvent close_event,
    bool has_valid_nickname) {
  std::string suffix;
  switch (close_event) {
    case UNMASK_PROMPT_CLOSED_NO_ATTEMPTS:
      suffix = ".NoAttempts";
      break;
    case UNMASK_PROMPT_CLOSED_FAILED_TO_UNMASK_RETRIABLE_FAILURE:
    case UNMASK_PROMPT_CLOSED_FAILED_TO_UNMASK_NON_RETRIABLE_FAILURE:
      suffix = ".Failure";
      break;
    case UNMASK_PROMPT_CLOSED_ABANDON_UNMASKING:
      suffix = ".AbandonUnmasking";
      break;
    case UNMASK_PROMPT_UNMASKED_CARD_FIRST_ATTEMPT:
    case UNMASK_PROMPT_UNMASKED_CARD_AFTER_FAILED_ATTEMPTS:
      suffix = ".Success";
      break;
    default:
      NOTREACHED();
      return;
  }
  base::UmaHistogramLongTimes("Autofill.UnmaskPrompt.Duration", duration);
  base::UmaHistogramLongTimes("Autofill.UnmaskPrompt.Duration" + suffix,
                              duration);

  if (has_valid_nickname) {
    base::UmaHistogramLongTimes("Autofill.UnmaskPrompt.Duration.WithNickname",
                                duration);
    base::UmaHistogramLongTimes(
        "Autofill.UnmaskPrompt.Duration" + suffix + ".WithNickname", duration);
  }
}

// static
void AutofillMetrics::LogTimeBeforeAbandonUnmasking(
    const base::TimeDelta& duration,
    bool has_valid_nickname) {
  base::UmaHistogramLongTimes(
      "Autofill.UnmaskPrompt.TimeBeforeAbandonUnmasking", duration);
  if (has_valid_nickname) {
    base::UmaHistogramLongTimes(
        "Autofill.UnmaskPrompt.TimeBeforeAbandonUnmasking.WithNickname",
        duration);
  }
}

// static
void AutofillMetrics::LogRealPanResult(
    AutofillClient::PaymentsRpcResult result,
    AutofillClient::PaymentsRpcCardType card_type) {
  PaymentsRpcResult metric_result;
  switch (result) {
    case AutofillClient::PaymentsRpcResult::kSuccess:
      metric_result = PAYMENTS_RESULT_SUCCESS;
      break;
    case AutofillClient::PaymentsRpcResult::kTryAgainFailure:
      metric_result = PAYMENTS_RESULT_TRY_AGAIN_FAILURE;
      break;
    case AutofillClient::PaymentsRpcResult::kPermanentFailure:
      metric_result = PAYMENTS_RESULT_PERMANENT_FAILURE;
      break;
    case AutofillClient::PaymentsRpcResult::kNetworkError:
      metric_result = PAYMENTS_RESULT_NETWORK_ERROR;
      break;
    case AutofillClient::PaymentsRpcResult::kVcnRetrievalTryAgainFailure:
      DCHECK_EQ(card_type, AutofillClient::PaymentsRpcCardType::kVirtualCard);
      metric_result = PAYMENTS_RESULT_VCN_RETRIEVAL_TRY_AGAIN_FAILURE;
      break;
    case AutofillClient::PaymentsRpcResult::kVcnRetrievalPermanentFailure:
      DCHECK_EQ(card_type, AutofillClient::PaymentsRpcCardType::kVirtualCard);
      metric_result = PAYMENTS_RESULT_VCN_RETRIEVAL_PERMANENT_FAILURE;
      break;
    case AutofillClient::PaymentsRpcResult::kNone:
      NOTREACHED();
      return;
  }

  std::string card_type_suffix;
  switch (card_type) {
    case AutofillClient::PaymentsRpcCardType::kServerCard:
      card_type_suffix = "ServerCard";
      break;
    case AutofillClient::PaymentsRpcCardType::kVirtualCard:
      card_type_suffix = "VirtualCard";
      break;
    case AutofillClient::PaymentsRpcCardType::kUnknown:
      NOTREACHED();
      return;
  }

  base::UmaHistogramEnumeration("Autofill.UnmaskPrompt.GetRealPanResult",
                                metric_result);

  base::UmaHistogramEnumeration(
      "Autofill.UnmaskPrompt.GetRealPanResult." + card_type_suffix,
      metric_result);
}

// static
void AutofillMetrics::LogRealPanDuration(
    const base::TimeDelta& duration,
    AutofillClient::PaymentsRpcResult result,
    AutofillClient::PaymentsRpcCardType card_type) {
  std::string result_suffix;
  std::string card_type_suffix;

  switch (card_type) {
    case AutofillClient::PaymentsRpcCardType::kServerCard:
      card_type_suffix = "ServerCard";
      break;
    case AutofillClient::PaymentsRpcCardType::kVirtualCard:
      card_type_suffix = "VirtualCard";
      break;
    case AutofillClient::PaymentsRpcCardType::kUnknown:
      NOTREACHED();
      return;
  }

  switch (result) {
    case AutofillClient::PaymentsRpcResult::kSuccess:
      result_suffix = "Success";
      break;
    case AutofillClient::PaymentsRpcResult::kTryAgainFailure:
    case AutofillClient::PaymentsRpcResult::kPermanentFailure:
      result_suffix = "Failure";
      break;
    case AutofillClient::PaymentsRpcResult::kVcnRetrievalTryAgainFailure:
    case AutofillClient::PaymentsRpcResult::kVcnRetrievalPermanentFailure:
      DCHECK_EQ(card_type, AutofillClient::PaymentsRpcCardType::kVirtualCard);
      result_suffix = "VcnRetrievalFailure";
      break;
    case AutofillClient::PaymentsRpcResult::kNetworkError:
      result_suffix = "NetworkError";
      break;
    case AutofillClient::PaymentsRpcResult::kNone:
      NOTREACHED();
      return;
  }

  base::UmaHistogramLongTimes("Autofill.UnmaskPrompt.GetRealPanDuration",
                              duration);
  base::UmaHistogramLongTimes("Autofill.UnmaskPrompt.GetRealPanDuration." +
                                  card_type_suffix + "." + result_suffix,
                              duration);
}

// static
void AutofillMetrics::LogUnmaskingDuration(
    const base::TimeDelta& duration,
    AutofillClient::PaymentsRpcResult result,
    AutofillClient::PaymentsRpcCardType card_type) {
  std::string result_suffix;
  std::string card_type_suffix;

  switch (card_type) {
    case AutofillClient::PaymentsRpcCardType::kServerCard:
      card_type_suffix = "ServerCard";
      break;
    case AutofillClient::PaymentsRpcCardType::kVirtualCard:
      card_type_suffix = "VirtualCard";
      break;
    case AutofillClient::PaymentsRpcCardType::kUnknown:
      NOTREACHED();
      return;
  }

  switch (result) {
    case AutofillClient::PaymentsRpcResult::kSuccess:
      result_suffix = "Success";
      break;
    case AutofillClient::PaymentsRpcResult::kTryAgainFailure:
    case AutofillClient::PaymentsRpcResult::kPermanentFailure:
      result_suffix = "Failure";
      break;
    case AutofillClient::PaymentsRpcResult::kVcnRetrievalTryAgainFailure:
    case AutofillClient::PaymentsRpcResult::kVcnRetrievalPermanentFailure:
      DCHECK_EQ(card_type, AutofillClient::PaymentsRpcCardType::kVirtualCard);
      result_suffix = "VcnRetrievalFailure";
      break;
    case AutofillClient::PaymentsRpcResult::kNetworkError:
      result_suffix = "NetworkError";
      break;
    case AutofillClient::PaymentsRpcResult::kNone:
      NOTREACHED();
      return;
  }
  base::UmaHistogramLongTimes("Autofill.UnmaskPrompt.UnmaskingDuration",
                              duration);
  base::UmaHistogramLongTimes("Autofill.UnmaskPrompt.UnmaskingDuration." +
                                  card_type_suffix + "." + result_suffix,
                              duration);
}

// static
void AutofillMetrics::LogDeveloperEngagementMetric(
    DeveloperEngagementMetric metric) {
  DCHECK_LT(metric, NUM_DEVELOPER_ENGAGEMENT_METRICS);
  UMA_HISTOGRAM_ENUMERATION("Autofill.DeveloperEngagement", metric,
                            NUM_DEVELOPER_ENGAGEMENT_METRICS);
}

void AutofillMetrics::LogHeuristicPredictionQualityMetrics(
    FormInteractionsUkmLogger* form_interactions_ukm_logger,
    const FormStructure& form,
    const AutofillField& field,
    QualityMetricType metric_type) {
  LogPredictionQualityMetrics(
      PREDICTION_SOURCE_HEURISTIC,
      AutofillType(field.heuristic_type()).GetStorableType(),
      form_interactions_ukm_logger, form, field, metric_type,
      false /*log_rationalization_metrics*/);
}

void AutofillMetrics::LogServerPredictionQualityMetrics(
    FormInteractionsUkmLogger* form_interactions_ukm_logger,
    const FormStructure& form,
    const AutofillField& field,
    QualityMetricType metric_type) {
  LogPredictionQualityMetrics(
      PREDICTION_SOURCE_SERVER,
      AutofillType(field.server_type()).GetStorableType(),
      form_interactions_ukm_logger, form, field, metric_type,
      false /*log_rationalization_metrics*/);
}

void AutofillMetrics::LogOverallPredictionQualityMetrics(
    FormInteractionsUkmLogger* form_interactions_ukm_logger,
    const FormStructure& form,
    const AutofillField& field,
    QualityMetricType metric_type) {
  LogPredictionQualityMetrics(
      PREDICTION_SOURCE_OVERALL, field.Type().GetStorableType(),
      form_interactions_ukm_logger, form, field, metric_type,
      true /*log_rationalization_metrics*/);
}

void AutofillMetrics::LogEditedAutofilledFieldAtSubmission(
    FormInteractionsUkmLogger* form_interactions_ukm_logger,
    const FormStructure& form,
    const AutofillField& field) {
  const std::string aggregate_histogram =
      "Autofill.EditedAutofilledFieldAtSubmission.Aggregate";
  const std::string type_specific_histogram =
      "Autofill.EditedAutofilledFieldAtSubmission.ByFieldType";

  AutofilledFieldUserEditingStatusMetric editing_metric =
      field.previously_autofilled()
          ? AutofilledFieldUserEditingStatusMetric::AUTOFILLED_FIELD_WAS_EDITED
          : AutofilledFieldUserEditingStatusMetric::
                AUTOFILLED_FIELD_WAS_NOT_EDITED;

  // Record the aggregated UMA statistics.
  base::UmaHistogramEnumeration(aggregate_histogram, editing_metric);

  // Record the type specific UMA statistics.
  base::UmaHistogramSparse(type_specific_histogram,
                           GetFieldTypeUserEditStatusMetric(
                               field.Type().GetStorableType(), editing_metric));

  // Record the UMA statistics spliced by the autocomplete attribute value.
  FormType form_type = FieldTypeGroupToFormType(field.Type().group());
  if (form_type == FormType::kAddressForm ||
      form_type == FormType::kCreditCardForm) {
    bool autocomplete_off = field.autocomplete_attribute == "off";
    const std::string autocomplete_histogram = base::StrCat(
        {"Autofill.Autocomplete.", autocomplete_off ? "Off" : "NotOff",
         ".EditedAutofilledFieldAtSubmission.",
         form_type == FormType::kAddressForm ? "Address" : "CreditCard"});
    base::UmaHistogramEnumeration(autocomplete_histogram, editing_metric);
  }

  // If the field was edited, record the event to UKM.
  if (editing_metric ==
      AutofilledFieldUserEditingStatusMetric::AUTOFILLED_FIELD_WAS_EDITED) {
    form_interactions_ukm_logger->LogEditedAutofilledFieldAtSubmission(form,
                                                                       field);
  }
}

// static
void AutofillMetrics::LogServerQueryMetric(ServerQueryMetric metric) {
  DCHECK_LT(metric, NUM_SERVER_QUERY_METRICS);
  UMA_HISTOGRAM_ENUMERATION("Autofill.ServerQueryResponse", metric,
                            NUM_SERVER_QUERY_METRICS);
}

// static
void AutofillMetrics::LogUserHappinessMetric(
    UserHappinessMetric metric,
    FieldTypeGroup field_type_group,
    security_state::SecurityLevel security_level,
    uint32_t profile_form_bitmask) {
  LogUserHappinessMetric(metric, {FieldTypeGroupToFormType(field_type_group)},
                         security_level, profile_form_bitmask);
}

// static
void AutofillMetrics::LogUserHappinessMetric(
    UserHappinessMetric metric,
    const DenseSet<FormType>& form_types,
    security_state::SecurityLevel security_level,
    uint32_t profile_form_bitmask) {
  DCHECK_LT(metric, NUM_USER_HAPPINESS_METRICS);
  UMA_HISTOGRAM_ENUMERATION("Autofill.UserHappiness", metric,
                            NUM_USER_HAPPINESS_METRICS);
  if (base::Contains(form_types, FormType::kCreditCardForm)) {
    UMA_HISTOGRAM_ENUMERATION("Autofill.UserHappiness.CreditCard", metric,
                              NUM_USER_HAPPINESS_METRICS);
    LogUserHappinessBySecurityLevel(metric, FormType::kCreditCardForm,
                                    security_level);
  }
  if (base::Contains(form_types, FormType::kAddressForm)) {
    UMA_HISTOGRAM_ENUMERATION("Autofill.UserHappiness.Address", metric,
                              NUM_USER_HAPPINESS_METRICS);
    if (metric != AutofillMetrics::FORMS_LOADED) {
      LogUserHappinessByProfileFormType(metric, profile_form_bitmask);
    }
    LogUserHappinessBySecurityLevel(metric, FormType::kAddressForm,
                                    security_level);
  }
  if (base::Contains(form_types, FormType::kPasswordForm)) {
    UMA_HISTOGRAM_ENUMERATION("Autofill.UserHappiness.Password", metric,
                              NUM_USER_HAPPINESS_METRICS);
    LogUserHappinessBySecurityLevel(metric, FormType::kPasswordForm,
                                    security_level);
  }
  if (base::Contains(form_types, FormType::kUnknownFormType)) {
    UMA_HISTOGRAM_ENUMERATION("Autofill.UserHappiness.Unknown", metric,
                              NUM_USER_HAPPINESS_METRICS);
    LogUserHappinessBySecurityLevel(metric, FormType::kUnknownFormType,
                                    security_level);
  }
}

// static
void AutofillMetrics::LogUserHappinessBySecurityLevel(
    UserHappinessMetric metric,
    FormType form_type,
    security_state::SecurityLevel security_level) {
  if (security_level == security_state::SecurityLevel::SECURITY_LEVEL_COUNT) {
    return;
  }

  std::string histogram_name = "Autofill.UserHappiness.";
  switch (form_type) {
    case FormType::kCreditCardForm:
      histogram_name += "CreditCard";
      break;

    case FormType::kAddressForm:
      histogram_name += "Address";
      break;

    case FormType::kPasswordForm:
      histogram_name += "Password";
      break;

    case FormType::kUnknownFormType:
      histogram_name += "Unknown";
      break;

    default:
      NOTREACHED();
      return;
  }

  base::UmaHistogramEnumeration(security_state::GetSecurityLevelHistogramName(
                                    histogram_name, security_level),
                                metric, NUM_USER_HAPPINESS_METRICS);
}

// static
void AutofillMetrics::LogUserHappinessByProfileFormType(
    UserHappinessMetric metric,
    uint32_t profile_form_bitmask) {
  base::UmaHistogramEnumeration(
      "Autofill.UserHappiness.Address" +
          data_util::GetSuffixForProfileFormType(profile_form_bitmask),
      metric, NUM_USER_HAPPINESS_METRICS);

  if (data_util::ContainsAddress(profile_form_bitmask) &&
      (data_util::ContainsPhone(profile_form_bitmask) ||
       data_util::ContainsEmail(profile_form_bitmask)))
    base::UmaHistogramEnumeration(
        "Autofill.UserHappiness.Address.AddressPlusContact", metric,
        NUM_USER_HAPPINESS_METRICS);
}

// static
void AutofillMetrics::LogFormFillDurationFromLoadWithAutofill(
    const base::TimeDelta& duration) {
  LogFormFillDuration("Autofill.FillDuration.FromLoad.WithAutofill", duration);
}

// static
void AutofillMetrics::LogFormFillDurationFromLoadWithoutAutofill(
    const base::TimeDelta& duration) {
  LogFormFillDuration("Autofill.FillDuration.FromLoad.WithoutAutofill",
                      duration);
}

// static
void AutofillMetrics::LogFormFillDurationFromLoadForOneTimeCode(
    const base::TimeDelta& duration) {
  LogFormFillDuration("Autofill.WebOTP.OneTimeCode.FillDuration.FromLoad",
                      duration);
}

// static
void AutofillMetrics::LogFormFillDurationFromInteraction(
    const DenseSet<FormType>& form_types,
    bool used_autofill,
    const base::TimeDelta& duration) {
  std::string parent_metric;
  if (used_autofill) {
    parent_metric = "Autofill.FillDuration.FromInteraction.WithAutofill";
  } else {
    parent_metric = "Autofill.FillDuration.FromInteraction.WithoutAutofill";
  }
  LogFormFillDuration(parent_metric, duration);
  if (base::Contains(form_types, FormType::kCreditCardForm)) {
    LogFormFillDuration(parent_metric + ".CreditCard", duration);
  }
  if (base::Contains(form_types, FormType::kAddressForm)) {
    LogFormFillDuration(parent_metric + ".Address", duration);
  }
  if (base::Contains(form_types, FormType::kPasswordForm)) {
    LogFormFillDuration(parent_metric + ".Password", duration);
  }
  if (base::Contains(form_types, FormType::kUnknownFormType)) {
    LogFormFillDuration(parent_metric + ".Unknown", duration);
  }
}

// static
void AutofillMetrics::LogFormFillDurationFromInteractionForOneTimeCode(
    const base::TimeDelta& duration) {
  LogFormFillDuration(
      "Autofill.WebOTP.OneTimeCode.FillDuration.FromInteraction", duration);
}

// static
void AutofillMetrics::LogFormFillDuration(const std::string& metric,
                                          const base::TimeDelta& duration) {
  base::UmaHistogramCustomTimes(metric, duration, base::Milliseconds(100),
                                base::Minutes(10), 50);
}

// static
void AutofillMetrics::LogIsAutofillEnabledAtStartup(bool enabled) {
  UMA_HISTOGRAM_BOOLEAN("Autofill.IsEnabled.Startup", enabled);
}

// static
void AutofillMetrics::LogIsAutofillProfileEnabledAtStartup(bool enabled) {
  UMA_HISTOGRAM_BOOLEAN("Autofill.Address.IsEnabled.Startup", enabled);
}

// static
void AutofillMetrics::LogIsAutofillCreditCardEnabledAtStartup(bool enabled) {
  UMA_HISTOGRAM_BOOLEAN("Autofill.CreditCard.IsEnabled.Startup", enabled);
}

// static
void AutofillMetrics::LogIsAutofillEnabledAtPageLoad(
    bool enabled,
    AutofillSyncSigninState sync_state) {
  std::string name("Autofill.IsEnabled.PageLoad");
  UMA_HISTOGRAM_BOOLEAN(name, enabled);
  base::UmaHistogramBoolean(name + GetMetricsSyncStateSuffix(sync_state),
                            enabled);
}

// static
void AutofillMetrics::LogIsAutofillProfileEnabledAtPageLoad(
    bool enabled,
    AutofillSyncSigninState sync_state) {
  std::string name("Autofill.Address.IsEnabled.PageLoad");
  UMA_HISTOGRAM_BOOLEAN(name, enabled);
  base::UmaHistogramBoolean(name + GetMetricsSyncStateSuffix(sync_state),
                            enabled);
}

// static
void AutofillMetrics::LogIsAutofillCreditCardEnabledAtPageLoad(
    bool enabled,
    AutofillSyncSigninState sync_state) {
  std::string name("Autofill.CreditCard.IsEnabled.PageLoad");
  UMA_HISTOGRAM_BOOLEAN(name, enabled);
  base::UmaHistogramBoolean(name + GetMetricsSyncStateSuffix(sync_state),
                            enabled);
}

// static
void AutofillMetrics::LogStoredProfileCount(size_t num_profiles) {
  UMA_HISTOGRAM_COUNTS_1M("Autofill.StoredProfileCount", num_profiles);
}

// static
void AutofillMetrics::LogStoredProfileDisusedCount(size_t num_profiles) {
  UMA_HISTOGRAM_COUNTS_1000("Autofill.StoredProfileDisusedCount", num_profiles);
}

// static
void AutofillMetrics::LogStoredProfileDaysSinceLastUse(size_t days) {
  UMA_HISTOGRAM_COUNTS_1000("Autofill.DaysSinceLastUse.StoredProfile", days);
}

// static
void AutofillMetrics::LogStoredCreditCardMetrics(
    const std::vector<std::unique_ptr<CreditCard>>& local_cards,
    const std::vector<std::unique_ptr<CreditCard>>& server_cards,
    size_t server_card_count_with_card_art_image,
    base::TimeDelta disused_data_threshold) {
  size_t num_local_cards = 0;
  size_t num_local_cards_with_nickname = 0;
  size_t num_masked_cards = 0;
  size_t num_masked_cards_with_nickname = 0;
  size_t num_unmasked_cards = 0;
  size_t num_disused_local_cards = 0;
  size_t num_disused_masked_cards = 0;
  size_t num_disused_unmasked_cards = 0;

  // Concatenate the local and server cards into one big collection of raw
  // CreditCard pointers.
  std::vector<const CreditCard*> credit_cards;
  credit_cards.reserve(local_cards.size() + server_cards.size());
  for (const auto* collection : {&local_cards, &server_cards}) {
    for (const auto& card : *collection) {
      credit_cards.push_back(card.get());
    }
  }

  // Iterate over all of the cards and gather metrics.
  const base::Time now = AutofillClock::Now();
  for (const CreditCard* card : credit_cards) {
    const base::TimeDelta time_since_last_use = now - card->use_date();
    const int days_since_last_use = time_since_last_use.InDays();
    const int disused_delta =
        (time_since_last_use > disused_data_threshold) ? 1 : 0;
    UMA_HISTOGRAM_COUNTS_1000("Autofill.DaysSinceLastUse.StoredCreditCard",
                              days_since_last_use);
    switch (card->record_type()) {
      case CreditCard::LOCAL_CARD:
        UMA_HISTOGRAM_COUNTS_1000(
            "Autofill.DaysSinceLastUse.StoredCreditCard.Local",
            days_since_last_use);
        num_local_cards += 1;
        num_disused_local_cards += disused_delta;
        if (card->HasNonEmptyValidNickname())
          num_local_cards_with_nickname += 1;
        break;
      case CreditCard::MASKED_SERVER_CARD:
        UMA_HISTOGRAM_COUNTS_1000(
            "Autofill.DaysSinceLastUse.StoredCreditCard.Server",
            days_since_last_use);
        UMA_HISTOGRAM_COUNTS_1000(
            "Autofill.DaysSinceLastUse.StoredCreditCard.Server.Masked",
            days_since_last_use);
        num_masked_cards += 1;
        num_disused_masked_cards += disused_delta;
        if (card->HasNonEmptyValidNickname())
          num_masked_cards_with_nickname += 1;
        break;
      case CreditCard::FULL_SERVER_CARD:
        UMA_HISTOGRAM_COUNTS_1000(
            "Autofill.DaysSinceLastUse.StoredCreditCard.Server",
            days_since_last_use);
        UMA_HISTOGRAM_COUNTS_1000(
            "Autofill.DaysSinceLastUse.StoredCreditCard.Server.Unmasked",
            days_since_last_use);
        num_unmasked_cards += 1;
        num_disused_unmasked_cards += disused_delta;
        break;
      case CreditCard::VIRTUAL_CARD:
        // This card type is not persisted in Chrome.
        NOTREACHED();
        break;
    }
  }

  // Calculate some summary info.
  const size_t num_server_cards = num_masked_cards + num_unmasked_cards;
  const size_t num_cards = num_local_cards + num_server_cards;
  const size_t num_disused_server_cards =
      num_disused_masked_cards + num_disused_unmasked_cards;
  const size_t num_disused_cards =
      num_disused_local_cards + num_disused_server_cards;

  // Log the overall counts.
  UMA_HISTOGRAM_COUNTS_1000("Autofill.StoredCreditCardCount", num_cards);
  UMA_HISTOGRAM_COUNTS_1000("Autofill.StoredCreditCardCount.Local",
                            num_local_cards);
  UMA_HISTOGRAM_COUNTS_1000("Autofill.StoredCreditCardCount.Local.WithNickname",
                            num_local_cards_with_nickname);
  UMA_HISTOGRAM_COUNTS_1000("Autofill.StoredCreditCardCount.Server",
                            num_server_cards);
  UMA_HISTOGRAM_COUNTS_1000("Autofill.StoredCreditCardCount.Server.Masked",
                            num_masked_cards);
  UMA_HISTOGRAM_COUNTS_1000(
      "Autofill.StoredCreditCardCount.Server.Masked.WithNickname",
      num_masked_cards_with_nickname);
  UMA_HISTOGRAM_COUNTS_1000("Autofill.StoredCreditCardCount.Server.Unmasked",
                            num_unmasked_cards);

  // For card types held by the user, log how many are disused.
  if (num_cards) {
    UMA_HISTOGRAM_COUNTS_1000("Autofill.StoredCreditCardDisusedCount",
                              num_disused_cards);
  }
  if (num_local_cards) {
    UMA_HISTOGRAM_COUNTS_1000("Autofill.StoredCreditCardDisusedCount.Local",
                              num_disused_local_cards);
  }
  if (num_server_cards) {
    UMA_HISTOGRAM_COUNTS_1000("Autofill.StoredCreditCardDisusedCount.Server",
                              num_disused_server_cards);
  }
  if (num_masked_cards) {
    UMA_HISTOGRAM_COUNTS_1000(
        "Autofill.StoredCreditCardDisusedCount.Server.Masked",
        num_disused_masked_cards);
  }
  if (num_unmasked_cards) {
    UMA_HISTOGRAM_COUNTS_1000(
        "Autofill.StoredCreditCardDisusedCount.Server.Unmasked",
        num_disused_unmasked_cards);
  }

  // Log the number of server cards that are enrolled with virtual cards.
  size_t virtual_card_enabled_card_count = base::ranges::count_if(
      server_cards, [](const std::unique_ptr<CreditCard>& card) {
        return card->virtual_card_enrollment_state() ==
               CreditCard::VirtualCardEnrollmentState::ENROLLED;
      });
  base::UmaHistogramCounts1000(
      "Autofill.StoredCreditCardCount.Server.WithVirtualCardMetadata",
      virtual_card_enabled_card_count);

  // Log the number of server cards that have valid customized art images.
  base::UmaHistogramCounts1000(
      "Autofill.StoredCreditCardCount.Server.WithCardArtImage",
      server_card_count_with_card_art_image);
}

// static
void AutofillMetrics::LogStoredOfferMetrics(
    const std::vector<std::unique_ptr<AutofillOfferData>>& offers) {
  base::UmaHistogramCounts1000("Autofill.Offer.StoredOfferCount",
                               offers.size());

  for (const std::unique_ptr<AutofillOfferData>& offer : offers) {
    base::UmaHistogramCounts1000(
        "Autofill.Offer.StoredOfferRelatedMerchantCount",
        offer->merchant_origins.size());
    base::UmaHistogramCounts1000("Autofill.Offer.StoredOfferRelatedCardCount",
                                 offer->eligible_instrument_id.size());
  }
}

// static
void AutofillMetrics::LogSyncedOfferDataBeingValid(bool valid) {
  base::UmaHistogramBoolean("Autofill.Offer.SyncedOfferDataBeingValid", valid);
}

// static
void AutofillMetrics::LogNumberOfCreditCardsSuppressedForDisuse(
    size_t num_cards) {
  UMA_HISTOGRAM_COUNTS_1000("Autofill.CreditCardsSuppressedForDisuse",
                            num_cards);
}

// static
void AutofillMetrics::LogNumberOfCreditCardsDeletedForDisuse(size_t num_cards) {
  UMA_HISTOGRAM_COUNTS_1000("Autofill.CreditCardsDeletedForDisuse", num_cards);
}

// static
void AutofillMetrics::LogHiddenOrPresentationalSelectFieldsFilled() {
  UMA_HISTOGRAM_BOOLEAN("Autofill.HiddenOrPresentationalSelectFieldsFilled",
                        true);
}

// static
void AutofillMetrics::LogNumberOfProfilesAtAutofillableFormSubmission(
    size_t num_profiles) {
  UMA_HISTOGRAM_COUNTS_1M(
      "Autofill.StoredProfileCountAtAutofillableFormSubmission", num_profiles);
}

// static
void AutofillMetrics::LogNumberOfAddressesSuppressedForDisuse(
    size_t num_profiles) {
  UMA_HISTOGRAM_COUNTS_1000("Autofill.AddressesSuppressedForDisuse",
                            num_profiles);
}

// static
void AutofillMetrics::LogNumberOfAddressesDeletedForDisuse(
    size_t num_profiles) {
  UMA_HISTOGRAM_COUNTS_1000("Autofill.AddressesDeletedForDisuse", num_profiles);
}

// static
void AutofillMetrics::LogAddressSuggestionsCount(size_t num_suggestions) {
  UMA_HISTOGRAM_COUNTS_1M("Autofill.AddressSuggestionsCount", num_suggestions);
}

// static
void AutofillMetrics::LogAutofillSuggestionAcceptedIndex(int index,
                                                         PopupType popup_type,
                                                         bool off_the_record) {
  base::UmaHistogramSparse("Autofill.SuggestionAcceptedIndex",
                           std::min(index, kMaxBucketsCount));

  if (popup_type == PopupType::kCreditCards) {
    base::UmaHistogramSparse("Autofill.SuggestionAcceptedIndex.CreditCard",
                             std::min(index, kMaxBucketsCount));
  } else if (popup_type == PopupType::kAddresses ||
             popup_type == PopupType::kPersonalInformation) {
    base::UmaHistogramSparse("Autofill.SuggestionAcceptedIndex.Profile",
                             std::min(index, kMaxBucketsCount));
  } else {
    base::UmaHistogramSparse("Autofill.SuggestionAcceptedIndex.Other",
                             std::min(index, kMaxBucketsCount));
  }

  base::RecordAction(base::UserMetricsAction("Autofill_SelectedSuggestion"));

  base::UmaHistogramBoolean("Autofill.SuggestionAccepted.OffTheRecord",
                            off_the_record);
}

// static
void AutofillMetrics::LogAutofillPopupHidingReason(PopupHidingReason reason) {
  base::UmaHistogramEnumeration("Autofill.PopupHidingReason", reason);
}

// static
void AutofillMetrics::LogAutofillFormCleared() {
  base::RecordAction(base::UserMetricsAction("Autofill_ClearedForm"));
}

// static
void AutofillMetrics::LogNumberOfEditedAutofilledFields(
    size_t num_edited_autofilled_fields,
    bool observed_submission) {
  if (observed_submission) {
    UMA_HISTOGRAM_COUNTS_1000(
        "Autofill.NumberOfEditedAutofilledFieldsAtSubmission",
        num_edited_autofilled_fields);
  } else {
    UMA_HISTOGRAM_COUNTS_1000(
        "Autofill.NumberOfEditedAutofilledFieldsAtSubmission.NoSubmission",
        num_edited_autofilled_fields);
  }
}

// static
void AutofillMetrics::LogServerResponseHasDataForForm(bool has_data) {
  UMA_HISTOGRAM_BOOLEAN("Autofill.ServerResponseHasDataForForm", has_data);
}

// static
void AutofillMetrics::LogProfileActionOnFormSubmitted(
    AutofillProfileAction action) {
  UMA_HISTOGRAM_ENUMERATION("Autofill.ProfileActionOnFormSubmitted", action,
                            AUTOFILL_PROFILE_ACTION_ENUM_SIZE);
}

// static
void AutofillMetrics::LogAutofillFormSubmittedState(
    AutofillFormSubmittedState state,
    bool is_for_credit_card,
    bool has_upi_vpa_field,
    const DenseSet<FormType>& form_types,
    const base::TimeTicks& form_parsed_timestamp,
    FormSignature form_signature,
    AutofillMetrics::FormInteractionsUkmLogger* form_interactions_ukm_logger) {
  UMA_HISTOGRAM_ENUMERATION("Autofill.FormSubmittedState", state,
                            AUTOFILL_FORM_SUBMITTED_STATE_ENUM_SIZE);

  switch (state) {
    case NON_FILLABLE_FORM_OR_NEW_DATA:
      base::RecordAction(
          base::UserMetricsAction("Autofill_FormSubmitted_NonFillable"));
      break;

    case FILLABLE_FORM_AUTOFILLED_ALL:
      base::RecordAction(
          base::UserMetricsAction("Autofill_FormSubmitted_FilledAll"));
      break;

    case FILLABLE_FORM_AUTOFILLED_SOME:
      base::RecordAction(
          base::UserMetricsAction("Autofill_FormSubmitted_FilledSome"));
      break;

    case FILLABLE_FORM_AUTOFILLED_NONE_DID_SHOW_SUGGESTIONS:
      base::RecordAction(base::UserMetricsAction(
          "Autofill_FormSubmitted_FilledNone_SuggestionsShown"));
      break;

    case FILLABLE_FORM_AUTOFILLED_NONE_DID_NOT_SHOW_SUGGESTIONS:
      base::RecordAction(base::UserMetricsAction(
          "Autofill_FormSubmitted_FilledNone_SuggestionsNotShown"));
      break;

    default:
      NOTREACHED();
      break;
  }
  form_interactions_ukm_logger->LogFormSubmitted(
      is_for_credit_card, has_upi_vpa_field, form_types, state,
      form_parsed_timestamp, form_signature);
}

// static
void AutofillMetrics::LogAutofillPerfectFilling(bool is_address,
                                                bool perfect_filling) {
  if (is_address) {
    UMA_HISTOGRAM_BOOLEAN("Autofill.PerfectFilling.Addresses", perfect_filling);
  } else {
    UMA_HISTOGRAM_BOOLEAN("Autofill.PerfectFilling.CreditCards",
                          perfect_filling);
  }
}

// static
void AutofillMetrics::LogNumberOfFramesWithDetectedFields(size_t num_frames) {
  if (num_frames == 0)
    return;
  base::UmaHistogramCounts100(
      "Autofill.Iframes.NumberOfFramesWithDetectedFields", num_frames);
}

// static
void AutofillMetrics::LogNumberOfFramesWithDetectedCreditCardFields(
    size_t num_frames) {
  if (num_frames == 0)
    return;
  base::UmaHistogramCounts100(
      "Autofill.Iframes.NumberOfFramesWithDetectedCreditCardFields",
      num_frames);
}

// static
void AutofillMetrics::LogNumberOfFramesWithAutofilledCreditCardFields(
    size_t num_frames) {
  if (num_frames == 0)
    return;
  base::UmaHistogramCounts100(
      "Autofill.Iframes.NumberOfFramesWithAutofilledCreditCardFields",
      num_frames);
}

// static
void AutofillMetrics::LogCreditCardSeamlessFills(
    const ServerFieldTypeSet& autofilled_types) {
  bool name = autofilled_types.contains(CREDIT_CARD_NAME_FULL) ||
              (autofilled_types.contains(CREDIT_CARD_NAME_FIRST) &&
               autofilled_types.contains(CREDIT_CARD_NAME_LAST));
  bool number = autofilled_types.contains(CREDIT_CARD_NUMBER);
  bool exp = autofilled_types.contains(CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR) ||
             autofilled_types.contains(CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR) ||
             (autofilled_types.contains(CREDIT_CARD_EXP_MONTH) &&
              (autofilled_types.contains(CREDIT_CARD_EXP_2_DIGIT_YEAR) ||
               autofilled_types.contains(CREDIT_CARD_EXP_4_DIGIT_YEAR)));
  bool cvc = autofilled_types.contains(CREDIT_CARD_VERIFICATION_CODE);
  CreditCardSeamlessFillMetric emit;
  if (name && number && exp && cvc) {
    emit = CreditCardSeamlessFillMetric::kFullFill;
  } else if (!name && number && exp && cvc) {
    emit = CreditCardSeamlessFillMetric::kOptionalNameMissing;
  } else if (name && number && exp && !cvc) {
    emit = CreditCardSeamlessFillMetric::kOptionalCvcMissing;
  } else if (!name && number && exp && !cvc) {
    emit = CreditCardSeamlessFillMetric::kOptionalNameAndCvcMissing;
  } else if (name && number && !exp && cvc) {
    emit = CreditCardSeamlessFillMetric::kFullFillButExpDateMissing;
  } else {
    emit = CreditCardSeamlessFillMetric::kPartialFill;
  }
  base::UmaHistogramEnumeration("Autofill.CreditCard.SeamlessFills", emit);
}

// static
void AutofillMetrics::LogCreditCardNumberFills(
    const ServerFieldTypeSet& autofilled_types) {
  bool emit = autofilled_types.contains(CREDIT_CARD_NUMBER);
  base::UmaHistogramBoolean("Autofill.CreditCard.NumberFills", emit);
}

// static
void AutofillMetrics::LogDetermineHeuristicTypesTiming(
    const base::TimeDelta& duration) {
  UMA_HISTOGRAM_TIMES("Autofill.Timing.DetermineHeuristicTypes", duration);
}

// static
void AutofillMetrics::LogParseFormTiming(const base::TimeDelta& duration) {
  UMA_HISTOGRAM_TIMES("Autofill.Timing.ParseForm", duration);
}

// static
void AutofillMetrics::LogNumberOfProfilesConsideredForDedupe(
    size_t num_considered) {
  // A maximum of 50 is enforced to reduce the number of generated buckets.
  UMA_HISTOGRAM_COUNTS_1000(
      "Autofill.NumberOfProfilesConsideredForDedupe",
      std::min(static_cast<int>(num_considered), kMaxBucketsCount));
}

// static
void AutofillMetrics::LogNumberOfProfilesRemovedDuringDedupe(
    size_t num_removed) {
  // A maximum of 50 is enforced to reduce the number of generated buckets.
  UMA_HISTOGRAM_COUNTS_1000(
      "Autofill.NumberOfProfilesRemovedDuringDedupe",
      std::min(static_cast<int>(num_removed), kMaxBucketsCount));
}

// static
void AutofillMetrics::LogIsQueriedCreditCardFormSecure(bool is_secure) {
  UMA_HISTOGRAM_BOOLEAN("Autofill.QueriedCreditCardFormIsSecure", is_secure);
}

// static
void AutofillMetrics::LogWalletAddressConversionType(
    WalletAddressConversionType type) {
  DCHECK_LT(type, NUM_CONVERTED_ADDRESS_CONVERSION_TYPES);
  UMA_HISTOGRAM_ENUMERATION("Autofill.WalletAddressConversionType", type,
                            NUM_CONVERTED_ADDRESS_CONVERSION_TYPES);
}

// static
void AutofillMetrics::LogShowedHttpNotSecureExplanation() {
  base::RecordAction(
      base::UserMetricsAction("Autofill_ShowedHttpNotSecureExplanation"));
}

// static
void AutofillMetrics::LogAutocompleteDaysSinceLastUse(size_t days) {
  UMA_HISTOGRAM_COUNTS_1000("Autocomplete.DaysSinceLastUse", days);
}

// static
void AutofillMetrics::LogAutocompleteSuggestionAcceptedIndex(int index) {
  base::UmaHistogramSparse("Autofill.SuggestionAcceptedIndex.Autocomplete",
                           std::min(index, kMaxBucketsCount));
  AutofillMetrics::Log(AutocompleteEvent::AUTOCOMPLETE_SUGGESTION_SELECTED);
}

// static
void AutofillMetrics::LogAutocompleteQuery(bool created) {
  UMA_HISTOGRAM_BOOLEAN("Autofill.AutocompleteQuery", created);
}

// static
void AutofillMetrics::LogAutocompleteSuggestions(bool has_suggestions) {
  UMA_HISTOGRAM_BOOLEAN("Autofill.AutocompleteSuggestions", has_suggestions);
}

// static
void AutofillMetrics::OnAutocompleteSuggestionsShown() {
  AutofillMetrics::Log(AutocompleteEvent::AUTOCOMPLETE_SUGGESTIONS_SHOWN);
}

void AutofillMetrics::LogNumberOfAutocompleteEntriesCleanedUp(int nb_entries) {
  UMA_HISTOGRAM_COUNTS_1000("Autocomplete.Cleanup", nb_entries);
}

// static
void AutofillMetrics::Log(AutocompleteEvent event) {
  DCHECK_LT(event, AutocompleteEvent::NUM_AUTOCOMPLETE_EVENTS);
  std::string name("Autocomplete.Events");

  base::UmaHistogramEnumeration(name, event, NUM_AUTOCOMPLETE_EVENTS);
}

// static
const char* AutofillMetrics::SubmissionSourceToUploadEventMetric(
    SubmissionSource source) {
  switch (source) {
    case SubmissionSource::NONE:
      return "Autofill.UploadEvent.None";
    case SubmissionSource::SAME_DOCUMENT_NAVIGATION:
      return "Autofill.UploadEvent.SameDocumentNavigation";
    case SubmissionSource::XHR_SUCCEEDED:
      return "Autofill.UploadEvent.XhrSucceeded";
    case SubmissionSource::FRAME_DETACHED:
      return "Autofill.UploadEvent.FrameDetached";
    case SubmissionSource::DOM_MUTATION_AFTER_XHR:
      return "Autofill.UploadEvent.DomMutationAfterXhr";
    case SubmissionSource::PROBABLY_FORM_SUBMITTED:
      return "Autofill.UploadEvent.ProbablyFormSubmitted";
    case SubmissionSource::FORM_SUBMISSION:
      return "Autofill.UploadEvent.FormSubmission";
  }
  // Unittests exercise this path, so do not put NOTREACHED() here.
  return "Autofill.UploadEvent.Unknown";
}

// static
void AutofillMetrics::LogUploadEvent(SubmissionSource submission_source,
                                     bool was_sent) {
  UMA_HISTOGRAM_BOOLEAN("Autofill.UploadEvent", was_sent);
  base::UmaHistogramEnumeration(
      SubmissionSourceToUploadEventMetric(submission_source),
      was_sent ? UploadEventStatus::kSent : UploadEventStatus::kNotSent);
}

// static
void AutofillMetrics::LogCardUploadDecisionsUkm(ukm::UkmRecorder* ukm_recorder,
                                                ukm::SourceId source_id,
                                                const GURL& url,
                                                int upload_decision_metrics) {
  DCHECK(upload_decision_metrics);
  DCHECK_LT(upload_decision_metrics, 1 << kNumCardUploadDecisionMetrics);
  if (!url.is_valid())
    return;
  ukm::builders::Autofill_CardUploadDecision(source_id)
      .SetUploadDecision(upload_decision_metrics)
      .Record(ukm_recorder);
}

// static
void AutofillMetrics::LogDeveloperEngagementUkm(
    ukm::UkmRecorder* ukm_recorder,
    ukm::SourceId source_id,
    const GURL& url,
    bool is_for_credit_card,
    DenseSet<FormType> form_types,
    int developer_engagement_metrics,
    FormSignature form_signature) {
  DCHECK(developer_engagement_metrics);
  DCHECK_LT(developer_engagement_metrics,
            1 << NUM_DEVELOPER_ENGAGEMENT_METRICS);
  if (!url.is_valid())
    return;

  ukm::builders::Autofill_DeveloperEngagement(source_id)
      .SetDeveloperEngagement(developer_engagement_metrics)
      .SetIsForCreditCard(is_for_credit_card)
      .SetFormTypes(FormTypesToBitVector(form_types))
      .SetFormSignature(HashFormSignature(form_signature))
      .Record(ukm_recorder);
}

AutofillMetrics::FormInteractionsUkmLogger::FormInteractionsUkmLogger(
    ukm::UkmRecorder* ukm_recorder,
    const ukm::SourceId source_id)
    : ukm_recorder_(ukm_recorder), source_id_(source_id) {}

void AutofillMetrics::FormInteractionsUkmLogger::OnFormsParsed(
    const ukm::SourceId source_id) {
  if (!CanLog())
    return;

  source_id_ = source_id;
}

void AutofillMetrics::FormInteractionsUkmLogger::LogInteractedWithForm(
    bool is_for_credit_card,
    size_t local_record_type_count,
    size_t server_record_type_count,
    FormSignature form_signature) {
  if (!CanLog())
    return;

  ukm::builders::Autofill_InteractedWithForm(source_id_)
      .SetIsForCreditCard(is_for_credit_card)
      .SetLocalRecordTypeCount(local_record_type_count)
      .SetServerRecordTypeCount(server_record_type_count)
      .SetFormSignature(HashFormSignature(form_signature))
      .Record(ukm_recorder_);
}

void AutofillMetrics::FormInteractionsUkmLogger::LogSuggestionsShown(
    const FormStructure& form,
    const AutofillField& field,
    const base::TimeTicks& form_parsed_timestamp,
    bool off_the_record) {
  if (!CanLog())
    return;

  ukm::builders::Autofill_SuggestionsShown(source_id_)
      .SetHeuristicType(static_cast<int>(field.heuristic_type()))
      .SetHtmlFieldType(static_cast<int>(field.html_type()))
      .SetServerType(static_cast<int>(field.server_type()))
      .SetFormSignature(HashFormSignature(form.form_signature()))
      .SetFieldSignature(HashFieldSignature(field.GetFieldSignature()))
      .SetMillisecondsSinceFormParsed(
          MillisecondsSinceFormParsed(form_parsed_timestamp))
      .Record(ukm_recorder_);

  base::UmaHistogramBoolean("Autofill.SuggestionShown.OffTheRecord",
                            off_the_record);
}

void AutofillMetrics::FormInteractionsUkmLogger::LogDidFillSuggestion(
    int record_type,
    bool is_for_credit_card,
    const FormStructure& form,
    const AutofillField& field) {
  if (!CanLog())
    return;

  ukm::builders::Autofill_SuggestionFilled(source_id_)
      .SetRecordType(record_type)
      .SetIsForCreditCard(is_for_credit_card)
      .SetMillisecondsSinceFormParsed(
          MillisecondsSinceFormParsed(form.form_parsed_timestamp()))
      .SetFormSignature(HashFormSignature(form.form_signature()))
      .SetFieldSignature(HashFieldSignature(field.GetFieldSignature()))
      .Record(ukm_recorder_);
}

void AutofillMetrics::FormInteractionsUkmLogger::
    LogEditedAutofilledFieldAtSubmission(const FormStructure& form,
                                         const AutofillField& field) {
  if (!CanLog())
    return;

  ukm::builders::Autofill_EditedAutofilledFieldAtSubmission(source_id_)
      .SetFieldSignature(HashFieldSignature(field.GetFieldSignature()))
      .SetFormSignature(HashFormSignature(form.form_signature()))
      .SetOverallType(static_cast<int64_t>(field.Type().GetStorableType()))
      .Record(ukm_recorder_);
}

void AutofillMetrics::FormInteractionsUkmLogger::LogTextFieldDidChange(
    const FormStructure& form,
    const AutofillField& field) {
  if (!CanLog())
    return;

  ukm::builders::Autofill_TextFieldDidChange(source_id_)
      .SetFormSignature(HashFormSignature(form.form_signature()))
      .SetFieldSignature(HashFieldSignature(field.GetFieldSignature()))
      .SetFieldTypeGroup(static_cast<int>(field.Type().group()))
      .SetHeuristicType(static_cast<int>(field.heuristic_type()))
      .SetServerType(static_cast<int>(field.server_type()))
      .SetHtmlFieldType(static_cast<int>(field.html_type()))
      .SetHtmlFieldMode(static_cast<int>(field.html_mode()))
      .SetIsAutofilled(field.is_autofilled)
      .SetIsEmpty(field.IsEmpty())
      .SetMillisecondsSinceFormParsed(
          MillisecondsSinceFormParsed(form.form_parsed_timestamp()))
      .Record(ukm_recorder_);
}

void AutofillMetrics::FormInteractionsUkmLogger::LogFieldFillStatus(
    const FormStructure& form,
    const AutofillField& field,
    QualityMetricType metric_type) {
  if (!CanLog())
    return;

  ukm::builders::Autofill_FieldFillStatus(source_id_)
      .SetMillisecondsSinceFormParsed(
          MillisecondsSinceFormParsed(form.form_parsed_timestamp()))
      .SetFormSignature(HashFormSignature(form.form_signature()))
      .SetFieldSignature(HashFieldSignature(field.GetFieldSignature()))
      .SetValidationEvent(static_cast<int64_t>(metric_type))
      .SetIsAutofilled(static_cast<int64_t>(field.is_autofilled))
      .SetWasPreviouslyAutofilled(
          static_cast<int64_t>(field.previously_autofilled()))
      .Record(ukm_recorder_);
}

// TODO(szhangcs): Take FormStructure and AutofillField and extract
// FormSignature and TimeTicks inside the function.
void AutofillMetrics::FormInteractionsUkmLogger::LogFieldType(
    const base::TimeTicks& form_parsed_timestamp,
    FormSignature form_signature,
    FieldSignature field_signature,
    QualityMetricPredictionSource prediction_source,
    QualityMetricType metric_type,
    ServerFieldType predicted_type,
    ServerFieldType actual_type) {
  if (!CanLog())
    return;

  ukm::builders::Autofill_FieldTypeValidation(source_id_)
      .SetMillisecondsSinceFormParsed(
          MillisecondsSinceFormParsed(form_parsed_timestamp))
      .SetFormSignature(HashFormSignature(form_signature))
      .SetFieldSignature(HashFieldSignature(field_signature))
      .SetValidationEvent(static_cast<int64_t>(metric_type))
      .SetPredictionSource(static_cast<int64_t>(prediction_source))
      .SetPredictedType(static_cast<int64_t>(predicted_type))
      .SetActualType(static_cast<int64_t>(actual_type))
      .Record(ukm_recorder_);
}

void AutofillMetrics::FormInteractionsUkmLogger::
    LogHiddenRepresentationalFieldSkipDecision(const FormStructure& form,
                                               const AutofillField& field,
                                               bool is_skipped) {
  if (!CanLog())
    return;

  ukm::builders::Autofill_HiddenRepresentationalFieldSkipDecision(source_id_)
      .SetFormSignature(HashFormSignature(form.form_signature()))
      .SetFieldSignature(HashFieldSignature(field.GetFieldSignature()))
      .SetFieldTypeGroup(static_cast<int>(field.Type().group()))
      .SetFieldOverallType(static_cast<int>(field.Type().GetStorableType()))
      .SetHeuristicType(static_cast<int>(field.heuristic_type()))
      .SetServerType(static_cast<int>(field.server_type()))
      .SetHtmlFieldType(static_cast<int>(field.html_type()))
      .SetHtmlFieldMode(static_cast<int>(field.html_mode()))
      .SetIsSkipped(is_skipped)
      .Record(ukm_recorder_);
}

void AutofillMetrics::FormInteractionsUkmLogger::
    LogRepeatedServerTypePredictionRationalized(
        const FormSignature form_signature,
        const AutofillField& field,
        ServerFieldType old_type) {
  if (!CanLog())
    return;

  ukm::builders::Autofill_RepeatedServerTypePredictionRationalized(source_id_)
      .SetFormSignature(HashFormSignature(form_signature))
      .SetFieldSignature(HashFieldSignature(field.GetFieldSignature()))
      .SetFieldTypeGroup(static_cast<int>(field.Type().group()))
      .SetFieldNewOverallType(static_cast<int>(field.Type().GetStorableType()))
      .SetHeuristicType(static_cast<int>(field.heuristic_type()))
      .SetHtmlFieldType(static_cast<int>(field.html_type()))
      .SetHtmlFieldMode(static_cast<int>(field.html_mode()))
      .SetServerType(static_cast<int>(field.server_type()))
      .SetFieldOldOverallType(static_cast<int>(old_type))
      .Record(ukm_recorder_);
}

int64_t AutofillMetrics::FormTypesToBitVector(
    const DenseSet<FormType>& form_types) {
  int64_t form_type_bv = 0;
  for (FormType form_type : form_types) {
    DCHECK_LT(static_cast<int64_t>(form_type), 63);
    form_type_bv |= 1LL << static_cast<int64_t>(form_type);
  }
  return form_type_bv;
}

void AutofillMetrics::LogServerCardLinkClicked(
    AutofillSyncSigninState sync_state) {
  UMA_HISTOGRAM_ENUMERATION("Autofill.ServerCardLinkClicked", sync_state,
                            AutofillSyncSigninState::kNumSyncStates);
}

void AutofillMetrics::LogCardUploadEnabledMetric(
    CardUploadEnabledMetric metric_value,
    AutofillSyncSigninState sync_state) {
  const std::string parent_metric = std::string("Autofill.CardUploadEnabled");
  base::UmaHistogramEnumeration(parent_metric, metric_value);

  const std::string child_metric =
      parent_metric + GetMetricsSyncStateSuffix(sync_state);
  base::UmaHistogramEnumeration(child_metric, metric_value);
}

// static
const char* AutofillMetrics::GetMetricsSyncStateSuffix(
    AutofillSyncSigninState sync_state) {
  switch (sync_state) {
    case AutofillSyncSigninState::kSignedOut:
      return ".SignedOut";
    case AutofillSyncSigninState::kSignedIn:
      return ".SignedIn";
    case AutofillSyncSigninState::kSignedInAndWalletSyncTransportEnabled:
      return ".SignedInAndWalletSyncTransportEnabled";
    case AutofillSyncSigninState::kSignedInAndSyncFeatureEnabled:
      return ".SignedInAndSyncFeatureEnabled";
    case AutofillSyncSigninState::kSyncPaused:
      return ".SyncPaused";
    case AutofillSyncSigninState::kNumSyncStates:
      return ".Unknown";
  }
}

void AutofillMetrics::FormInteractionsUkmLogger::LogFormSubmitted(
    bool is_for_credit_card,
    bool has_upi_vpa_field,
    const DenseSet<FormType>& form_types,
    AutofillFormSubmittedState state,
    const base::TimeTicks& form_parsed_timestamp,
    FormSignature form_signature) {
  if (!CanLog())
    return;

  ukm::builders::Autofill_FormSubmitted builder(source_id_);
  builder.SetAutofillFormSubmittedState(static_cast<int>(state))
      .SetIsForCreditCard(is_for_credit_card)
      .SetHasUpiVpaField(has_upi_vpa_field)
      .SetFormTypes(FormTypesToBitVector(form_types))
      .SetFormSignature(HashFormSignature(form_signature));
  if (form_parsed_timestamp.is_null())
    DCHECK(state == NON_FILLABLE_FORM_OR_NEW_DATA ||
           state == FILLABLE_FORM_AUTOFILLED_NONE_DID_NOT_SHOW_SUGGESTIONS)
        << state;
  else
    builder.SetMillisecondsSinceFormParsed(
        MillisecondsSinceFormParsed(form_parsed_timestamp));

  builder.Record(ukm_recorder_);
}

void AutofillMetrics::FormInteractionsUkmLogger::LogFormEvent(
    FormEvent form_event,
    const DenseSet<FormType>& form_types,
    const base::TimeTicks& form_parsed_timestamp) {
  if (!CanLog())
    return;

  if (form_parsed_timestamp.is_null())
    return;

  ukm::builders::Autofill_FormEvent builder(source_id_);
  builder.SetAutofillFormEvent(static_cast<int>(form_event))
      .SetFormTypes(FormTypesToBitVector(form_types))
      .SetMillisecondsSinceFormParsed(
          MillisecondsSinceFormParsed(form_parsed_timestamp))
      .Record(ukm_recorder_);
}

bool AutofillMetrics::FormInteractionsUkmLogger::CanLog() const {
  return ukm_recorder_ != nullptr;
}

int64_t AutofillMetrics::FormInteractionsUkmLogger::MillisecondsSinceFormParsed(
    const base::TimeTicks& form_parsed_timestamp) const {
  DCHECK(!form_parsed_timestamp.is_null());
  // Use the pinned timestamp as the current time if it's set.
  base::TimeTicks now = pinned_timestamp_.is_null()
                            ? AutofillTickClock::NowTicks()
                            : pinned_timestamp_;

  return ukm::GetExponentialBucketMin(
      (now - form_parsed_timestamp).InMilliseconds(),
      kAutofillEventDataBucketSpacing);
}

AutofillMetrics::UkmTimestampPin::UkmTimestampPin(
    FormInteractionsUkmLogger* logger)
    : logger_(logger) {
  DCHECK(logger_);
  DCHECK(!logger_->has_pinned_timestamp());
  logger_->set_pinned_timestamp(AutofillTickClock::NowTicks());
}

AutofillMetrics::UkmTimestampPin::~UkmTimestampPin() {
  DCHECK(logger_->has_pinned_timestamp());
  logger_->set_pinned_timestamp(base::TimeTicks());
}

// static
void AutofillMetrics::LogAddressFormImportRequirementMetric(
    AutofillMetrics::AddressProfileImportRequirementMetric metric) {
  // Shift the requirement type index by one bit to the right.
  // The freed least significant bit is used to indicate the fulfillment status
  // of the specific requirement.
  base::UmaHistogramEnumeration("Autofill.AddressProfileImportRequirements",
                                metric);
}

// static
void AutofillMetrics::
    LogAddressFormImportCountrySpecificFieldRequirementsMetric(
        bool is_zip_missing,
        bool is_state_missing,
        bool is_city_missing,
        bool is_line1_missing) {
  const auto metric = static_cast<
      AutofillMetrics::
          AddressProfileImportCountrySpecificFieldRequirementsMetric>(
      (is_zip_missing ? 0b1 : 0) | (is_state_missing ? 0b10 : 0) |
      (is_city_missing ? 0b100 : 0) | (is_line1_missing ? 0b1000 : 0));
  base::UmaHistogramEnumeration(
      "Autofill.AddressProfileImportCountrySpecificFieldRequirements", metric);
}

// static
void AutofillMetrics::LogAddressFormImportStatustMetric(
    AutofillMetrics::AddressProfileImportStatusMetric metric) {
  base::UmaHistogramEnumeration("Autofill.AddressProfileImportStatus", metric);
}

// static
void AutofillMetrics::LogFieldParsingPageTranslationStatusMetric(bool metric) {
  base::UmaHistogramBoolean("Autofill.ParsedFieldTypesWasPageTranslated",
                            metric);
}

// static
void AutofillMetrics::LogFieldParsingTranslatedFormLanguageMetric(
    base::StringPiece locale) {
  base::UmaHistogramSparse(
      "Autofill.ParsedFieldTypesUsingTranslatedPageLanguage",
      language::LanguageUsageMetrics::ToLanguageCodeHash(locale));
}

// static
void AutofillMetrics::LogWebOTPPhoneCollectionMetricStateUkm(
    ukm::UkmRecorder* recorder,
    ukm::SourceId source_id,
    uint32_t phone_collection_metric_state) {
  // UKM recording is not supported for WebViews.
  if (!recorder || source_id == ukm::kInvalidSourceId)
    return;

  ukm::builders::WebOTPImpact builder(source_id);
  builder.SetPhoneCollection(phone_collection_metric_state);
  builder.Record(recorder);
}

// static
void AutofillMetrics::LogNumberOfAutofilledFieldsAtSubmission(
    size_t number_of_accepted_fields,
    size_t number_of_corrected_fields) {
  base::UmaHistogramExactLinear(
      "Autofill.NumberOfAutofilledFieldsAtSubmission.Total",
      number_of_accepted_fields + number_of_corrected_fields, 50);
  base::UmaHistogramExactLinear(
      "Autofill.NumberOfAutofilledFieldsAtSubmission.Accepted",
      number_of_accepted_fields, 50);
  base::UmaHistogramExactLinear(
      "Autofill.NumberOfAutofilledFieldsAtSubmission.Corrected",
      number_of_corrected_fields, 50);
}

void AutofillMetrics::LogProfileImportType(
    AutofillProfileImportType import_type) {
  base::UmaHistogramEnumeration("Autofill.ProfileImport.ProfileImportType",
                                import_type);
}

void AutofillMetrics::LogSilentUpdatesProfileImportType(
    AutofillProfileImportType import_type) {
  base::UmaHistogramEnumeration(
      "Autofill.ProfileImport.SilentUpdatesProfileImportType", import_type);
}

void AutofillMetrics::LogNewProfileImportDecision(
    AutofillClient::SaveAddressProfileOfferUserDecision decision) {
  base::UmaHistogramEnumeration("Autofill.ProfileImport.NewProfileDecision",
                                decision);
}

void AutofillMetrics::LogNewProfileEditedType(ServerFieldType edited_type) {
  base::UmaHistogramEnumeration(
      "Autofill.ProfileImport.NewProfileEditedType",
      ConvertSettingsVisibleFieldTypeForMetrics(edited_type));
}

void AutofillMetrics::LogNewProfileNumberOfEditedFields(
    int number_of_edited_fields) {
  base::UmaHistogramExactLinear(
      "Autofill.ProfileImport.NewProfileNumberOfEditedFields",
      number_of_edited_fields, /*exclusive_max=*/15);
}

void AutofillMetrics::LogProfileUpdateImportDecision(
    AutofillClient::SaveAddressProfileOfferUserDecision decision) {
  base::UmaHistogramEnumeration("Autofill.ProfileImport.UpdateProfileDecision",
                                decision);
}

void AutofillMetrics::LogProfileUpdateAffectedType(
    ServerFieldType affected_type,
    AutofillClient::SaveAddressProfileOfferUserDecision decision) {
  // TODO(crbug.com/1253798): Remove the special-case metric in favor of more
  // general one once the majority of clients contribute to the more general
  // one.
  if (decision ==
      AutofillClient::SaveAddressProfileOfferUserDecision::kAccepted) {
    base::UmaHistogramEnumeration(
        "Autofill.ProfileImport.UpdateProfileAffectedType",
        ConvertSettingsVisibleFieldTypeForMetrics(affected_type));
  }

  // Record the decision-specific metric.
  base::UmaHistogramEnumeration(
      base::StrCat({"Autofill.ProfileImport.UpdateProfileAffectedType",
                    GetSaveAndUpdatePromptDecisionMetricsSuffix(decision)}),
      ConvertSettingsVisibleFieldTypeForMetrics(affected_type));

  // But also collect an histogram for any decision.
  base::UmaHistogramEnumeration(
      "Autofill.ProfileImport.UpdateProfileAffectedType.Any",
      ConvertSettingsVisibleFieldTypeForMetrics(affected_type));
}

void AutofillMetrics::LogProfileUpdateEditedType(ServerFieldType edited_type) {
  base::UmaHistogramEnumeration(
      "Autofill.ProfileImport.UpdateProfileEditedType",
      ConvertSettingsVisibleFieldTypeForMetrics(edited_type));
}

void AutofillMetrics::LogUpdateProfileNumberOfEditedFields(
    int number_of_edited_fields) {
  base::UmaHistogramExactLinear(
      "Autofill.ProfileImport.UpdateProfileNumberOfEditedFields",
      number_of_edited_fields, /*exclusive_max=*/15);
}

void AutofillMetrics::LogUpdateProfileNumberOfAffectedFields(
    int number_of_edited_fields,
    AutofillClient::SaveAddressProfileOfferUserDecision decision) {
  // TODO(crbug.com/1253798): Remove the special-case metric in favor of more
  // general one once the majority of clients contribute to the more general
  // one.
  if (decision ==
      AutofillClient::SaveAddressProfileOfferUserDecision::kAccepted) {
    base::UmaHistogramExactLinear(
        "Autofill.ProfileImport.UpdateProfileNumberOfAffectedFields",
        number_of_edited_fields, /*exclusive_max=*/15);
  }

  // Record the decision-specific metric.
  base::UmaHistogramExactLinear(
      base::StrCat({"Autofill.ProfileImport.UpdateProfileAffectedType",
                    GetSaveAndUpdatePromptDecisionMetricsSuffix(decision)}),
      number_of_edited_fields, /*exclusive_max=*/15);

  // But also collect an histogram for any decision.
  base::UmaHistogramExactLinear(
      "Autofill.ProfileImport.UpdateProfileAffectedType.Any",
      number_of_edited_fields, /*exclusive_max=*/15);
}

void AutofillMetrics::LogVerificationStatusOfNameTokensOnProfileUsage(
    const AutofillProfile& profile) {
  constexpr base::StringPiece base_histogram_name =
      "Autofill.NameTokenVerificationStatusAtProfileUsage.";

  for (const auto& type_name_pair : kStructuredNameTypeToNameMap) {
    // Do not record the status for empty values.
    if (profile.GetRawInfo(type_name_pair.first).empty()) {
      continue;
    }

    structured_address::VerificationStatus status =
        profile.GetVerificationStatus(type_name_pair.first);
    base::UmaHistogramEnumeration(
        base::StrCat({base_histogram_name, type_name_pair.second}), status);
    base::UmaHistogramEnumeration(base::StrCat({base_histogram_name, "Any"}),
                                  status);
  }
}

void AutofillMetrics::LogVerificationStatusOfAddressTokensOnProfileUsage(
    const AutofillProfile& profile) {
  constexpr base::StringPiece base_histogram_name =
      "Autofill.AddressTokenVerificationStatusAtProfileUsage.";

  for (const auto& type_name_pair : kStructuredAddressTypeToNameMap) {
    // Do not record the status for empty values.
    if (profile.GetRawInfo(type_name_pair.first).empty()) {
      continue;
    }

    structured_address::VerificationStatus status =
        profile.GetVerificationStatus(type_name_pair.first);
    base::UmaHistogramEnumeration(
        base::StrCat({base_histogram_name, type_name_pair.second}), status);
    base::UmaHistogramEnumeration(base::StrCat({base_histogram_name, "Any"}),
                                  status);
  }
}

// static
void AutofillMetrics::LogVirtualCardMetadataSynced(bool existing_card) {
  base::UmaHistogramBoolean("Autofill.VirtualCard.MetadataSynced",
                            existing_card);
}

// static
void AutofillMetrics::LogImageFetchResult(bool succeeded) {
  base::UmaHistogramBoolean("Autofill.ImageFetcher.Result", succeeded);
}

// static
void AutofillMetrics::LogOtpAuthAttempt() {
  base::UmaHistogramBoolean("Autofill.OtpAuth.SmsOtp.Attempt", true);
}

// static
void AutofillMetrics::LogOtpAuthResult(OtpAuthEvent event) {
  DCHECK_LE(event, OtpAuthEvent::kMaxValue);
  base::UmaHistogramEnumeration("Autofill.OtpAuth.SmsOtp.Result", event);
}

// static
void AutofillMetrics::LogOtpAuthRetriableError(OtpAuthEvent event) {
  DCHECK_LE(event, OtpAuthEvent::kMaxValue);
  base::UmaHistogramEnumeration("Autofill.OtpAuth.SmsOtp.RetriableError",
                                event);
}

// static
void AutofillMetrics::LogOtpAuthUnmaskCardRequestLatency(
    const base::TimeDelta& duration) {
  base::UmaHistogramLongTimes(
      "Autofill.OtpAuth.SmsOtp.RequestLatency.UnmaskCardRequest", duration);
}

// static
void AutofillMetrics::LogOtpAuthSelectChallengeOptionRequestLatency(
    const base::TimeDelta& duration) {
  base::UmaHistogramLongTimes(
      "Autofill.OtpAuth.SmsOtp.RequestLatency.SelectChallengeOptionRequest",
      duration);
}

// static
void AutofillMetrics::LogOtpInputDialogShown() {
  base::UmaHistogramBoolean("Autofill.OtpInputDialog.SmsOtp.Shown", true);
}

// static
void AutofillMetrics::LogOtpInputDialogResult(OtpInputDialogResult result,
                                              bool temporary_error_shown) {
  DCHECK_GT(result, OtpInputDialogResult::kUnknown);
  DCHECK_LE(result, OtpInputDialogResult::kMaxValue);
  std::string temporary_error_shown_suffix = temporary_error_shown
                                                 ? ".WithPreviousTemporaryError"
                                                 : ".WithNoTemporaryError";
  base::UmaHistogramEnumeration("Autofill.OtpInputDialog.SmsOtp.Result",
                                result);
  base::UmaHistogramEnumeration(
      "Autofill.OtpInputDialog.SmsOtp.Result" + temporary_error_shown_suffix,
      result);
}

// static
void AutofillMetrics::LogOtpInputDialogErrorMessageShown(
    OtpInputDialogError error) {
  DCHECK_GT(error, OtpInputDialogError::kUnknown);
  DCHECK_LE(error, OtpInputDialogError::kMaxValue);
  base::UmaHistogramEnumeration(
      "Autofill.OtpInputDialog.SmsOtp.ErrorMessageShown", error);
}

// static
void AutofillMetrics::LogOtpInputDialogNewOtpRequested() {
  base::UmaHistogramBoolean("Autofill.OtpInputDialog.SmsOtp.NewOtpRequested",
                            true);
}

}  // namespace autofill
