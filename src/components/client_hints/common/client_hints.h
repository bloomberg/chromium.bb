// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CLIENT_HINTS_COMMON_CLIENT_HINTS_H_
#define COMPONENTS_CLIENT_HINTS_COMMON_CLIENT_HINTS_H_

#include "components/content_settings/core/common/content_settings.h"

class GURL;

namespace blink {
struct WebEnabledClientHints;
}

namespace client_hints {

// Retrieves the persistent client hints that should be set when fetching a
// resource from |url|. The method updates |client_hints| with the result.
// |client_hints_rules| contains the content settings for the client hints.
void GetAllowedClientHintsFromSource(
    const GURL& url,
    const ContentSettingsForOneType& client_hints_rules,
    blink::WebEnabledClientHints* client_hints);

}  // namespace client_hints

#endif  // COMPONENTS_CLIENT_HINTS_COMMON_CLIENT_HINTS_H_
