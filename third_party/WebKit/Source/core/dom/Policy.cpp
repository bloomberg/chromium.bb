// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/Policy.h"

#include "core/dom/Document.h"
#include "core/inspector/ConsoleMessage.h"
#include "platform/feature_policy/FeaturePolicy.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/wtf/text/StringUTF8Adaptor.h"
#include "third_party/WebKit/common/feature_policy/feature_policy.h"

namespace blink {

// static
Policy* Policy::Create(Document* document) {
  return new Policy(document);
}

// explicit
Policy::Policy(Document* document) : document_(document) {}

bool Policy::allowsFeature(const String& feature) const {
  if (GetDefaultFeatureNameMap().Contains(feature)) {
    return document_->GetFeaturePolicy()->IsFeatureEnabled(
        GetDefaultFeatureNameMap().at(feature));
  }

  AddWarningForUnrecognizedFeature(feature);
  return false;
}

bool Policy::allowsFeature(const String& feature, const String& url) const {
  const scoped_refptr<SecurityOrigin> origin =
      SecurityOrigin::CreateFromString(url);
  if (!origin || origin->IsUnique()) {
    document_->AddConsoleMessage(ConsoleMessage::Create(
        kOtherMessageSource, kWarningMessageLevel,
        "Invalid origin url for feature '" + feature + "': " + url + "."));
    return false;
  }

  if (!GetDefaultFeatureNameMap().Contains(feature)) {
    AddWarningForUnrecognizedFeature(feature);
    return false;
  }

  return document_->GetFeaturePolicy()->IsFeatureEnabledForOrigin(
      GetDefaultFeatureNameMap().at(feature), origin->ToUrlOrigin());
}

Vector<String> Policy::allowedFeatures() const {
  Vector<String> allowed_features;
  for (const auto& entry : GetDefaultFeatureNameMap()) {
    if (document_->GetFeaturePolicy()->IsFeatureEnabled(entry.value))
      allowed_features.push_back(entry.key);
  }
  return allowed_features;
}

Vector<String> Policy::getAllowlistForFeature(const String& feature) const {
  if (GetDefaultFeatureNameMap().Contains(feature)) {
    const FeaturePolicy::Whitelist whitelist =
        document_->GetFeaturePolicy()->GetWhitelistForFeature(
            GetDefaultFeatureNameMap().at(feature));
    if (whitelist.MatchesAll())
      return Vector<String>({"*"});
    Vector<String> allowlist;
    for (const auto& origin : whitelist.Origins()) {
      allowlist.push_back(WTF::String::FromUTF8(origin.Serialize().c_str()));
    }
    return allowlist;
  }

  AddWarningForUnrecognizedFeature(feature);
  return Vector<String>();
}

void Policy::AddWarningForUnrecognizedFeature(const String& feature) const {
  document_->AddConsoleMessage(
      ConsoleMessage::Create(kOtherMessageSource, kWarningMessageLevel,
                             "Unrecognized feature: '" + feature + "'."));
}

void Policy::Trace(blink::Visitor* visitor) {
  visitor->Trace(document_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
