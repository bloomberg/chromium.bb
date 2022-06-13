// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/client_hints/common/client_hints.h"

#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/blink/public/common/client_hints/enabled_client_hints.h"
#include "url/gurl.h"

namespace client_hints {

void GetAllowedClientHintsFromSource(
    const GURL& url,
    const ContentSettingsForOneType& client_hints_rules,
    blink::EnabledClientHints* client_hints) {
  if (client_hints_rules.empty())
    return;

  if (!network::IsUrlPotentiallyTrustworthy(url))
    return;

  const GURL& origin = url.DeprecatedGetOriginAsURL();

  for (const auto& rule : client_hints_rules) {
    // Look for an exact match since persisted client hints are disabled by
    // default, and enabled only on per-host basis.
    if (rule.primary_pattern == ContentSettingsPattern::Wildcard() ||
        !rule.primary_pattern.Matches(origin)) {
      continue;
    }

    // Found an exact match.
    DCHECK(ContentSettingsPattern::Wildcard() == rule.secondary_pattern);
    DCHECK(rule.setting_value.is_dict());

    const base::Value* list_value =
        rule.setting_value.FindKey(kClientHintsSettingKey);
    if (list_value == nullptr)
      continue;
    DCHECK(list_value->is_list());
    for (const auto& client_hint : list_value->GetList()) {
      DCHECK(client_hint.is_int());
      network::mojom::WebClientHintsType client_hint_mojo =
          static_cast<network::mojom::WebClientHintsType>(client_hint.GetInt());
      if (network::mojom::IsKnownEnumValue(client_hint_mojo))
        client_hints->SetIsEnabled(client_hint_mojo, true);
    }
    // Match found for |url| and client hints have been set.
    return;
  }
}

}  // namespace client_hints
