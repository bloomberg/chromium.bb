// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PROTO_LEGACY_PROTO_BRIDGE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PROTO_LEGACY_PROTO_BRIDGE_H_

#include "components/autofill/core/browser/proto/api_v1.pb.h"
#include "components/autofill/core/browser/proto/server.pb.h"

namespace autofill {

// Creates an API request from a legacy request.
autofill::AutofillPageQueryRequest CreateApiRequestFromLegacyRequest(
    const AutofillQueryContents& legacy_request);

// Creates a legacy response from an API response.
AutofillQueryResponseContents CreateLegacyResponseFromApiResponse(
    const autofill::AutofillQueryResponse& api_response);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PROTO_LEGACY_PROTO_BRIDGE_H_
