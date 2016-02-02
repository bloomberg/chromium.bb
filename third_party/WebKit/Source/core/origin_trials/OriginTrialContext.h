// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OriginTrialContext_h
#define OriginTrialContext_h

#include "core/CoreExport.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExecutionContext.h"
#include "wtf/text/WTFString.h"

namespace blink {

class WebTrialTokenValidator;

// The Experimental Framework (EF) provides limited access to experimental
// features, on a per-origin basis (origin trials). This class provides the
// implementation to check if the experimental feature should be enabled for the
// current context.  This class is not for direct use by feature implementers.
// Instead, the OriginTrials generated class provides a static method for each
// feature to check if it is enabled. Experimental features must be defined in
// RuntimeEnabledFeatures.in, which is used to generate OriginTrials.h/cpp.
//
// Experimental features are defined by string names, provided by the
// implementers. The EF code does not maintain an enum or constant list for
// feature names. Instead, the EF validates the name provided by the feature
// implementation against any provided tokens.
//
// This class is not intended to be instantiated. Any required state is kept
// with a WebApiKeyValidator object held in the Platform object.
// The static methods in this class may be called either from the main thread
// or from a worker thread.
//
// TODO(chasej): Link to documentation, or provide more detail on keys, .etc
class CORE_EXPORT OriginTrialContext {
private:
    friend class OriginTrialContextTest;
    friend class OriginTrials;

    OriginTrialContext();

    // Returns true if the feature should be considered enabled for the current
    // execution context. This method usually makes use of the token validator
    // object in the platform, but this may be overridden if a custom validator
    // is required (for testing, for instance).
    static bool isFeatureEnabled(ExecutionContext*, const String& featureName, String* errorMessage, WebTrialTokenValidator* = nullptr);
};

} // namespace blink

#endif // OriginTrialContext_h
