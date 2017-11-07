// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/feature_policy/FeaturePolicy.h"

#include "platform/json/JSONValues.h"
#include "platform/network/HTTPParsers.h"
#include "platform/runtime_enabled_features.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/wtf/ASCIICType.h"
#include "platform/wtf/BitVector.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/text/ParsingUtilities.h"
#include "platform/wtf/text/StringUTF8Adaptor.h"
#include "url/gurl.h"

namespace blink {

namespace {

// TODO(loonybear): Deprecate the methods in this namesapce when deprecating old
// allow syntax.
bool IsValidOldAllowSyntax(const String& policy,
                           scoped_refptr<SecurityOrigin> src_origin) {
  // Old syntax enable all features on src_origin, If src_origin does not exist
  // (example, http header does not have a src_origin), then the syntax cannot
  // be valid.
  if (!src_origin)
    return false;
  // allow = "feature" is also supported by new syntax.
  if (!policy.Contains(' '))
    return false;
  // Old syntax only allows whitespace as valid delimiter.
  if (policy.Contains(';') || policy.Contains(','))
    return false;
  // An empty policy is also allowed in the new syntax.
  if (policy.ContainsOnlyWhitespace())
    return false;
  // Old syntax does not support specifying wildcards / origins for any feature.
  if (policy.Contains("self") || policy.Contains("src") ||
      policy.Contains("none") || policy.Contains("*")) {
    return false;
  }

  // Verify that the policy follows this syntax:
  //     allow = "name1 name2 name3 ...", name* = 1*( ALPHA / DIGIT / "-" )
  auto IsValidFeatureNameCharacter = [](auto c) {
    return IsASCIIAlphanumeric(c) || c == '-';
  };

  for (unsigned i = 0; i < policy.length(); i++) {
    if (!IsValidFeatureNameCharacter(policy[i]) && !IsASCIISpace(policy[i]))
      return false;
  }
  return true;
}

ParsedFeaturePolicy ParseOldAllowSyntax(const String& policy,
                                        const url::Origin& origin,
                                        Vector<String>* messages,
                                        const FeatureNameMap& feature_names) {
  ParsedFeaturePolicy whitelists;
  if (messages) {
    messages->push_back(
        "The old syntax (allow=\"feature1 feature2 feature3 ...\") will soon "
        "be deprecated");
  }
  Vector<String> tokens;
  policy.Split(' ', tokens);
  for (const String& token : tokens) {
    if (!feature_names.Contains(token)) {
      if (messages)
        messages->push_back("Unrecognized feature: '" + token + "'.");
      continue;
    }
    ParsedFeaturePolicyDeclaration whitelist;
    whitelist.feature = feature_names.at(token);
    whitelist.origins = {origin};
    whitelists.push_back(whitelist);
  }
  return whitelists;
}

}  // namespace

ParsedFeaturePolicy ParseFeaturePolicyHeader(
    const String& policy,
    scoped_refptr<SecurityOrigin> origin,
    Vector<String>* messages) {
  return ParseFeaturePolicy(policy, origin, nullptr, messages,
                            GetDefaultFeatureNameMap());
}

ParsedFeaturePolicy ParseFeaturePolicyAttribute(
    const String& policy,
    scoped_refptr<SecurityOrigin> self_origin,
    scoped_refptr<SecurityOrigin> src_origin,
    Vector<String>* messages,
    bool* old_syntax) {
  return ParseFeaturePolicy(policy, self_origin, src_origin, messages,
                            GetDefaultFeatureNameMap(), old_syntax);
}

ParsedFeaturePolicy ParseFeaturePolicy(
    const String& policy,
    scoped_refptr<SecurityOrigin> self_origin,
    scoped_refptr<SecurityOrigin> src_origin,
    Vector<String>* messages,
    const FeatureNameMap& feature_names,
    bool* old_syntax) {
  // Temporarily supporting old allow syntax:
  //     allow = "feature1 feature2 feature3 ... "
  // TODO(loonybear): depracate this old syntax in the future.
  if (IsValidOldAllowSyntax(policy, src_origin)) {
    if (old_syntax)
      *old_syntax = true;
    return ParseOldAllowSyntax(policy, src_origin->ToUrlOrigin(), messages,
                               feature_names);
  }

  ParsedFeaturePolicy whitelists;
  BitVector features_specified(
      static_cast<int>(FeaturePolicyFeature::LAST_FEATURE));

  // RFC2616, section 4.2 specifies that headers appearing multiple times can be
  // combined with a comma. Walk the header string, and parse each comma
  // separated chunk as a separate header.
  Vector<String> policy_items;
  // policy_items = [ policy *( "," [ policy ] ) ]
  policy.Split(',', policy_items);
  for (const String& item : policy_items) {
    Vector<String> entry_list;
    // entry_list = [ entry *( ";" [ entry ] ) ]
    item.Split(';', entry_list);
    for (const String& entry : entry_list) {
      // Split removes extra whitespaces by default
      //     "name value1 value2" or "name".
      Vector<String> tokens;
      entry.Split(' ', tokens);
      // Empty policy. Skip.
      if (tokens.IsEmpty())
        continue;
      if (!feature_names.Contains(tokens[0])) {
        if (messages)
          messages->push_back("Unrecognized feature: '" + tokens[0] + "'.");
        continue;
      }

      FeaturePolicyFeature feature = feature_names.at(tokens[0]);
      // If a policy has already been specified for the current feature, drop
      // the new policy.
      if (features_specified.QuickGet(static_cast<int>(feature)))
        continue;

      ParsedFeaturePolicyDeclaration whitelist;
      whitelist.feature = feature;
      features_specified.QuickSet(static_cast<int>(feature));
      std::vector<url::Origin> origins;
      // If a policy entry has no (optional) values (e,g,
      // allow="feature_name1; feature_name2 value"), enable the feature for:
      //     a. if header policy (i.e., src_origin does not exist), self_origin;
      //     or
      //     b. if allow attribute (i.e., src_origin exists), src_origin.
      if (tokens.size() == 1) {
        origins.push_back(src_origin ? src_origin->ToUrlOrigin()
                                     : self_origin->ToUrlOrigin());
      }

      for (size_t i = 1; i < tokens.size(); i++) {
        if (EqualIgnoringASCIICase(tokens[i], "'self'")) {
          origins.push_back(self_origin->ToUrlOrigin());
        } else if (EqualIgnoringASCIICase(tokens[i], "'src'")) {
          origins.push_back(src_origin->ToUrlOrigin());
        } else if (EqualIgnoringASCIICase(tokens[i], "'none'")) {
          continue;
        } else if (tokens[i] == "*") {
          whitelist.matches_all_origins = true;
          break;
        } else {
          url::Origin target_origin = url::Origin::Create(
              GURL(StringUTF8Adaptor(tokens[i]).AsStringPiece()));
          if (!target_origin.unique())
            origins.push_back(target_origin);
          else if (messages)
            messages->push_back("Unrecognized origin: '" + tokens[i] + "'.");
        }
      }
      whitelist.origins = origins;
      whitelists.push_back(whitelist);
    }
  }
  return whitelists;
}

bool IsSupportedInFeaturePolicy(FeaturePolicyFeature feature) {
  if (!RuntimeEnabledFeatures::FeaturePolicyEnabled())
    return false;
  switch (feature) {
    case FeaturePolicyFeature::kFullscreen:
    case FeaturePolicyFeature::kPayment:
    case FeaturePolicyFeature::kUsb:
    case FeaturePolicyFeature::kWebVr:
      return true;
    case FeaturePolicyFeature::kSyncXHR:
    case FeaturePolicyFeature::kVibrate:
      return RuntimeEnabledFeatures::FeaturePolicyExperimentalFeaturesEnabled();
    default:
      return false;
  }
}

const FeatureNameMap& GetDefaultFeatureNameMap() {
  DEFINE_STATIC_LOCAL(FeatureNameMap, default_feature_name_map, ());
  if (default_feature_name_map.IsEmpty()) {
    default_feature_name_map.Set("fullscreen",
                                 FeaturePolicyFeature::kFullscreen);
    default_feature_name_map.Set("payment", FeaturePolicyFeature::kPayment);
    default_feature_name_map.Set("usb", FeaturePolicyFeature::kUsb);
    default_feature_name_map.Set("camera", FeaturePolicyFeature::kCamera);
    default_feature_name_map.Set("encrypted-media",
                                 FeaturePolicyFeature::kEncryptedMedia);
    default_feature_name_map.Set("microphone",
                                 FeaturePolicyFeature::kMicrophone);
    default_feature_name_map.Set("speaker", FeaturePolicyFeature::kSpeaker);
    default_feature_name_map.Set("geolocation",
                                 FeaturePolicyFeature::kGeolocation);
    default_feature_name_map.Set("midi", FeaturePolicyFeature::kMidiFeature);
    default_feature_name_map.Set("vr", FeaturePolicyFeature::kWebVr);
    if (RuntimeEnabledFeatures::FeaturePolicyExperimentalFeaturesEnabled()) {
      default_feature_name_map.Set("vibrate", FeaturePolicyFeature::kVibrate);
      default_feature_name_map.Set("cookie",
                                   FeaturePolicyFeature::kDocumentCookie);
      default_feature_name_map.Set("domain",
                                   FeaturePolicyFeature::kDocumentDomain);
      default_feature_name_map.Set("docwrite",
                                   FeaturePolicyFeature::kDocumentWrite);
      default_feature_name_map.Set("sync-script",
                                   FeaturePolicyFeature::kSyncScript);
      default_feature_name_map.Set("sync-xhr", FeaturePolicyFeature::kSyncXHR);
    }
  }
  return default_feature_name_map;
}

}  // namespace blink
