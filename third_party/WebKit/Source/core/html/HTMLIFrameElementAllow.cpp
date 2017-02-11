// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/HTMLIFrameElementAllow.h"

#include "core/html/HTMLIFrameElement.h"
#include "platform/feature_policy/FeaturePolicy.h"

using blink::WebFeaturePolicyFeature;

namespace blink {

HTMLIFrameElementAllow::HTMLIFrameElementAllow(HTMLIFrameElement* element)
    : DOMTokenList(this), m_element(element) {}

HTMLIFrameElementAllow::~HTMLIFrameElementAllow() {}

DEFINE_TRACE(HTMLIFrameElementAllow) {
  visitor->trace(m_element);
  DOMTokenList::trace(visitor);
  DOMTokenListObserver::trace(visitor);
}

Vector<WebFeaturePolicyFeature>
HTMLIFrameElementAllow::parseAllowedFeatureNames(
    String& invalidTokensErrorMessage) const {
  Vector<WebFeaturePolicyFeature> featureNames;
  unsigned numTokenErrors = 0;
  StringBuilder tokenErrors;
  const SpaceSplitString& tokens = this->tokens();

  // Collects a list of valid feature names.
  for (size_t i = 0; i < tokens.size(); ++i) {
    WebFeaturePolicyFeature feature =
        FeaturePolicy::getWebFeaturePolicyFeature(tokens[i]);
    if (feature == WebFeaturePolicyFeature::NotFound) {
      tokenErrors.append(tokenErrors.isEmpty() ? "'" : ", '");
      tokenErrors.append(tokens[i]);
      tokenErrors.append("'");
      ++numTokenErrors;
    } else {
      featureNames.push_back(feature);
    }
  }

  if (numTokenErrors) {
    tokenErrors.append(numTokenErrors > 1 ? " are invalid feature names."
                                          : " is an invalid feature name.");
    invalidTokensErrorMessage = tokenErrors.toString();
  }

  // Create a unique set of feature names.
  std::sort(featureNames.begin(), featureNames.end());
  auto it = std::unique(featureNames.begin(), featureNames.end());
  featureNames.shrink(it - featureNames.begin());

  return featureNames;
}

bool HTMLIFrameElementAllow::validateTokenValue(const AtomicString& tokenValue,
                                                ExceptionState&) const {
  return FeaturePolicy::getWebFeaturePolicyFeature(tokenValue.getString()) !=
         WebFeaturePolicyFeature::NotFound;
}

void HTMLIFrameElementAllow::valueWasSet() {
  DCHECK(m_element);
  m_element->allowValueWasSet();
}

}  // namespace blink
