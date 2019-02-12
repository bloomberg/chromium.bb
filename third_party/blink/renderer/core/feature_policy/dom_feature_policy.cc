// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/feature_policy/dom_feature_policy.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/feature_policy/feature_policy.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"

namespace blink {

bool DOMFeaturePolicy::allowsFeature(const String& feature) const {
  if (GetDefaultFeatureNameMap().Contains(feature)) {
    auto feature_name = GetDefaultFeatureNameMap().at(feature);
    mojom::PolicyValueType feature_type =
        GetPolicy()->GetFeatureList().at(feature_name).second;
    PolicyValue value = PolicyValue::CreateMaxPolicyValue(feature_type);
    return GetPolicy()->IsFeatureEnabled(feature_name, value);
  }

  AddWarningForUnrecognizedFeature(feature);
  return false;
}

bool DOMFeaturePolicy::allowsFeature(const String& feature,
                                     const String& url) const {
  scoped_refptr<const SecurityOrigin> origin =
      SecurityOrigin::CreateFromString(url);
  if (!origin || origin->IsOpaque()) {
    GetDocument()->AddConsoleMessage(ConsoleMessage::Create(
        kOtherMessageSource, kWarningMessageLevel,
        "Invalid origin url for feature '" + feature + "': " + url + "."));
    return false;
  }

  if (!GetDefaultFeatureNameMap().Contains(feature)) {
    AddWarningForUnrecognizedFeature(feature);
    return false;
  }

  auto feature_name = GetDefaultFeatureNameMap().at(feature);
  mojom::PolicyValueType feature_type =
      GetPolicy()->GetFeatureList().at(feature_name).second;
  PolicyValue value = PolicyValue::CreateMaxPolicyValue(feature_type);
  return GetPolicy()->IsFeatureEnabledForOrigin(feature_name,
                                                origin->ToUrlOrigin(), value);
}

Vector<String> DOMFeaturePolicy::features() const {
  Vector<String> features;
  for (const auto& entry : GetDefaultFeatureNameMap())
    features.push_back(entry.key);
  return features;
}

Vector<String> DOMFeaturePolicy::allowedFeatures() const {
  Vector<String> allowed_features;
  for (const auto& entry : GetDefaultFeatureNameMap()) {
    auto feature_name = entry.value;
    mojom::PolicyValueType feature_type =
        GetPolicy()->GetFeatureList().at(feature_name).second;
    PolicyValue value = PolicyValue::CreateMaxPolicyValue(feature_type);
    if (GetPolicy()->IsFeatureEnabled(feature_name, value))
      allowed_features.push_back(entry.key);
  }
  return allowed_features;
}

Vector<String> DOMFeaturePolicy::getAllowlistForFeature(
    const String& feature) const {
  if (GetDefaultFeatureNameMap().Contains(feature)) {
    auto feature_name = GetDefaultFeatureNameMap().at(feature);
    auto feature_type = GetPolicy()->GetFeatureList().at(feature_name).second;

    const FeaturePolicy::Allowlist allowlist =
        GetPolicy()->GetAllowlistForFeature(feature_name);
    auto values = allowlist.Values();
    PolicyValue max_value = PolicyValue::CreateMaxPolicyValue(feature_type);
    if (values.empty()) {
      if (allowlist.GetFallbackValue().Type() !=
              mojom::PolicyValueType::kNull &&
          allowlist.GetFallbackValue() >= max_value)
        return Vector<String>({"*"});
    }
    Vector<String> result;
    for (const auto& entry : values) {
      result.push_back(WTF::String::FromUTF8(entry.first.Serialize().c_str()));
    }
    return result;
  }

  AddWarningForUnrecognizedFeature(feature);
  return Vector<String>();
}

void DOMFeaturePolicy::AddWarningForUnrecognizedFeature(
    const String& feature) const {
  GetDocument()->AddConsoleMessage(
      ConsoleMessage::Create(kOtherMessageSource, kWarningMessageLevel,
                             "Unrecognized feature: '" + feature + "'."));
}

void DOMFeaturePolicy::Trace(blink::Visitor* visitor) {
  ScriptWrappable::Trace(visitor);
}

void DOMFeaturePolicy::UpdateContainerPolicy(
    const ParsedFeaturePolicy& container_policy,
    scoped_refptr<const SecurityOrigin> src_origin) {}

}  // namespace blink
