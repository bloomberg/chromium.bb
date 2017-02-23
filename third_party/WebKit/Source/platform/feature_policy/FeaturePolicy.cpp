// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/feature_policy/FeaturePolicy.h"

#include "platform/RuntimeEnabledFeatures.h"
#include "platform/json/JSONValues.h"
#include "platform/network/HTTPParsers.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "wtf/PtrUtil.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

namespace {

// Given a string name, return the matching feature struct, or nullptr if it is
// not the name of a policy-controlled feature.
const FeaturePolicy::Feature* featureForName(
    const String& featureName,
    FeaturePolicy::FeatureList& features) {
  for (const FeaturePolicy::Feature* feature : features) {
    if (featureName == feature->featureName)
      return feature;
  }
  return nullptr;
}

}  // namespace

// Definitions of all features controlled by Feature Policy should appear here.
const FeaturePolicy::Feature kDocumentCookie{
    "cookie", FeaturePolicy::FeatureDefault::EnableForAll};
const FeaturePolicy::Feature kDocumentDomain{
    "domain", FeaturePolicy::FeatureDefault::EnableForAll};
const FeaturePolicy::Feature kDocumentWrite{
    "docwrite", FeaturePolicy::FeatureDefault::EnableForAll};
const FeaturePolicy::Feature kFullscreenFeature{
    "fullscreen", FeaturePolicy::FeatureDefault::EnableForSelf};
const FeaturePolicy::Feature kGeolocationFeature{
    "geolocation", FeaturePolicy::FeatureDefault::EnableForSelf};
const FeaturePolicy::Feature kMidiFeature{
    "midi", FeaturePolicy::FeatureDefault::EnableForAll};
const FeaturePolicy::Feature kNotificationsFeature{
    "notifications", FeaturePolicy::FeatureDefault::EnableForAll};
const FeaturePolicy::Feature kPaymentFeature{
    "payment", FeaturePolicy::FeatureDefault::EnableForSelf};
const FeaturePolicy::Feature kPushFeature{
    "push", FeaturePolicy::FeatureDefault::EnableForAll};
const FeaturePolicy::Feature kSyncScript{
    "sync-script", FeaturePolicy::FeatureDefault::EnableForAll};
const FeaturePolicy::Feature kSyncXHR{
    "sync-xhr", FeaturePolicy::FeatureDefault::EnableForAll};
const FeaturePolicy::Feature kUsermedia{
    "usermedia", FeaturePolicy::FeatureDefault::EnableForAll};
const FeaturePolicy::Feature kVibrateFeature{
    "vibrate", FeaturePolicy::FeatureDefault::EnableForSelf};
const FeaturePolicy::Feature kWebRTC{
    "webrtc", FeaturePolicy::FeatureDefault::EnableForAll};

