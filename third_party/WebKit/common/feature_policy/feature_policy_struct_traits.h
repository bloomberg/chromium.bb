// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_COMMON_FEATURE_POLICY_FEATURE_POLICY_STRUCT_TRAITS_H_
#define THIRD_PARTY_WEBKIT_COMMON_FEATURE_POLICY_FEATURE_POLICY_STRUCT_TRAITS_H_

#include <vector>

#include "mojo/public/cpp/bindings/enum_traits.h"
#include "third_party/WebKit/common/common_export.h"
#include "third_party/WebKit/common/feature_policy/feature_policy.h"
#include "third_party/WebKit/common/feature_policy/feature_policy.mojom-shared.h"
#include "third_party/WebKit/common/feature_policy/feature_policy_feature.h"
#include "third_party/WebKit/common/sandbox_flags.h"

namespace mojo {

#define STATIC_ASSERT_ENUM(a, b)                            \
  static_assert(static_cast<int>(a) == static_cast<int>(b), \
                "mismatching enum : " #a)

// TODO(crbug.com/789818) - Merge these 2 FeaturePolicyFeature enums.
STATIC_ASSERT_ENUM(::blink::FeaturePolicyFeature::kNotFound,
                   ::blink::mojom::FeaturePolicyFeature::kNotFound);
STATIC_ASSERT_ENUM(::blink::FeaturePolicyFeature::kAutoplay,
                   ::blink::mojom::FeaturePolicyFeature::kAutoplay);
STATIC_ASSERT_ENUM(::blink::FeaturePolicyFeature::kCamera,
                   ::blink::mojom::FeaturePolicyFeature::kCamera);
STATIC_ASSERT_ENUM(::blink::FeaturePolicyFeature::kEncryptedMedia,
                   ::blink::mojom::FeaturePolicyFeature::kEncryptedMedia);
STATIC_ASSERT_ENUM(::blink::FeaturePolicyFeature::kFullscreen,
                   ::blink::mojom::FeaturePolicyFeature::kFullscreen);
STATIC_ASSERT_ENUM(::blink::FeaturePolicyFeature::kGeolocation,
                   ::blink::mojom::FeaturePolicyFeature::kGeolocation);
STATIC_ASSERT_ENUM(::blink::FeaturePolicyFeature::kMicrophone,
                   ::blink::mojom::FeaturePolicyFeature::kMicrophone);
STATIC_ASSERT_ENUM(::blink::FeaturePolicyFeature::kMidiFeature,
                   ::blink::mojom::FeaturePolicyFeature::kMidiFeature);
STATIC_ASSERT_ENUM(::blink::FeaturePolicyFeature::kPayment,
                   ::blink::mojom::FeaturePolicyFeature::kPayment);
STATIC_ASSERT_ENUM(::blink::FeaturePolicyFeature::kSpeaker,
                   ::blink::mojom::FeaturePolicyFeature::kSpeaker);
STATIC_ASSERT_ENUM(::blink::FeaturePolicyFeature::kVibrate,
                   ::blink::mojom::FeaturePolicyFeature::kVibrate);
STATIC_ASSERT_ENUM(::blink::FeaturePolicyFeature::kDocumentCookie,
                   ::blink::mojom::FeaturePolicyFeature::kDocumentCookie);
STATIC_ASSERT_ENUM(::blink::FeaturePolicyFeature::kDocumentDomain,
                   ::blink::mojom::FeaturePolicyFeature::kDocumentDomain);
STATIC_ASSERT_ENUM(::blink::FeaturePolicyFeature::kDocumentWrite,
                   ::blink::mojom::FeaturePolicyFeature::kDocumentWrite);
STATIC_ASSERT_ENUM(::blink::FeaturePolicyFeature::kSyncScript,
                   ::blink::mojom::FeaturePolicyFeature::kSyncScript);
STATIC_ASSERT_ENUM(::blink::FeaturePolicyFeature::kSyncXHR,
                   ::blink::mojom::FeaturePolicyFeature::kSyncXHR);
STATIC_ASSERT_ENUM(::blink::FeaturePolicyFeature::kUsb,
                   ::blink::mojom::FeaturePolicyFeature::kUsb);
STATIC_ASSERT_ENUM(::blink::FeaturePolicyFeature::kWebVr,
                   ::blink::mojom::FeaturePolicyFeature::kWebVr);
STATIC_ASSERT_ENUM(::blink::FeaturePolicyFeature::kAccelerometer,
                   ::blink::mojom::FeaturePolicyFeature::kAccelerometer);
STATIC_ASSERT_ENUM(::blink::FeaturePolicyFeature::kAmbientLightSensor,
                   ::blink::mojom::FeaturePolicyFeature::kAmbientLightSensor);
STATIC_ASSERT_ENUM(::blink::FeaturePolicyFeature::kGyroscope,
                   ::blink::mojom::FeaturePolicyFeature::kGyroscope);
STATIC_ASSERT_ENUM(::blink::FeaturePolicyFeature::kMagnetometer,
                   ::blink::mojom::FeaturePolicyFeature::kMagnetometer);

// TODO(crbug.com/789818) - Merge these 2 WebSandboxFlags enums.
STATIC_ASSERT_ENUM(::blink::WebSandboxFlags::kNone,
                   ::blink::mojom::WebSandboxFlags::kNone);
STATIC_ASSERT_ENUM(::blink::WebSandboxFlags::kNavigation,
                   ::blink::mojom::WebSandboxFlags::kNavigation);
STATIC_ASSERT_ENUM(::blink::WebSandboxFlags::kPlugins,
                   ::blink::mojom::WebSandboxFlags::kPlugins);
STATIC_ASSERT_ENUM(::blink::WebSandboxFlags::kOrigin,
                   ::blink::mojom::WebSandboxFlags::kOrigin);
STATIC_ASSERT_ENUM(::blink::WebSandboxFlags::kForms,
                   ::blink::mojom::WebSandboxFlags::kForms);
STATIC_ASSERT_ENUM(::blink::WebSandboxFlags::kScripts,
                   ::blink::mojom::WebSandboxFlags::kScripts);
STATIC_ASSERT_ENUM(::blink::WebSandboxFlags::kTopNavigation,
                   ::blink::mojom::WebSandboxFlags::kTopNavigation);
STATIC_ASSERT_ENUM(::blink::WebSandboxFlags::kPopups,
                   ::blink::mojom::WebSandboxFlags::kPopups);
STATIC_ASSERT_ENUM(::blink::WebSandboxFlags::kAutomaticFeatures,
                   ::blink::mojom::WebSandboxFlags::kAutomaticFeatures);
STATIC_ASSERT_ENUM(::blink::WebSandboxFlags::kPointerLock,
                   ::blink::mojom::WebSandboxFlags::kPointerLock);
STATIC_ASSERT_ENUM(::blink::WebSandboxFlags::kDocumentDomain,
                   ::blink::mojom::WebSandboxFlags::kDocumentDomain);
STATIC_ASSERT_ENUM(::blink::WebSandboxFlags::kOrientationLock,
                   ::blink::mojom::WebSandboxFlags::kOrientationLock);
STATIC_ASSERT_ENUM(
    ::blink::WebSandboxFlags::kPropagatesToAuxiliaryBrowsingContexts,
    ::blink::mojom::WebSandboxFlags::kPropagatesToAuxiliaryBrowsingContexts);
STATIC_ASSERT_ENUM(::blink::WebSandboxFlags::kModals,
                   ::blink::mojom::WebSandboxFlags::kModals);
STATIC_ASSERT_ENUM(::blink::WebSandboxFlags::kPresentationController,
                   ::blink::mojom::WebSandboxFlags::kPresentationController);
STATIC_ASSERT_ENUM(
    ::blink::WebSandboxFlags::kTopNavigationByUserActivation,
    ::blink::mojom::WebSandboxFlags::kTopNavigationByUserActivation);
STATIC_ASSERT_ENUM(::blink::WebSandboxFlags::kDownloads,
                   ::blink::mojom::WebSandboxFlags::kDownloads);

template <>
struct BLINK_COMMON_EXPORT
    EnumTraits<blink::mojom::WebSandboxFlags, blink::WebSandboxFlags> {
  static blink::mojom::WebSandboxFlags ToMojom(blink::WebSandboxFlags flags) {
    return static_cast<blink::mojom::WebSandboxFlags>(flags);
  }
  static bool FromMojom(blink::mojom::WebSandboxFlags in,
                        blink::WebSandboxFlags* out) {
    *out = static_cast<blink::WebSandboxFlags>(in);
    return true;
  }
};

template <>
struct BLINK_COMMON_EXPORT EnumTraits<blink::mojom::FeaturePolicyFeature,
                                      blink::FeaturePolicyFeature> {
  static blink::mojom::FeaturePolicyFeature ToMojom(
      blink::FeaturePolicyFeature feature) {
    return static_cast<blink::mojom::FeaturePolicyFeature>(feature);
  }
  static bool FromMojom(blink::mojom::FeaturePolicyFeature in,
                        blink::FeaturePolicyFeature* out) {
    *out = static_cast<blink::FeaturePolicyFeature>(in);
    return true;
  }
};

template <>
class BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::ParsedFeaturePolicyDeclarationDataView,
                 blink::ParsedFeaturePolicyDeclaration> {
 public:
  static blink::FeaturePolicyFeature feature(
      const blink::ParsedFeaturePolicyDeclaration& policy) {
    return policy.feature;
  }
  static bool matches_all_origins(
      const blink::ParsedFeaturePolicyDeclaration& policy) {
    return policy.matches_all_origins;
  }
  static const std::vector<url::Origin>& origins(
      const blink::ParsedFeaturePolicyDeclaration& policy) {
    return policy.origins;
  }

  static bool Read(blink::mojom::ParsedFeaturePolicyDeclarationDataView in,
                   blink::ParsedFeaturePolicyDeclaration* out);
};

}  // namespace mojo

#endif  // THIRD_PARTY_WEBKIT_COMMON_FEATURE_POLICY_FEATURE_POLICY_STRUCT_TRAITS_H_
