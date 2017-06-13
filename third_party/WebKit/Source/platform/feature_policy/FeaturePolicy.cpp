// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/feature_policy/FeaturePolicy.h"

#include "platform/RuntimeEnabledFeatures.h"
#include "platform/json/JSONValues.h"
#include "platform/network/HTTPParsers.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/wtf/PtrUtil.h"

namespace blink {

WebParsedFeaturePolicy ParseFeaturePolicy(const String& policy,
                                          RefPtr<SecurityOrigin> origin,
                                          Vector<String>* messages) {
  return ParseFeaturePolicy(policy, origin, messages,
                            GetDefaultFeatureNameMap());
}

WebParsedFeaturePolicy ParseFeaturePolicy(const String& policy,
                                          RefPtr<SecurityOrigin> origin,
                                          Vector<String>* messages,
                                          const FeatureNameMap& feature_names) {
  Vector<WebParsedFeaturePolicyDeclaration> whitelists;

  // Use a reasonable parse depth limit; the actual maximum depth is only going
  // to be 4 for a valid policy, but we'll give the featurePolicyParser a chance
  // to report more specific errors, unless the string is really invalid.
  std::unique_ptr<JSONArray> policy_items = ParseJSONHeader(policy, 50);
  if (!policy_items) {
    if (messages)
      messages->push_back("Unable to parse header.");
    return whitelists;
  }

  for (size_t i = 0; i < policy_items->size(); ++i) {
    JSONObject* item = JSONObject::Cast(policy_items->at(i));
    if (!item) {
      if (messages)
        messages->push_back("Policy is not an object.");
      continue;  // Array element is not an object; skip
    }

    for (size_t j = 0; j < item->size(); ++j) {
      JSONObject::Entry entry = item->at(j);
      if (!feature_names.Contains(entry.first))
        continue;  // Unrecognized feature; skip
      WebFeaturePolicyFeature feature = feature_names.at(entry.first);
      JSONArray* targets = JSONArray::Cast(entry.second);
      if (!targets) {
        if (messages)
          messages->push_back("Whitelist is not an array of strings.");
        continue;
      }

      WebParsedFeaturePolicyDeclaration whitelist;
      whitelist.feature = feature;
      Vector<WebSecurityOrigin> origins;
      String target_string;
      for (size_t j = 0; j < targets->size(); ++j) {
        if (targets->at(j)->AsString(&target_string)) {
          if (EqualIgnoringASCIICase(target_string, "self")) {
            if (!origin->IsUnique())
              origins.push_back(origin);
          } else if (target_string == "*") {
            whitelist.matches_all_origins = true;
          } else {
            WebSecurityOrigin target_origin =
                WebSecurityOrigin::CreateFromString(target_string);
            if (!target_origin.IsNull() && !target_origin.IsUnique())
              origins.push_back(target_origin);
          }
        } else {
          if (messages)
            messages->push_back("Whitelist is not an array of strings.");
        }
      }
      whitelist.origins = origins;
      whitelists.push_back(whitelist);
    }
  }
  return whitelists;
}

bool IsSupportedInFeaturePolicy(WebFeaturePolicyFeature feature) {
  switch (feature) {
    // TODO(lunalu): Re-enabled fullscreen in feature policy once tests have
    // been updated.
    // crbug.com/666761
    case WebFeaturePolicyFeature::kFullscreen:
      return false;
    case WebFeaturePolicyFeature::kPayment:
      return true;
    case WebFeaturePolicyFeature::kVibrate:
      return RuntimeEnabledFeatures::FeaturePolicyExperimentalFeaturesEnabled();
    default:
      return false;
  }
}

const FeatureNameMap& GetDefaultFeatureNameMap() {
  DEFINE_STATIC_LOCAL(FeatureNameMap, default_feature_name_map, ());
  if (default_feature_name_map.IsEmpty()) {
    default_feature_name_map.Set("fullscreen",
                                 WebFeaturePolicyFeature::kFullscreen);
    default_feature_name_map.Set("payment", WebFeaturePolicyFeature::kPayment);
    default_feature_name_map.Set("usb", WebFeaturePolicyFeature::kUsb);
    if (RuntimeEnabledFeatures::FeaturePolicyExperimentalFeaturesEnabled()) {
      default_feature_name_map.Set("vibrate",
                                   WebFeaturePolicyFeature::kVibrate);
      default_feature_name_map.Set("camera", WebFeaturePolicyFeature::kCamera);
      default_feature_name_map.Set("encrypted-media",
                                   WebFeaturePolicyFeature::kEme);
      default_feature_name_map.Set("microphone",
                                   WebFeaturePolicyFeature::kMicrophone);
      default_feature_name_map.Set("speaker",
                                   WebFeaturePolicyFeature::kSpeaker);
      default_feature_name_map.Set("cookie",
                                   WebFeaturePolicyFeature::kDocumentCookie);
      default_feature_name_map.Set("domain",
                                   WebFeaturePolicyFeature::kDocumentDomain);
      default_feature_name_map.Set("docwrite",
                                   WebFeaturePolicyFeature::kDocumentWrite);
      default_feature_name_map.Set("geolocation",
                                   WebFeaturePolicyFeature::kGeolocation);
      default_feature_name_map.Set("midi",
                                   WebFeaturePolicyFeature::kMidiFeature);
      default_feature_name_map.Set("notifications",
                                   WebFeaturePolicyFeature::kNotifications);
      default_feature_name_map.Set("push", WebFeaturePolicyFeature::kPush);
      default_feature_name_map.Set("sync-script",
                                   WebFeaturePolicyFeature::kSyncScript);
      default_feature_name_map.Set("sync-xhr",
                                   WebFeaturePolicyFeature::kSyncXHR);
      default_feature_name_map.Set("webrtc", WebFeaturePolicyFeature::kWebRTC);
    }
  }
  return default_feature_name_map;
}

}  // namespace blink
