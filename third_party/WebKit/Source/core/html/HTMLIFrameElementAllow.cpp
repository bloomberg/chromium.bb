// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/HTMLIFrameElementAllow.h"

#include "core/html/HTMLIFrameElement.h"
#include "platform/feature_policy/FeaturePolicy.h"
#include "platform/wtf/text/StringBuilder.h"

using blink::WebFeaturePolicyFeature;

namespace blink {

HTMLIFrameElementAllow::HTMLIFrameElementAllow(HTMLIFrameElement* element)
    : DOMTokenList(this), element_(element) {}

HTMLIFrameElementAllow::~HTMLIFrameElementAllow() {}

DEFINE_TRACE(HTMLIFrameElementAllow) {
  visitor->Trace(element_);
  DOMTokenList::Trace(visitor);
  DOMTokenListObserver::Trace(visitor);
}

Vector<WebFeaturePolicyFeature>
HTMLIFrameElementAllow::ParseAllowedFeatureNames(
    String& invalid_tokens_error_message) const {
  Vector<WebFeaturePolicyFeature> feature_names;
  unsigned num_token_errors = 0;
  StringBuilder token_errors;
  const SpaceSplitString& tokens = this->Tokens();

  // Collects a list of valid feature names.
  for (size_t i = 0; i < tokens.size(); ++i) {
    WebFeaturePolicyFeature feature = GetWebFeaturePolicyFeature(tokens[i]);
    if (feature == WebFeaturePolicyFeature::kNotFound) {
      token_errors.Append(token_errors.IsEmpty() ? "'" : ", '");
      token_errors.Append(tokens[i]);
      token_errors.Append("'");
      ++num_token_errors;
    } else {
      feature_names.push_back(feature);
    }
  }

  if (num_token_errors) {
    token_errors.Append(num_token_errors > 1 ? " are invalid feature names."
                                             : " is an invalid feature name.");
    invalid_tokens_error_message = token_errors.ToString();
  }

  // Create a unique set of feature names.
  std::sort(feature_names.begin(), feature_names.end());
  auto it = std::unique(feature_names.begin(), feature_names.end());
  feature_names.Shrink(it - feature_names.begin());

  return feature_names;
}

bool HTMLIFrameElementAllow::ValidateTokenValue(const AtomicString& token_value,
                                                ExceptionState&) const {
  return GetWebFeaturePolicyFeature(token_value.GetString()) !=
         WebFeaturePolicyFeature::kNotFound;
}

void HTMLIFrameElementAllow::ValueWasSet() {
  DCHECK(element_);
  element_->AllowValueWasSet();
}

}  // namespace blink
