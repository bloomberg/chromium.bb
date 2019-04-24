// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/parse_info.h"

#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "extensions/common/error_utils.h"

namespace extensions {
namespace declarative_net_request {

ParseInfo::ParseInfo(ParseResult result) : result_(result) {}
ParseInfo::ParseInfo(ParseResult result, int rule_id)
    : result_(result), rule_id_(rule_id) {}
ParseInfo::ParseInfo(const ParseInfo&) = default;
ParseInfo& ParseInfo::operator=(const ParseInfo&) = default;

std::string ParseInfo::GetErrorDescription(
    const base::StringPiece json_rules_filename) const {
  // Every error except ERROR_PERSISTING_RULESET and ERROR_LIST_NOT_PASSED
  // requires |rule_id_|.
  DCHECK_EQ(!rule_id_.has_value(),
            result_ == ParseResult::ERROR_PERSISTING_RULESET ||
                result_ == ParseResult::ERROR_LIST_NOT_PASSED);

  std::string error;
  switch (result_) {
    case ParseResult::SUCCESS:
      NOTREACHED();
      break;
    case ParseResult::ERROR_RESOURCE_TYPE_DUPLICATED:
      error = ErrorUtils::FormatErrorMessage(kErrorResourceTypeDuplicated,
                                             json_rules_filename,
                                             std::to_string(*rule_id_));
      break;
    case ParseResult::ERROR_EMPTY_REDIRECT_RULE_PRIORITY:
      error = ErrorUtils::FormatErrorMessage(
          kErrorEmptyRedirectRuleKey, json_rules_filename,
          std::to_string(*rule_id_), kPriorityKey);
      break;
    case ParseResult::ERROR_EMPTY_REDIRECT_URL:
      error = ErrorUtils::FormatErrorMessage(
          kErrorEmptyRedirectRuleKey, json_rules_filename,
          std::to_string(*rule_id_), kRedirectUrlKey);
      break;
    case ParseResult::ERROR_INVALID_RULE_ID:
      error = ErrorUtils::FormatErrorMessage(
          kErrorInvalidRuleKey, json_rules_filename, std::to_string(*rule_id_),
          kIDKey, std::to_string(kMinValidID));
      break;
    case ParseResult::ERROR_INVALID_REDIRECT_RULE_PRIORITY:
      error = ErrorUtils::FormatErrorMessage(
          kErrorInvalidRuleKey, json_rules_filename, std::to_string(*rule_id_),
          kPriorityKey, std::to_string(kMinValidPriority));
      break;
    case ParseResult::ERROR_NO_APPLICABLE_RESOURCE_TYPES:
      error = ErrorUtils::FormatErrorMessage(kErrorNoApplicableResourceTypes,
                                             json_rules_filename,
                                             std::to_string(*rule_id_));
      break;
    case ParseResult::ERROR_EMPTY_DOMAINS_LIST:
      error = ErrorUtils::FormatErrorMessage(
          kErrorEmptyList, json_rules_filename, std::to_string(*rule_id_),
          kDomainsKey);
      break;
    case ParseResult::ERROR_EMPTY_RESOURCE_TYPES_LIST:
      error = ErrorUtils::FormatErrorMessage(
          kErrorEmptyList, json_rules_filename, std::to_string(*rule_id_),
          kResourceTypesKey);
      break;
    case ParseResult::ERROR_EMPTY_URL_FILTER:
      error = ErrorUtils::FormatErrorMessage(
          kErrorEmptyUrlFilter, json_rules_filename, std::to_string(*rule_id_),
          kUrlFilterKey);
      break;
    case ParseResult::ERROR_INVALID_REDIRECT_URL:
      error = ErrorUtils::FormatErrorMessage(
          kErrorInvalidRedirectUrl, json_rules_filename,
          std::to_string(*rule_id_), kRedirectUrlKey);
      break;
    case ParseResult::ERROR_DUPLICATE_IDS:
      error = ErrorUtils::FormatErrorMessage(
          kErrorDuplicateIDs, json_rules_filename, std::to_string(*rule_id_));
      break;
    case ParseResult::ERROR_PERSISTING_RULESET:
      error =
          ErrorUtils::FormatErrorMessage(kErrorPersisting, json_rules_filename);
      break;
    case ParseResult::ERROR_LIST_NOT_PASSED:
      error = ErrorUtils::FormatErrorMessage(kErrorListNotPassed,
                                             json_rules_filename);
      break;
    case ParseResult::ERROR_NON_ASCII_URL_FILTER:
      error = ErrorUtils::FormatErrorMessage(
          kErrorNonAscii, json_rules_filename, std::to_string(*rule_id_),
          kUrlFilterKey);
      break;
    case ParseResult::ERROR_NON_ASCII_DOMAIN:
      error = ErrorUtils::FormatErrorMessage(
          kErrorNonAscii, json_rules_filename, std::to_string(*rule_id_),
          kDomainsKey);
      break;
    case ParseResult::ERROR_NON_ASCII_EXCLUDED_DOMAIN:
      error = ErrorUtils::FormatErrorMessage(
          kErrorNonAscii, json_rules_filename, std::to_string(*rule_id_),
          kExcludedDomainsKey);
      break;
    case ParseResult::ERROR_INVALID_URL_FILTER:
      error = ErrorUtils::FormatErrorMessage(
          kErrorInvalidUrlFilter, json_rules_filename,
          std::to_string(*rule_id_), kUrlFilterKey);
      break;
  }
  return error;
}

}  // namespace declarative_net_request
}  // namespace extensions
