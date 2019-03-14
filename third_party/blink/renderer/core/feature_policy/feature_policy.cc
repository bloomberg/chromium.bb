// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/feature_policy/feature_policy.h"

#include <algorithm>
#include <map>
#include <utility>

#include "base/metrics/histogram_macros.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trial_context.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trials.h"
#include "third_party/blink/renderer/platform/json/json_values.h"
#include "third_party/blink/renderer/platform/network/http_parsers.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/bit_vector.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"
#include "url/origin.h"

namespace blink {

namespace {

// Returns true if this feature is currently disabled by an origin trial (it is
// origin trial controlled, and the origin trial is not enabled).
bool IsOriginTrialDisabled(const String& feature_name,
                           const ExecutionContext* execution_context) {
  bool (*origin_trial_enabled)(const blink::ExecutionContext*);

  // All origin trial controlled features should be listed here.
  if (feature_name == "frobulate") {
    origin_trial_enabled = origin_trials::OriginTrialsSampleAPIEnabled;
  } else {
    return false;
  }

  // Check whether this feature's origin trial is enabled, and block otherwise.
  DCHECK(origin_trial_enabled);
  return !origin_trial_enabled(execution_context);
}

// TODO(loonybear): once the new syntax is implemented, use this method to
// parse the policy value for each parameterized feature, and for non
// parameterized feature (i.e. boolean-type policy value).
PolicyValue GetFallbackValueForFeature(mojom::FeaturePolicyFeature feature) {
  if (feature == mojom::FeaturePolicyFeature::kOversizedImages)
    return PolicyValue(2.0, mojom::PolicyValueType::kDecDouble);
  return PolicyValue(false);
}

}  // namespace

ParsedFeaturePolicy ParseFeaturePolicyHeader(
    const String& policy,
    scoped_refptr<const SecurityOrigin> origin,
    Vector<String>* messages,
    ExecutionContext* execution_context) {
  return ParseFeaturePolicy(policy, origin, nullptr, messages,
                            GetDefaultFeatureNameMap(), execution_context);
}

ParsedFeaturePolicy ParseFeaturePolicyAttribute(
    const String& policy,
    scoped_refptr<const SecurityOrigin> self_origin,
    scoped_refptr<const SecurityOrigin> src_origin,
    Vector<String>* messages,
    Document* document) {
  return ParseFeaturePolicy(policy, self_origin, src_origin, messages,
                            GetDefaultFeatureNameMap(), document);
}

ParsedFeaturePolicy ParseFeaturePolicy(
    const String& policy,
    scoped_refptr<const SecurityOrigin> self_origin,
    scoped_refptr<const SecurityOrigin> src_origin,
    Vector<String>* messages,
    const FeatureNameMap& feature_names,
    ExecutionContext* execution_context) {
  ParsedFeaturePolicy allowlists;
  BitVector features_specified(
      static_cast<int>(mojom::FeaturePolicyFeature::kMaxValue) + 1);

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

      String feature_name = tokens[0];
      if (!feature_names.Contains(feature_name)) {
        if (messages) {
          messages->push_back("Unrecognized feature: '" + tokens[0] + "'.");
        }
        continue;
      }

      if (IsOriginTrialDisabled(feature_name, execution_context)) {
        if (messages) {
          messages->push_back("Origin trial controlled feature not enabled: '" +
                              tokens[0] + "'.");
        }
        continue;
      }

      mojom::FeaturePolicyFeature feature = feature_names.at(feature_name);
      mojom::PolicyValueType feature_type =
          FeaturePolicy::GetDefaultFeatureList().at(feature).second;
      // If a policy has already been specified for the current feature, drop
      // the new policy.
      if (features_specified.QuickGet(static_cast<int>(feature)))
        continue;

      // Count the use of this feature policy.
      if (src_origin) {
        Document* document = DynamicTo<Document>(execution_context);
        if (!document || !document->IsParsedFeaturePolicy(feature)) {
          UMA_HISTOGRAM_ENUMERATION("Blink.UseCounter.FeaturePolicy.Allow",
                                    feature);
          if (document) {
            document->SetParsedFeaturePolicy(feature);
          }
        }
      } else {
        UMA_HISTOGRAM_ENUMERATION("Blink.UseCounter.FeaturePolicy.Header",
                                  feature);
      }

      ParsedFeaturePolicyDeclaration allowlist(feature, feature_type);
      // TODO(loonybear): fallback value should be parsed from the new syntax.
      allowlist.fallback_value = GetFallbackValueForFeature(feature);
      allowlist.opaque_value = GetFallbackValueForFeature(feature);
      features_specified.QuickSet(static_cast<int>(feature));
      std::map<url::Origin, PolicyValue> values;
      PolicyValue value = PolicyValue::CreateMaxPolicyValue(feature_type);
      // If a policy entry has no listed origins (e.g. "feature_name1" in
      // allow="feature_name1; feature_name2 value"), enable the feature for:
      //     a. |self_origin|, if we are parsing a header policy (i.e.,
      //       |src_origin| is null);
      //     b. |src_origin|, if we are parsing an allow attribute (i.e.,
      //       |src_origin| is not null), |src_origin| is not opaque; or
      //     c. the opaque origin of the frame, if |src_origin| is opaque.
      if (tokens.size() == 1) {
        if (!src_origin) {
          values[self_origin->ToUrlOrigin()] = value;
        } else if (!src_origin->IsOpaque()) {
          values[src_origin->ToUrlOrigin()] = value;
        } else {
          allowlist.opaque_value = value;
        }
      }

      for (wtf_size_t i = 1; i < tokens.size(); i++) {
        // TODO(loonybear): for each token, parse the policy value from the
        // new syntax.
        if (!tokens[i].ContainsOnlyASCIIOrEmpty()) {
          messages->push_back("Non-ASCII characters in origin.");
          continue;
        }
        if (EqualIgnoringASCIICase(tokens[i], "'self'")) {
          values[self_origin->ToUrlOrigin()] = value;
        } else if (src_origin && EqualIgnoringASCIICase(tokens[i], "'src'")) {
          // Only the iframe allow attribute can define |src_origin|.
          // When parsing feature policy header, 'src' is disallowed and
          // |src_origin| = nullptr.
          // If the iframe will have an opaque origin (for example, if it is
          // sandboxed, or has a data: URL), then 'src' needs to refer to the
          // opaque origin of the frame, which is not known yet. In this case,
          // the |opaque_value| on the declaration is set, rather than adding
          // an origin to the allowlist.
          if (src_origin->IsOpaque()) {
            allowlist.opaque_value = value;
          } else {
            values[src_origin->ToUrlOrigin()] = value;
          }
        } else if (EqualIgnoringASCIICase(tokens[i], "'none'")) {
          continue;
        } else if (tokens[i] == "*") {
          allowlist.fallback_value = value;
          allowlist.opaque_value = value;
          break;
        } else {
          scoped_refptr<SecurityOrigin> target_origin =
              SecurityOrigin::CreateFromString(tokens[i]);
          if (!target_origin->IsOpaque())
            values[target_origin->ToUrlOrigin()] = value;
          else if (messages)
            messages->push_back("Unrecognized origin: '" + tokens[i] + "'.");
        }
      }
      allowlist.values = std::move(values);
      allowlists.push_back(allowlist);
    }
  }
  return allowlists;
}

