// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/feature_policy/feature_policy.h"

#include "third_party/blink/renderer/platform/json/json_values.h"
#include "third_party/blink/renderer/platform/network/http_parsers.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/ascii_ctype.h"
#include "third_party/blink/renderer/platform/wtf/bit_vector.h"
#include "third_party/blink/renderer/platform/wtf/text/parsing_utilities.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"
#include "url/gurl.h"

namespace blink {

namespace {

// TODO(loonybear): Deprecate the methods in this namesapce when deprecating old
// allow syntax.
bool IsValidOldAllowSyntax(const String& policy,
                           scoped_refptr<const SecurityOrigin> src_origin) {
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
        "The old syntax (allow=\"feature1 feature2 feature3 ...\") is "
        "deprecated and will be removed in Chrome 68. Use semicolons to "
        "separate features (allow=\"feature1; feature2; feature3; ...\").");
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
    scoped_refptr<const SecurityOrigin> origin,
    Vector<String>* messages) {
  return ParseFeaturePolicy(policy, origin, nullptr, messages,
                            GetDefaultFeatureNameMap());
}

ParsedFeaturePolicy ParseFeaturePolicyAttribute(
    const String& policy,
    scoped_refptr<const SecurityOrigin> self_origin,
    scoped_refptr<const SecurityOrigin> src_origin,
    Vector<String>* messages,
    bool* old_syntax) {
  return ParseFeaturePolicy(policy, self_origin, src_origin, messages,
                            GetDefaultFeatureNameMap(), old_syntax);
}

ParsedFeaturePolicy ParseFeaturePolicy(
    const String& policy,
    scoped_refptr<const SecurityOrigin> self_origin,
    scoped_refptr<const SecurityOrigin> src_origin,
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
      static_cast<int>(mojom::FeaturePolicyFeature::kMaxValue));

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

      mojom::FeaturePolicyFeature feature = feature_names.at(tokens[0]);
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
      //     a. |self_origin|, if we are parsing a header policy (i.e.,
      //       |src_origin| is null);
      //     b. |src_origin|, if we are parsing an allow attribute (i.e.,
      //       |src_origin| is not null), |src_origin| is not opaque; or
      //     c. the opaque origin of the frame, if |src_origin| is opaque.
      if (tokens.size() == 1) {
        if (!src_origin) {
          origins.push_back(self_origin->ToUrlOrigin());
        } else if (!src_origin->IsUnique()) {
          origins.push_back(src_origin->ToUrlOrigin());
        } else {
          whitelist.matches_opaque_src = true;
        }
      }

