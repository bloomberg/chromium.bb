/*
 * Copyright (C) 2013 Google Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "third_party/blink/renderer/core/frame/sandbox_flags.h"

#include "third_party/blink/public/common/frame/sandbox_flags.h"
#include "third_party/blink/renderer/core/feature_policy/feature_policy.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

// An array of pairs of some sandbox flags and their corresponding
// FeaturePolicies that implement them. Eventually almost all sandbox flags
// should be converted to feature policies (https://crbug.com/812381).
using SandboxFlagFeaturePolicyPairs =
    Vector<std::pair<SandboxFlags, mojom::FeaturePolicyFeature>>;
const SandboxFlagFeaturePolicyPairs& SandboxFlagsWithFeaturePolicies() {
  DEFINE_STATIC_LOCAL(
      SandboxFlagFeaturePolicyPairs, array,
      ({{kSandboxTopNavigation, mojom::FeaturePolicyFeature::kTopNavigation},
        {kSandboxForms, mojom::FeaturePolicyFeature::kFormSubmission},
        {kSandboxScripts, mojom::FeaturePolicyFeature::kScript},
        {kSandboxPopups, mojom::FeaturePolicyFeature::kPopups},
        {kSandboxPointerLock, mojom::FeaturePolicyFeature::kPointerLock},
        {kSandboxModals, mojom::FeaturePolicyFeature::kModals},
        {kSandboxOrientationLock,
         mojom::FeaturePolicyFeature::kOrientationLock},
        {kSandboxPresentationController,
         mojom::FeaturePolicyFeature::kPresentation}}));
  return array;
}

// This returns a super mask which indicates the set of all flags that have
// corresponding feature policies. With FeaturePolicyForSandbox, these flags
// are always removed from the set of sandbox flags set for a sandboxed
// <iframe> (those sandbox flags are now contained in the |ContainerPolicy|).
SandboxFlags SandboxFlagsImplementedByFeaturePolicy() {
  DEFINE_STATIC_LOCAL(SandboxFlags, mask, (kSandboxNone));
  if (mask == kSandboxNone) {
    for (const auto& pair : SandboxFlagsWithFeaturePolicies())
      mask |= pair.first;
  }
  return mask;
}

SandboxFlags ParseSandboxPolicy(const SpaceSplitString& policy,
                                String& invalid_tokens_error_message) {
  // http://www.w3.org/TR/html5/the-iframe-element.html#attr-iframe-sandbox
  // Parse the unordered set of unique space-separated tokens.
  SandboxFlags flags = kSandboxAll;
  unsigned length = policy.size();
  unsigned number_of_token_errors = 0;
  StringBuilder token_errors;

  for (unsigned index = 0; index < length; index++) {
    // Turn off the corresponding sandbox flag if it's set as "allowed".
    String sandbox_token(policy[index]);
    if (EqualIgnoringASCIICase(sandbox_token, "allow-same-origin")) {
      flags &= ~kSandboxOrigin;
    } else if (EqualIgnoringASCIICase(sandbox_token, "allow-forms")) {
      flags &= ~kSandboxForms;
    } else if (EqualIgnoringASCIICase(sandbox_token, "allow-scripts")) {
      flags &= ~kSandboxScripts;
      flags &= ~kSandboxAutomaticFeatures;
    } else if (EqualIgnoringASCIICase(sandbox_token, "allow-top-navigation")) {
      flags &= ~kSandboxTopNavigation;
    } else if (EqualIgnoringASCIICase(sandbox_token, "allow-popups")) {
      flags &= ~kSandboxPopups;
    } else if (EqualIgnoringASCIICase(sandbox_token, "allow-pointer-lock")) {
      flags &= ~kSandboxPointerLock;
    } else if (EqualIgnoringASCIICase(sandbox_token,
                                      "allow-orientation-lock")) {
      flags &= ~kSandboxOrientationLock;
    } else if (EqualIgnoringASCIICase(sandbox_token,
                                      "allow-popups-to-escape-sandbox")) {
      flags &= ~kSandboxPropagatesToAuxiliaryBrowsingContexts;
    } else if (EqualIgnoringASCIICase(sandbox_token, "allow-modals")) {
      flags &= ~kSandboxModals;
    } else if (EqualIgnoringASCIICase(sandbox_token, "allow-presentation")) {
      flags &= ~kSandboxPresentationController;
    } else if (EqualIgnoringASCIICase(
                   sandbox_token, "allow-top-navigation-by-user-activation")) {
      flags &= ~kSandboxTopNavigationByUserActivation;
    } else if (EqualIgnoringASCIICase(
                   sandbox_token, "allow-downloads-without-user-activation")) {
      flags &= ~kSandboxDownloads;
    } else {
      token_errors.Append(token_errors.IsEmpty() ? "'" : ", '");
      token_errors.Append(sandbox_token);
      token_errors.Append("'");
      number_of_token_errors++;
    }
  }

  if (number_of_token_errors) {
    token_errors.Append(number_of_token_errors > 1
                            ? " are invalid sandbox flags."
                            : " is an invalid sandbox flag.");
    invalid_tokens_error_message = token_errors.ToString();
  }

  return flags;
}

// Removes a certain set of flags from |sandbox_flags| for which we have feature
// policies implemented.
SandboxFlags GetSandboxFlagsNotImplementedAsFeaturePolicy(
    SandboxFlags sandbox_flags) {
  // Punch all the sandbox flags which are converted to feature policy.
  return sandbox_flags & ~SandboxFlagsImplementedByFeaturePolicy();
}

void ApplySandboxFlagsToParsedFeaturePolicy(
    SandboxFlags sandbox_flags,
    ParsedFeaturePolicy& parsed_feature_policy) {
  for (const auto& pair : SandboxFlagsWithFeaturePolicies()) {
    if (sandbox_flags & pair.first)
      DisallowFeatureIfNotPresent(pair.second, parsed_feature_policy);
  }
}

STATIC_ASSERT_ENUM(WebSandboxFlags::kNone, kSandboxNone);
STATIC_ASSERT_ENUM(WebSandboxFlags::kNavigation, kSandboxNavigation);
STATIC_ASSERT_ENUM(WebSandboxFlags::kPlugins, kSandboxPlugins);
STATIC_ASSERT_ENUM(WebSandboxFlags::kOrigin, kSandboxOrigin);
STATIC_ASSERT_ENUM(WebSandboxFlags::kForms, kSandboxForms);
STATIC_ASSERT_ENUM(WebSandboxFlags::kScripts, kSandboxScripts);
STATIC_ASSERT_ENUM(WebSandboxFlags::kTopNavigation, kSandboxTopNavigation);
STATIC_ASSERT_ENUM(WebSandboxFlags::kPopups, kSandboxPopups);
STATIC_ASSERT_ENUM(WebSandboxFlags::kAutomaticFeatures,
                   kSandboxAutomaticFeatures);
STATIC_ASSERT_ENUM(WebSandboxFlags::kPointerLock, kSandboxPointerLock);
STATIC_ASSERT_ENUM(WebSandboxFlags::kDocumentDomain, kSandboxDocumentDomain);
STATIC_ASSERT_ENUM(WebSandboxFlags::kOrientationLock, kSandboxOrientationLock);
STATIC_ASSERT_ENUM(WebSandboxFlags::kPropagatesToAuxiliaryBrowsingContexts,
                   kSandboxPropagatesToAuxiliaryBrowsingContexts);
STATIC_ASSERT_ENUM(WebSandboxFlags::kModals, kSandboxModals);
STATIC_ASSERT_ENUM(WebSandboxFlags::kPresentationController,
                   kSandboxPresentationController);
STATIC_ASSERT_ENUM(WebSandboxFlags::kTopNavigationByUserActivation,
                   kSandboxTopNavigationByUserActivation);
STATIC_ASSERT_ENUM(WebSandboxFlags::kDownloads, kSandboxDownloads);

}  // namespace blink
