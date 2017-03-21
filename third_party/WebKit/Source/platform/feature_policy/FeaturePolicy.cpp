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

WebFeaturePolicyFeature getWebFeaturePolicyFeature(const String& feature) {
  if (feature == "fullscreen")
    return WebFeaturePolicyFeature::Fullscreen;
  if (feature == "payment")
    return WebFeaturePolicyFeature::Payment;
  if (feature == "vibrate")
    return WebFeaturePolicyFeature::Vibrate;
  if (feature == "usermedia")
    return WebFeaturePolicyFeature::Usermedia;
  if (RuntimeEnabledFeatures::featurePolicyExperimentalFeaturesEnabled()) {
    if (feature == "cookie")
      return WebFeaturePolicyFeature::DocumentCookie;
    if (feature == "domain")
      return WebFeaturePolicyFeature::DocumentDomain;
    if (feature == "docwrite")
      return WebFeaturePolicyFeature::DocumentWrite;
    if (feature == "geolocation")
      return WebFeaturePolicyFeature::Geolocation;
    if (feature == "midi")
      return WebFeaturePolicyFeature::MidiFeature;
    if (feature == "notifications")
      return WebFeaturePolicyFeature::Notifications;
    if (feature == "push")
      return WebFeaturePolicyFeature::Push;
    if (feature == "sync-script")
      return WebFeaturePolicyFeature::SyncScript;
    if (feature == "sync-xhr")
      return WebFeaturePolicyFeature::SyncXHR;
    if (feature == "webrtc")
      return WebFeaturePolicyFeature::WebRTC;
  }
  return WebFeaturePolicyFeature::NotFound;
}

WebParsedFeaturePolicy parseFeaturePolicy(const String& policy,
                                          RefPtr<SecurityOrigin> origin,
                                          Vector<String>* messages) {
  Vector<WebParsedFeaturePolicyDeclaration> whitelists;

  // Use a reasonable parse depth limit; the actual maximum depth is only going
  // to be 4 for a valid policy, but we'll give the featurePolicyParser a chance
  // to report more specific errors, unless the string is really invalid.
  std::unique_ptr<JSONArray> policyItems = parseJSONHeader(policy, 50);
  if (!policyItems) {
    if (messages)
      messages->push_back("Unable to parse header.");
    return whitelists;
  }

  for (size_t i = 0; i < policyItems->size(); ++i) {
    JSONObject* item = JSONObject::cast(policyItems->at(i));
    if (!item) {
      if (messages)
        messages->push_back("Policy is not an object.");
      continue;  // Array element is not an object; skip
    }

    for (size_t j = 0; j < item->size(); ++j) {
      JSONObject::Entry entry = item->at(j);
      WebFeaturePolicyFeature feature = getWebFeaturePolicyFeature(entry.first);
      if (feature == WebFeaturePolicyFeature::NotFound)
        continue;  // Unrecognized feature; skip
      JSONArray* targets = JSONArray::cast(entry.second);
      if (!targets) {
        if (messages)
          messages->push_back("Whitelist is not an array of strings.");
        continue;
      }

      WebParsedFeaturePolicyDeclaration whitelist;
      whitelist.feature = feature;
      Vector<WebSecurityOrigin> origins;
      String targetString;
      for (size_t j = 0; j < targets->size(); ++j) {
        if (targets->at(j)->asString(&targetString)) {
          if (equalIgnoringCase(targetString, "self")) {
            if (!origin->isUnique())
              origins.push_back(origin);
          } else if (targetString == "*") {
            whitelist.matchesAllOrigins = true;
          } else {
            WebSecurityOrigin targetOrigin =
                WebSecurityOrigin::createFromString(targetString);
            if (!targetOrigin.isNull() && !targetOrigin.isUnique())
              origins.push_back(targetOrigin);
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
WebParsedFeaturePolicy getContainerPolicyFromAllowedFeatures(
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