bool IsFeatureDeclared(mojom::FeaturePolicyFeature feature,
                       const ParsedFeaturePolicy& policy) {
  return std::any_of(policy.begin(), policy.end(),
                     [feature](const auto& declaration) {
                       return declaration.feature == feature;
                     });
}

bool RemoveFeatureIfPresent(mojom::FeaturePolicyFeature feature,
                            ParsedFeaturePolicy& policy) {
  auto new_end = std::remove_if(policy.begin(), policy.end(),
                                [feature](const auto& declaration) {
                                  return declaration.feature == feature;
                                });
  if (new_end == policy.end())
    return false;
  policy.erase(new_end, policy.end());
  return true;
}

bool DisallowFeatureIfNotPresent(mojom::FeaturePolicyFeature feature,
                                 ParsedFeaturePolicy& policy) {
  if (IsFeatureDeclared(feature, policy))
    return false;
  blink::mojom::PolicyValueType feature_type =
      blink::FeaturePolicy::GetDefaultFeatureList().at(feature).second;
  ParsedFeaturePolicyDeclaration allowlist(feature, feature_type);
  policy.push_back(allowlist);
  return true;
}

bool AllowFeatureEverywhereIfNotPresent(mojom::FeaturePolicyFeature feature,
                                        ParsedFeaturePolicy& policy) {
  if (IsFeatureDeclared(feature, policy))
    return false;
  blink::mojom::PolicyValueType feature_type =
      blink::FeaturePolicy::GetDefaultFeatureList().at(feature).second;
  ParsedFeaturePolicyDeclaration allowlist(feature, feature_type);
  allowlist.fallback_value.SetToMax();
  allowlist.opaque_value.SetToMax();
  policy.push_back(allowlist);
  return true;
}

