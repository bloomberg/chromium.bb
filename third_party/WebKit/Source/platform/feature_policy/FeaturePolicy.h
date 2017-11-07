// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FeaturePolicy_h
#define FeaturePolicy_h

#include "platform/PlatformExport.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/StringHash.h"
#include "platform/wtf/text/WTFString.h"
#include "third_party/WebKit/common/feature_policy/feature_policy.h"
#include "url/origin.h"

#include <memory>

namespace blink {

// Returns a map between feature name (string) and WebFeaturePolicyFeature
// (enum).
typedef HashMap<String, WebFeaturePolicyFeature> FeatureNameMap;
PLATFORM_EXPORT const FeatureNameMap& GetDefaultFeatureNameMap();

// Converts a header policy string into a vector of whitelists, one for each
// feature specified. Unrecognized features are filtered out. If |messages|
// is not null, then any message in the input will cause a warning message to be
// appended to it.
// Example of a feature policy string:
//     "vibrate a.com b.com; fullscreen 'none'; payment 'self', payment *".
PLATFORM_EXPORT ParsedFeaturePolicy
ParseFeaturePolicyHeader(const String& policy,
                         scoped_refptr<SecurityOrigin>,
                         Vector<String>* messages);

// Converts a container policy string into a vector of whitelists, given self
// and src origins provided, one for each feature specified. Unrecognized
// features are filtered out. If |messages| is not null, then any message in the
// input will cause as warning message to be appended to it.
// Example of a feature policy string:
//     "vibrate a.com 'src'; fullscreen 'none'; payment 'self', payment *".
// If |old_syntax| is not null, it will be set true if the deprecated
// space-deparated feature list syntax is detected.
// TODO(loonybear): remove the boolean once the space separated feature list
// syntax is deprecated.
// https://crbug.com/761009.
PLATFORM_EXPORT ParsedFeaturePolicy
ParseFeaturePolicyAttribute(const String& policy,
                            scoped_refptr<SecurityOrigin> self_origin,
                            scoped_refptr<SecurityOrigin> src_origin,
                            Vector<String>* messages,
                            bool* old_syntax);

// Converts a feature policy string into a vector of whitelists (see comments
// above), with an explicit FeatureNameMap. This algorithm is called by both
// header policy parsing and container policy parsing. |self_origin| and
// |src_origin| are both nullable.
// If |old_syntax| is not null, it will be set true if the deprecated
// space-deparated feature list syntax is detected.
// TODO(loonybear): remove the boolean once the space separated feature list
// syntax is deprecated.
// https://crbug.com/761009.
PLATFORM_EXPORT ParsedFeaturePolicy
ParseFeaturePolicy(const String& policy,
                   scoped_refptr<SecurityOrigin> self_origin,
                   scoped_refptr<SecurityOrigin> src_origin,
                   Vector<String>* messages,
                   const FeatureNameMap& feature_names,
                   bool* old_syntax = nullptr);

// Verifies whether feature policy is enabled and |feature| is supported in
// feature policy.
PLATFORM_EXPORT bool IsSupportedInFeaturePolicy(WebFeaturePolicyFeature);

}  // namespace blink

#endif  // FeaturePolicy_h