      for (size_t i = 1; i < tokens.size(); i++) {
        if (!tokens[i].ContainsOnlyASCII()) {
          messages->push_back("Non-ASCII characters in origin.");
          continue;
        }
        if (EqualIgnoringASCIICase(tokens[i], "'self'")) {
          origins.push_back(self_origin->ToUrlOrigin());
        } else if (src_origin && EqualIgnoringASCIICase(tokens[i], "'src'")) {
          // Only the iframe allow attribute can define |src_origin|.
          // When parsing feature policy header, 'src' is disallowed and
          // |src_origin| = nullptr.
          // If the iframe will have an opaque origin (for example, if it is
          // sandboxed, or has a data: URL), then 'src' needs to refer to the
          // opaque origin of the frame, which is not known yet. In this case,
          // the |matches_opaque_src| flag on the declaration is set, rather
          // than adding an origin to the allowlist.
          if (src_origin->IsUnique()) {
            whitelist.matches_opaque_src = true;
          } else {
            origins.push_back(src_origin->ToUrlOrigin());
          }
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

bool IsSupportedInFeaturePolicy(mojom::FeaturePolicyFeature feature) {
  switch (feature) {
    case mojom::FeaturePolicyFeature::kFullscreen:
    case mojom::FeaturePolicyFeature::kPayment:
    case mojom::FeaturePolicyFeature::kUsb:
    case mojom::FeaturePolicyFeature::kWebVr:
    case mojom::FeaturePolicyFeature::kAccelerometer:
    case mojom::FeaturePolicyFeature::kAmbientLightSensor:
    case mojom::FeaturePolicyFeature::kGyroscope:
    case mojom::FeaturePolicyFeature::kMagnetometer:
      return true;
    case mojom::FeaturePolicyFeature::kPictureInPicture:
      return RuntimeEnabledFeatures::PictureInPictureAPIEnabled();
    case mojom::FeaturePolicyFeature::kSyncXHR:
      return true;
    case mojom::FeaturePolicyFeature::kUnsizedMedia:
      return RuntimeEnabledFeatures::FeaturePolicyExperimentalFeaturesEnabled();
    case mojom::FeaturePolicyFeature::kVerticalScroll:
      return RuntimeEnabledFeatures::FeaturePolicyExperimentalFeaturesEnabled();
    default:
      return false;
  }
}

const FeatureNameMap& GetDefaultFeatureNameMap() {
  DEFINE_STATIC_LOCAL(FeatureNameMap, default_feature_name_map, ());
  if (default_feature_name_map.IsEmpty()) {
    default_feature_name_map.Set("fullscreen",
                                 mojom::FeaturePolicyFeature::kFullscreen);
    default_feature_name_map.Set("payment",
                                 mojom::FeaturePolicyFeature::kPayment);
    default_feature_name_map.Set("usb", mojom::FeaturePolicyFeature::kUsb);
    default_feature_name_map.Set("camera",
                                 mojom::FeaturePolicyFeature::kCamera);
    default_feature_name_map.Set("encrypted-media",
                                 mojom::FeaturePolicyFeature::kEncryptedMedia);
    default_feature_name_map.Set("microphone",
                                 mojom::FeaturePolicyFeature::kMicrophone);
    default_feature_name_map.Set("speaker",
                                 mojom::FeaturePolicyFeature::kSpeaker);
    default_feature_name_map.Set("geolocation",
                                 mojom::FeaturePolicyFeature::kGeolocation);
    default_feature_name_map.Set("midi",
                                 mojom::FeaturePolicyFeature::kMidiFeature);
    default_feature_name_map.Set("sync-xhr",
                                 mojom::FeaturePolicyFeature::kSyncXHR);
    default_feature_name_map.Set("vr", mojom::FeaturePolicyFeature::kWebVr);
    default_feature_name_map.Set("accelerometer",
                                 mojom::FeaturePolicyFeature::kAccelerometer);
    default_feature_name_map.Set(
        "ambient-light-sensor",
        mojom::FeaturePolicyFeature::kAmbientLightSensor);
    default_feature_name_map.Set("gyroscope",
                                 mojom::FeaturePolicyFeature::kGyroscope);
    default_feature_name_map.Set("magnetometer",
                                 mojom::FeaturePolicyFeature::kMagnetometer);
    if (RuntimeEnabledFeatures::PictureInPictureAPIEnabled()) {
      default_feature_name_map.Set(
          "picture-in-picture", mojom::FeaturePolicyFeature::kPictureInPicture);
    }
    if (RuntimeEnabledFeatures::FeaturePolicyExperimentalFeaturesEnabled()) {
      default_feature_name_map.Set(
          "cookie", mojom::FeaturePolicyFeature::kDocumentCookie);
      default_feature_name_map.Set(
          "domain", mojom::FeaturePolicyFeature::kDocumentDomain);
      default_feature_name_map.Set("docwrite",
                                   mojom::FeaturePolicyFeature::kDocumentWrite);
      default_feature_name_map.Set("sync-script",
                                   mojom::FeaturePolicyFeature::kSyncScript);
      default_feature_name_map.Set("unsized-media",
                                   mojom::FeaturePolicyFeature::kUnsizedMedia);
      default_feature_name_map.Set(
          "vertical-scroll", mojom::FeaturePolicyFeature::kVerticalScroll);
    }
    if (RuntimeEnabledFeatures::FeaturePolicyAutoplayFeatureEnabled()) {
      default_feature_name_map.Set("autoplay",
                                   mojom::FeaturePolicyFeature::kAutoplay);
    }
  }
  return default_feature_name_map;
}

}  // namespace blink
