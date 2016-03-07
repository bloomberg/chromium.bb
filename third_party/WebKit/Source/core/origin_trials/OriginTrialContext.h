// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OriginTrialContext_h
#define OriginTrialContext_h

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "wtf/Forward.h"
#include "wtf/HashSet.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"

namespace blink {

class ExecutionContext;
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
// This class is abstract, and should be subclassed for each execution context
// which supports origin trials.
//
// TODO(chasej): Link to documentation, or provide more detail on keys, .etc
class CORE_EXPORT OriginTrialContext : public NoBaseWillBeGarbageCollectedFinalized<OriginTrialContext> {
public:
    static const char kTrialHeaderName[];
    virtual ~OriginTrialContext() = default;

    virtual ExecutionContext* executionContext() = 0;
    virtual Vector<String> getTokens() = 0;

    DECLARE_VIRTUAL_TRACE();

protected:
    OriginTrialContext();

private:
    friend class OriginTrialContextTest;
    friend class OriginTrials;

    // Returns true if the feature should be considered enabled for the current
    // execution context. This method usually makes use of the token validator
    // object in the platform, but this may be overridden if a custom validator
    // is required (for testing, for instance).
    bool isFeatureEnabled(const String& featureName, String* errorMessage, WebTrialTokenValidator* = nullptr);

    bool hasValidToken(Vector<String> tokens, const String& featureName, String* errorMessage, WebTrialTokenValidator*);

    // Records whether an error message has been generated, for each feature
    // name. Since these messages are generally written to the console, this is
    // used to avoid cluttering the console with messages on every access.
    HashSet<String> m_errorMessageGeneratedForFeature;
};

} // namespace blink

#endif // OriginTrialContext_h
