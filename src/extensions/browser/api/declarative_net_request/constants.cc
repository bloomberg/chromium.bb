// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/constants.h"

namespace extensions {
namespace declarative_net_request {

const char kErrorResourceTypeDuplicated[] =
    "*: Rule with id * includes and excludes the same resource.";
const char kErrorEmptyRedirectRuleKey[] =
    "*: Rule with id * does not specify the value for * key. This is required "
    "for redirect rules.";
const char kErrorInvalidRuleKey[] =
    "*: Rule with id * has an invalid value for * key. This should be greater "
    "than or equal to *.";
const char kErrorNoApplicableResourceTypes[] =
    "*: Rule with id * is not applicable to any resource type.";
const char kErrorEmptyList[] =
    "*: Rule with id * cannot have an empty list as the value for * key.";
const char kErrorEmptyUrlFilter[] =
    "*: Rule with id * cannot have an empty value for * key.";
const char kErrorInvalidRedirectUrl[] =
    "*: Rule with id * does not provide a valid URL for * key.";
const char kErrorDuplicateIDs[] =
    "*: Rule with id * does not have a unique ID.";
// Don't surface the actual error to the user, since it's an implementation
// detail.
const char kErrorPersisting[] = "*: Rules file could not be parsed.";
const char kErrorListNotPassed[] = "*: Rules file must contain a list.";
const char kErrorNonAscii[] =
    "*: Rule with id * cannot have non-ascii characters as part of \"*\" key.";
const char kErrorInvalidUrlFilter[] =
    "*: Rule with id * has an invalid value for \"*\" key.";

const char kRuleCountExceeded[] =
    "Declarative Net Request: Rule count exceeded. Some rules were ignored.";
const char kRuleNotParsedWarning[] =
    "Declarative Net Request: Rule with * couldn't be parsed. Parse error: "
    "*.";
const char kTooManyParseFailuresWarning[] =
    "Declarative Net Request: Too many rule parse failures; Reporting the "
    "first *.";
const char kIndexAndPersistRulesTimeHistogram[] =
    "Extensions.DeclarativeNetRequest.IndexAndPersistRulesTime";
const char kIndexRulesTimeHistogram[] =
    "Extensions.DeclarativeNetRequest.IndexRulesTime";
const char kManifestRulesCountHistogram[] =
    "Extensions.DeclarativeNetRequest.ManifestRulesCount";

}  // namespace declarative_net_request
}  // namespace extensions
