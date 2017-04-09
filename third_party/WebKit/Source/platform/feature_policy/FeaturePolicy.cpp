// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/feature_policy/FeaturePolicy.h"

#include "platform/RuntimeEnabledFeatures.h"
#include "platform/json/JSONValues.h"
#include "platform/network/HTTPParsers.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "wtf/PtrUtil.h"

namespace blink {

WebFeaturePolicyFeature GetWebFeaturePolicyFeature(const String& feature) {
  if (feature == "fullscreen")
    return WebFeaturePolicyFeature::kFullscreen;
  if (feature == "payment")
    return WebFeaturePolicyFeature::kPayment;
  if (feature == "vibrate")
    return WebFeaturePolicyFeature::kVibrate;
  if (RuntimeEnabledFeatures::featurePolicyExperimentalFeaturesEnabled()) {
    if (feature == "camera")
      return WebFeaturePolicyFeature::kCamera;
    if (feature == "eme")
      return WebFeaturePolicyFeature::kEme;
    if (feature == "microphone")
      return WebFeaturePolicyFeature::kMicrophone;
    if (feature == "speaker")
      return WebFeaturePolicyFeature::kSpeaker;
    if (feature == "cookie")
      return WebFeaturePolicyFeature::kDocumentCookie;
    if (feature == "domain")
      return WebFeaturePolicyFeature::kDocumentDomain;
    if (feature == "docwrite")
      return WebFeaturePolicyFeature::kDocumentWrite;
    if (feature == "geolocation")
      return WebFeaturePolicyFeature::kGeolocation;
    if (feature == "midi")
      return WebFeaturePolicyFeature::kMidiFeature;
    if (feature == "notifications")
      return WebFeaturePolicyFeature::kNotifications;
    if (feature == "push")
      return WebFeaturePolicyFeature::kPush;
    if (feature == "sync-script")
      return WebFeaturePolicyFeature::kSyncScript;
    if (feature == "sync-xhr")
      return WebFeaturePolicyFeature::kSyncXHR;
    if (feature == "webrtc")
      return WebFeaturePolicyFeature::kWebRTC;
  }
  return WebFeaturePolicyFeature::kNotFound;
}

WebParsedFeaturePolicy ParseFeaturePolicy(const String& policy,
                                          RefPtr<SecurityOrigin> origin,
                                          Vector<String>* messages) {
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
      WebFeaturePolicyFeature feature = GetWebFeaturePolicyFeature(entry.first);
      if (feature == WebFeaturePolicyFeature::kNotFound)
        continue;  // Unrecognized feature; skip
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
          if (EqualIgnoringCase(target_string, "self")) {
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

// TODO(lunalu): also take information of allowfullscreen and
// allowpaymentrequest into account when constructing the whitelist.
WebParsedFeaturePolicy GetContainerPolicyFromAllowedFeatures(
    const WebVector<WebFeaturePolicyFeature>& features,
    RefPtr<SecurityOrigin> origin) {
  Vector<WebParsedFeaturePolicyDeclaration> whitelists;
  for (const WebFeaturePolicyFeature feature : features) {
    WebParsedFeaturePolicyDeclaration whitelist;
    whitelist.feature = feature;
    whitelist.origins = Vector<WebSecurityOrigin>(1UL, {origin});
    whitelists.push_back(whitelist);
  }
  return whitelists;
}

}  // namespace blink
