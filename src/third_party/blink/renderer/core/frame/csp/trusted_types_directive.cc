// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/csp/trusted_types_directive.h"

#include "third_party/blink/renderer/platform/network/content_security_policy_parsers.h"
#include "third_party/blink/renderer/platform/wtf/text/parsing_utilities.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

bool IsNotPolicyNameChar(UChar c) {
  // This implements the negation of one char of tt-policy-name from
  // https://w3c.github.io/webappsec-trusted-types/dist/spec/#trusted-types-csp-directive/
  bool is_name_char = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') ||
                      (c >= 'A' && c <= 'Z') || c == '-' || c == '#' ||
                      c == '=' || c == '_' || c == '/' || c == '@' ||
                      c == '.' || c == '%';
  return !is_name_char;
}

bool IsPolicyName(const String& name) {
  // This implements tt-policy-name from
  // https://w3c.github.io/webappsec-trusted-types/dist/spec/#trusted-types-csp-directive/
  return name.Find(&IsNotPolicyNameChar) == kNotFound;
}

}  // namespace

network::mojom::blink::CSPTrustedTypesPtr CSPTrustedTypesParse(
    const String& value,
    ContentSecurityPolicy* policy) {
  auto result = network::mojom::blink::CSPTrustedTypes::New();

  // Turn whitespace-y characters into ' ' and then split on ' ' into list_.
  value.SimplifyWhiteSpace().Split(' ', false, result->list);

  auto drop_fn = [policy, &result](const String& src) -> bool {
    DCHECK_EQ(src, src.StripWhiteSpace());
    // Handle keywords and special tokens first:
    if (src == "'allow-duplicates'") {
      result->allow_duplicates = true;
      return true;
    }
    if (src == "*") {
      result->allow_any = true;
      return true;
    }
    if (src == "'none'") {
      if (result->list.size() > 1) {
        policy->ReportInvalidSourceExpression("trusted-types", src);
      }
      return true;
    }
    return !IsPolicyName(src);
  };

  // There appears to be no wtf::Vector equivalent to STLs erase(from, to)
  // method, so we can't do the canonical .erase(remove_if(..), end) and have
  // to emulate this:
  result->list.Shrink(
      std::remove_if(result->list.begin(), result->list.end(), drop_fn) -
      result->list.begin());

  return result;
}

bool CSPTrustedTypesAllows(
    const network::mojom::blink::CSPTrustedTypes& trusted_types,
    const String& value,
    bool is_duplicate,
    ContentSecurityPolicy::AllowTrustedTypePolicyDetails& violation_details) {
  if (is_duplicate && !trusted_types.allow_duplicates) {
    violation_details = ContentSecurityPolicy::AllowTrustedTypePolicyDetails::
        kDisallowedDuplicateName;
  } else if (is_duplicate && value == "default") {
    violation_details = ContentSecurityPolicy::AllowTrustedTypePolicyDetails::
        kDisallowedDuplicateName;
  } else if (!IsPolicyName(value)) {
    violation_details =
        ContentSecurityPolicy::AllowTrustedTypePolicyDetails::kDisallowedName;
  } else if (!(trusted_types.allow_any || trusted_types.list.Contains(value))) {
    violation_details =
        ContentSecurityPolicy::AllowTrustedTypePolicyDetails::kDisallowedName;
  } else {
    violation_details =
        ContentSecurityPolicy::AllowTrustedTypePolicyDetails::kAllowed;
  }
  return violation_details ==
         ContentSecurityPolicy::AllowTrustedTypePolicyDetails::kAllowed;
}

}  // namespace blink