WebFeaturePolicyFeature FeaturePolicy::getWebFeaturePolicyFeature(
    const String& feature) {
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

// static
std::unique_ptr<FeaturePolicy::Whitelist> FeaturePolicy::Whitelist::from(
    const WebParsedFeaturePolicyDeclaration& parsedDeclaration) {
  std::unique_ptr<Whitelist> whitelist(new FeaturePolicy::Whitelist);
  if (parsedDeclaration.matchesAllOrigins) {
    whitelist->addAll();
  } else {
    for (const WebSecurityOrigin& origin : parsedDeclaration.origins)
      whitelist->add(static_cast<WTF::PassRefPtr<SecurityOrigin>>(origin));
  }
  return whitelist;
}

FeaturePolicy::Whitelist::Whitelist() : m_matchesAllOrigins(false) {}

void FeaturePolicy::Whitelist::addAll() {
  m_matchesAllOrigins = true;
}

void FeaturePolicy::Whitelist::add(RefPtr<SecurityOrigin> origin) {
  m_origins.push_back(std::move(origin));
}

bool FeaturePolicy::Whitelist::contains(const SecurityOrigin& origin) const {
  if (m_matchesAllOrigins)
    return true;
  for (const auto& targetOrigin : m_origins) {
    if (targetOrigin->isSameSchemeHostPortAndSuborigin(&origin))
      return true;
  }
  return false;
}

String FeaturePolicy::Whitelist::toString() {
  StringBuilder sb;
  sb.append("[");
  if (m_matchesAllOrigins) {
    sb.append("*");
  } else {
    for (size_t i = 0; i < m_origins.size(); ++i) {
      if (i > 0) {
        sb.append(", ");
      }
      sb.append(m_origins[i]->toString());
    }
  }
  sb.append("]");
  return sb.toString();
}

// static
const FeaturePolicy::FeatureList& FeaturePolicy::getDefaultFeatureList() {
  DEFINE_STATIC_LOCAL(
      Vector<const FeaturePolicy::Feature*>, defaultFeatureList,
      ({&kDocumentCookie, &kDocumentDomain, &kDocumentWrite,
        &kGeolocationFeature, &kFullscreenFeature, &kMidiFeature,
        &kNotificationsFeature, &kPaymentFeature, &kPushFeature, &kSyncScript,
        &kSyncXHR, &kUsermedia, &kVibrateFeature, &kWebRTC}));
  return defaultFeatureList;
}

// static
std::unique_ptr<FeaturePolicy> FeaturePolicy::createFromParentPolicy(
    const FeaturePolicy* parent,
    const WebParsedFeaturePolicyHeader* containerPolicy,
    RefPtr<SecurityOrigin> currentOrigin,
    FeaturePolicy::FeatureList& features) {
  DCHECK(currentOrigin);
  std::unique_ptr<FeaturePolicy> newPolicy =
      WTF::wrapUnique(new FeaturePolicy(currentOrigin, features));
  for (const FeaturePolicy::Feature* feature : features) {
    if (!parent ||
        parent->isFeatureEnabledForOrigin(*feature, *currentOrigin)) {
      newPolicy->m_inheritedFeatures.set(feature, true);
    } else {
      newPolicy->m_inheritedFeatures.set(feature, false);
    }
  }
  if (containerPolicy)
    newPolicy->addContainerPolicy(containerPolicy, parent);
  return newPolicy;
}

// static
std::unique_ptr<FeaturePolicy> FeaturePolicy::createFromParentPolicy(
    const FeaturePolicy* parent,
    const WebParsedFeaturePolicyHeader* containerPolicy,
    RefPtr<SecurityOrigin> currentOrigin) {
  return createFromParentPolicy(parent, containerPolicy,
                                std::move(currentOrigin),
                                getDefaultFeatureList());
}

void FeaturePolicy::addContainerPolicy(
    const WebParsedFeaturePolicyHeader* containerPolicy,
    const FeaturePolicy* parent) {
  DCHECK(containerPolicy);
  DCHECK(parent);
  for (const WebParsedFeaturePolicyDeclaration& parsedDeclaration :
       *containerPolicy) {
    // If a feature is enabled in the parent frame, and the parent chooses to
    // delegate it to the child frame, using the iframe attribute, then the
    // feature should be enabled in the child frame.
    const FeaturePolicy::Feature* feature =
        featureForName(parsedDeclaration.featureName, m_features);
    if (!feature)
      continue;
    if (Whitelist::from(parsedDeclaration)->contains(*m_origin) &&
        parent->isFeatureEnabled(*feature)) {
      m_inheritedFeatures.set(feature, true);
    } else {
      m_inheritedFeatures.set(feature, false);
    }
  }
}

// static
WebParsedFeaturePolicyHeader FeaturePolicy::parseFeaturePolicy(
    const String& policy,
    RefPtr<SecurityOrigin> origin,
    Vector<String>* messages) {
  Vector<WebParsedFeaturePolicyDeclaration> whitelists;

  // Use a reasonable parse depth limit; the actual maximum depth is only going
  // to be 4 for a valid policy, but we'll give the featurePolicyParser a chance
  // to report more specific errors, unless the string is really invalid.
  std::unique_ptr<JSONArray> policyItems = parseJSONHeader(policy, 50);
  if (!policyItems) {
    if (messages)
      messages->push_back("Unable to parse header");
    return whitelists;
  }

  for (size_t i = 0; i < policyItems->size(); ++i) {
    JSONObject* item = JSONObject::cast(policyItems->at(i));
    if (!item) {
      if (messages)
        messages->push_back("Policy is not an object");
      continue;  // Array element is not an object; skip
    }

    for (size_t j = 0; j < item->size(); ++j) {
      JSONObject::Entry entry = item->at(j);
      String featureName = entry.first;
      JSONArray* targets = JSONArray::cast(entry.second);
      if (!targets) {
        if (messages)
          messages->push_back("Whitelist is not an array of strings.");
        continue;
      }

      WebParsedFeaturePolicyDeclaration whitelist;
      whitelist.featureName = featureName;
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

void FeaturePolicy::setHeaderPolicy(
    const WebParsedFeaturePolicyHeader& policy) {
  DCHECK(m_headerWhitelists.isEmpty());
  for (const WebParsedFeaturePolicyDeclaration& parsedDeclaration : policy) {
    const FeaturePolicy::Feature* feature =
        featureForName(parsedDeclaration.featureName, m_features);
    if (!feature)
      continue;
    m_headerWhitelists.set(feature, Whitelist::from(parsedDeclaration));
  }
}

bool FeaturePolicy::isFeatureEnabledForOrigin(
    const FeaturePolicy::Feature& feature,
    const SecurityOrigin& origin) const {
  DCHECK(m_inheritedFeatures.contains(&feature));
  if (!m_inheritedFeatures.at(&feature)) {
    return false;
  }
  if (m_headerWhitelists.contains(&feature)) {
    return m_headerWhitelists.at(&feature)->contains(origin);
  }
  if (feature.defaultPolicy == FeaturePolicy::FeatureDefault::EnableForAll) {
    return true;
  }
  if (feature.defaultPolicy == FeaturePolicy::FeatureDefault::EnableForSelf) {
    return m_origin->isSameSchemeHostPortAndSuborigin(&origin);
  }
  return false;
}

bool FeaturePolicy::isFeatureEnabled(
    const FeaturePolicy::Feature& feature) const {
  DCHECK(m_origin);
  return isFeatureEnabledForOrigin(feature, *m_origin);
}

FeaturePolicy::FeaturePolicy(RefPtr<SecurityOrigin> currentOrigin,
                             FeaturePolicy::FeatureList& features)
    : m_origin(std::move(currentOrigin)), m_features(features) {}

String FeaturePolicy::toString() {
  StringBuilder sb;
  sb.append("Feature Policy for frame in origin: ");
  sb.append(m_origin->toString());
  sb.append("\n");
  sb.append("Inherited features:\n");
  for (const auto& inheritedFeature : m_inheritedFeatures) {
    sb.append("  ");
    sb.append(inheritedFeature.key->featureName);
    sb.append(": ");
    sb.append(inheritedFeature.value ? "true" : "false");
    sb.append("\n");
  }
  sb.append("Header whitelists:\n");
  for (const auto& whitelist : m_headerWhitelists) {
    sb.append("  ");
    sb.append(whitelist.key->featureName);
    sb.append(": ");
    sb.append(whitelist.value->toString());
    sb.append("\n");
  }
  return sb.toString();
}

}  // namespace blink
