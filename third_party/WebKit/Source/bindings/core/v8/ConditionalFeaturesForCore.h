// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ConditionalFeaturesForCore_h
#define ConditionalFeaturesForCore_h

#include "core/CoreExport.h"
#include "platform/feature_policy/FeaturePolicy.h"
#include "v8/include/v8.h"

namespace blink {

class LocalFrame;
class ScriptState;

// Installs all of the conditionally enabled V8 bindings on the Window object.
// This is called separately from other objects so that attributes and
// interfaces which need to be visible on the global object are installed even
// when the V8 context is reused (i.e., after navigation)
CORE_EXPORT void installConditionalFeaturesOnWindow(const ScriptState*);

CORE_EXPORT bool isFeatureEnabledInFrame(const FeaturePolicy::Feature&,
                                         const LocalFrame*);

CORE_EXPORT void registerInstallConditionalFeaturesForCore();

}  // namespace blink

#endif  // ConditionalFeaturesForCore_h
