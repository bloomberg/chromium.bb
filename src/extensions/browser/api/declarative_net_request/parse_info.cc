// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/parse_info.h"

#include "base/logging.h"
#include "extensions/common/error_utils.h"

namespace extensions {
namespace declarative_net_request {

ParseInfo::ParseInfo(ParseResult result) : result_(result) {}
ParseInfo::ParseInfo(ParseResult result, int rule_id)
    : result_(result), rule_id_(rule_id) {}
ParseInfo::ParseInfo(const ParseInfo&) = default;
ParseInfo& ParseInfo::operator=(const ParseInfo&) = default;

std::string ParseInfo::GetErrorDescription() const {
  // Every error except ERROR_PERSISTING_RULESET requires |rule_id_|.
  DCHECK_EQ(!rule_id_.has_value(),
            result_ == ParseResult::ERROR_PERSISTING_RULESET);

  std::string error;
  switch (result_) {
    case ParseResult::SUCCESS:
      NOTREACHED();
      break;
    case ParseResult::ERROR_RESOURCE_TYPE_DUPLICATED:
      error = ErrorUtils::FormatErrorMessage(kErrorResourceTypeDuplicated,
                                             std::to_string(*rule_id_));
      break;
    case ParseResult::ERROR_EMPTY_REDIRECT_RULE_PRIORITY:
      error = ErrorUtils::FormatErrorMessage(
          kErrorEmptyRedirectRuleKey, std::to_string(*rule_id_), kPriorityKey);
      break;
    case ParseResult::ERROR_EMPTY_REDIRECT_URL:
      error = ErrorUtils::FormatErrorMessage(kErrorEmptyRedirectRuleKey,
                                             std::to_string(*rule_id_),
                                             kRedirectUrlKey);
      break;
    case ParseResult::ERROR_INVALID_RULE_ID:
      error = ErrorUtils::FormatErrorMessage(kErrorInvalidRuleKey,
                                             std::to_string(*rule_id_), kIDKey,
                                             std::to_string(kMinValidID));
      break;
    case ParseResult::ERROR_INVALID_REDIRECT_RULE_PRIORITY:
      error = ErrorUtils::FormatErrorMessage(
          kErrorInvalidRuleKey, std::to_string(*rule_id_), kPriorityKey,
          std::to_string(kMinValidPriority));
      break;
    case ParseResult::ERROR_NO_APPLICABLE_RESOURCE_TYPES:
      error = ErrorUtils::FormatErrorMessage(kErrorNoApplicableResourceTypes,

                                             std::to_string(*rule_id_));
      break;
    case ParseResult::ERROR_EMPTY_DOMAINS_LIST:
      error = ErrorUtils::FormatErrorMessage(
          kErrorEmptyList, std::to_string(*rule_id_), kDomainsKey);
      break;
    case ParseResult::ERROR_EMPTY_RESOURCE_TYPES_LIST:
      error = ErrorUtils::FormatErrorMessage(
          kErrorEmptyList, std::to_string(*rule_id_), kResourceTypesKey);
      break;
    case ParseResult::ERROR_EMPTY_URL_FILTER:
      error = ErrorUtils::FormatErrorMessage(
          kErrorEmptyUrlFilter, std::to_string(*rule_id_), kUrlFilterKey);
      break;
    case ParseResult::ERROR_INVALID_REDIRECT_URL:
      error = ErrorUtils::FormatErrorMessage(
          kErrorInvalidRedirectUrl, std::to_string(*rule_id_), kRedirectUrlKey);
      break;
    case ParseResult::ERROR_DUPLICATE_IDS:
      error = ErrorUtils::FormatErrorMessage(kErrorDuplicateIDs,
                                             std::to_string(*rule_id_));
      break;
    case ParseResult::ERROR_PERSISTING_RULESET:
      error = kErrorPersisting;
      break;
    case ParseResult::ERROR_NON_ASCII_URL_FILTER:
      error = ErrorUtils::FormatErrorMessage(
          kErrorNonAscii, std::to_string(*rule_id_), kUrlFilterKey);
      break;
    case ParseResult::ERROR_NON_ASCII_DOMAIN:
      error = ErrorUtils::FormatErrorMessage(
          kErrorNonAscii, std::to_string(*rule_id_), kDomainsKey);
      break;
    case ParseResult::ERROR_NON_ASCII_EXCLUDED_DOMAIN:
      error = ErrorUtils::FormatErrorMessage(
          kErrorNonAscii, std::to_string(*rule_id_), kExcludedDomainsKey);
      break;
    case ParseResult::ERROR_INVALID_URL_FILTER:
      error = ErrorUtils::FormatErrorMessage(
          kErrorInvalidUrlFilter, std::to_string(*rule_id_), kUrlFilterKey);
      break;
    case ParseResult::ERROR_EMPTY_REMOVE_HEADERS_LIST:
      error = ErrorUtils::FormatErrorMessage(kErrorEmptyRemoveHeadersList,
                                             std::to_string(*rule_id_),
                                             kRemoveHeadersListKey);
      break;
  }
  return error;
}

}  // namespace declarative_net_request
}  // namespace extensions
