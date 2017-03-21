// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FeaturePolicy_h
#define FeaturePolicy_h

#include "platform/PlatformExport.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/WebFeaturePolicy.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"

#include <memory>

namespace blink {

// Returns the corresponding WebFeaturePolicyFeature enum given a feature
// string.
PLATFORM_EXPORT WebFeaturePolicyFeature
getWebFeaturePolicyFeature(const String& feature);

// Converts a JSON feature policy string into a vector of whitelists, one for
// each feature specified. Unrecognized features are filtered out. If |messages|
// is not null, then any errors in the input will cause an error message to be
// appended to it.
PLATFORM_EXPORT WebParsedFeaturePolicy
parseFeaturePolicy(const String& policy,
                   RefPtr<SecurityOrigin>,
                   Vector<String>* messages);

// Given a vector of WebFeaturePolicyFeatures and an origin, creates a vector of
// whitelists, one for each feature specified.
PLATFORM_EXPORT WebParsedFeaturePolicy getContainerPolicyFromAllowedFeatures(
    const WebVector<WebFeaturePolicyFeature>& features,
    RefPtr<SecurityOrigin>);

}  // namespace blink

#endif  // FeaturePolicy_h
