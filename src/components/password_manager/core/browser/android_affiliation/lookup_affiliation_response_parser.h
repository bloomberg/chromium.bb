// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ANDROID_AFFILIATION_LOOKUP_AFFILIATION_RESPONSE_PARSER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ANDROID_AFFILIATION_LOOKUP_AFFILIATION_RESPONSE_PARSER_H_

#include <stddef.h>

#include <map>
#include <vector>

#include "components/password_manager/core/browser/android_affiliation/affiliation_api.pb.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_fetcher_delegate.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_utils.h"

namespace password_manager {

bool ParseLookupAffiliationResponse(
    const std::vector<FacetURI>& requested_facet_uris,
    const affiliation_pb::LookupAffiliationResponse& response,
    AffiliationFetcherDelegate::Result* result);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ANDROID_AFFILIATION_LOOKUP_AFFILIATION_RESPONSE_PARSER_H_