void DisallowFeature(mojom::FeaturePolicyFeature feature,
                     ParsedFeaturePolicy& policy) {
  RemoveFeatureIfPresent(feature, policy);
  DisallowFeatureIfNotPresent(feature, policy);
}

void AllowFeatureEverywhere(mojom::FeaturePolicyFeature feature,
                            ParsedFeaturePolicy& policy) {
  RemoveFeatureIfPresent(feature, policy);
  AllowFeatureEverywhereIfNotPresent(feature, policy);
}

// This method defines the feature names which will be recognized by the parser
// for the Feature-Policy HTTP header and the <iframe> "allow" attribute, as
// well as the features which will be recognized by the document or iframe
// policy object.
//
// Features which are implemented behind a flag should generally also have the
// same flag controlling whether they are in this map. Note that features which
// are shipping as part of an origin trial should add their feature names to
// this map unconditionally, as the trial token could be added after the HTTP
// header needs to be parsed. This also means that top-level documents which
// simply want to embed another page which uses an origin trial feature, without
// using the feature themselves, can use feature policy to allow use of the
// feature in subframes. (The framed document will still require a valid origin
// trial token to use the feature in this scenario.)
const FeatureNameMap& GetDefaultFeatureNameMap() {
  DEFINE_STATIC_LOCAL(FeatureNameMap, default_feature_name_map, ());
  if (default_feature_name_map.IsEmpty()) {
    default_feature_name_map.Set("autoplay",
                                 mojom::FeaturePolicyFeature::kAutoplay);
    default_feature_name_map.Set("camera",
                                 mojom::FeaturePolicyFeature::kCamera);
    default_feature_name_map.Set("encrypted-media",
                                 mojom::FeaturePolicyFeature::kEncryptedMedia);
    default_feature_name_map.Set("fullscreen",
                                 mojom::FeaturePolicyFeature::kFullscreen);
    default_feature_name_map.Set("geolocation",
                                 mojom::FeaturePolicyFeature::kGeolocation);
    default_feature_name_map.Set("microphone",
                                 mojom::FeaturePolicyFeature::kMicrophone);
    default_feature_name_map.Set("midi",
                                 mojom::FeaturePolicyFeature::kMidiFeature);
    default_feature_name_map.Set("speaker",
                                 mojom::FeaturePolicyFeature::kSpeaker);
    default_feature_name_map.Set("sync-xhr",
                                 mojom::FeaturePolicyFeature::kSyncXHR);
    default_feature_name_map.Set("frobulate",
                                 mojom::FeaturePolicyFeature::kFrobulate);
    // Under origin trial: Should be made conditional on WebVR and WebXR
    // runtime flags once it is out of trial.
    ASSERT_ORIGIN_TRIAL(WebVR);
    ASSERT_ORIGIN_TRIAL(WebXR);
    default_feature_name_map.Set("vr", mojom::FeaturePolicyFeature::kWebVr);
    if (RuntimeEnabledFeatures::ExperimentalProductivityFeaturesEnabled()) {
      default_feature_name_map.Set(
          "layout-animations", mojom::FeaturePolicyFeature::kLayoutAnimations);
      default_feature_name_map.Set("document-write",
                                   mojom::FeaturePolicyFeature::kDocumentWrite);
      default_feature_name_map.Set(
          "document-domain", mojom::FeaturePolicyFeature::kDocumentDomain);
      default_feature_name_map.Set("font-display-late-swap",
                                   mojom::FeaturePolicyFeature::kFontDisplay);
      default_feature_name_map.Set(
          "unoptimized-images",
          mojom::FeaturePolicyFeature::kUnoptimizedImages);
      default_feature_name_map.Set("lazyload",
                                   mojom::FeaturePolicyFeature::kLazyLoad);
      default_feature_name_map.Set(
          "legacy-image-formats",
          mojom::FeaturePolicyFeature::kLegacyImageFormats);
      default_feature_name_map.Set(
          "oversized-images", mojom::FeaturePolicyFeature::kOversizedImages);
      default_feature_name_map.Set("unsized-media",
                                   mojom::FeaturePolicyFeature::kUnsizedMedia);
      default_feature_name_map.Set(
          "vertical-scroll", mojom::FeaturePolicyFeature::kVerticalScroll);
      default_feature_name_map.Set("sync-script",
                                   mojom::FeaturePolicyFeature::kSyncScript);
    }
    if (RuntimeEnabledFeatures::FeaturePolicyForSandboxEnabled()) {
      default_feature_name_map.Set(
          "forms", mojom::FeaturePolicyFeature::kFormSubmission);
      default_feature_name_map.Set("modals",
                                   mojom::FeaturePolicyFeature::kModals);
      default_feature_name_map.Set(
          "orientation-lock", mojom::FeaturePolicyFeature::kOrientationLock);
      default_feature_name_map.Set("pointer-lock",
                                   mojom::FeaturePolicyFeature::kPointerLock);
      default_feature_name_map.Set("popups",
                                   mojom::FeaturePolicyFeature::kPopups);
      default_feature_name_map.Set("presentation",
                                   mojom::FeaturePolicyFeature::kPresentation);
      default_feature_name_map.Set("scripts",
                                   mojom::FeaturePolicyFeature::kScript);
      default_feature_name_map.Set("top-navigation",
                                   mojom::FeaturePolicyFeature::kTopNavigation);
    }
    if (RuntimeEnabledFeatures::WebHIDEnabled()) {
      default_feature_name_map.Set("hid", mojom::FeaturePolicyFeature::kHid);
    }
    if (RuntimeEnabledFeatures::IdleDetectionEnabled()) {
      default_feature_name_map.Set("idle-detection",
                                   mojom::FeaturePolicyFeature::kIdleDetection);
    }
    if (RuntimeEnabledFeatures::PaymentRequestEnabled()) {
      default_feature_name_map.Set("payment",
                                   mojom::FeaturePolicyFeature::kPayment);
    }
    if (RuntimeEnabledFeatures::PictureInPictureAPIEnabled()) {
      default_feature_name_map.Set(
          "picture-in-picture", mojom::FeaturePolicyFeature::kPictureInPicture);
    }
    if (RuntimeEnabledFeatures::SensorEnabled()) {
      default_feature_name_map.Set("accelerometer",
                                   mojom::FeaturePolicyFeature::kAccelerometer);
      default_feature_name_map.Set(
          "ambient-light-sensor",
          mojom::FeaturePolicyFeature::kAmbientLightSensor);
      default_feature_name_map.Set("gyroscope",
                                   mojom::FeaturePolicyFeature::kGyroscope);
      default_feature_name_map.Set("magnetometer",
                                   mojom::FeaturePolicyFeature::kMagnetometer);
    }
    if (RuntimeEnabledFeatures::SerialEnabled()) {
      default_feature_name_map.Set("serial",
                                   mojom::FeaturePolicyFeature::kSerial);
    }
    if (RuntimeEnabledFeatures::WakeLockEnabled()) {
      default_feature_name_map.Set("wake-lock",
                                   mojom::FeaturePolicyFeature::kWakeLock);
    }
    if (RuntimeEnabledFeatures::WebUSBEnabled()) {
      default_feature_name_map.Set("usb", mojom::FeaturePolicyFeature::kUsb);
    }
  }
  return default_feature_name_map;
}

const Vector<String> GetAvailableFeatures(ExecutionContext* execution_context) {
  Vector<String> available_features;
  for (const auto& feature : GetDefaultFeatureNameMap()) {
    if (!IsOriginTrialDisabled(feature.key, execution_context))
      available_features.push_back(feature.key);
  }
  return available_features;
}

const String& GetNameForFeature(mojom::FeaturePolicyFeature feature) {
  for (const auto& entry : GetDefaultFeatureNameMap()) {
    if (entry.value == feature)
      return entry.key;
  }
  return g_empty_string;
}

}  // namespace blink
