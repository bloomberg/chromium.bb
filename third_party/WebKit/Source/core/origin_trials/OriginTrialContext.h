// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OriginTrialContext_h
#define OriginTrialContext_h

#include "core/CoreExport.h"
#include "platform/Supplementable.h"
#include "wtf/HashSet.h"
#include "wtf/Vector.h"
#include "wtf/text/StringHash.h"
#include "wtf/text/WTFString.h"

namespace blink {

class ExecutionContext;
enum class WebOriginTrialTokenStatus;
class WebTrialTokenValidator;

// The Origin Trials Framework provides limited access to experimental features,
// on a per-origin basis (origin trials). This class provides the implementation
// to check if the experimental feature should be enabled for the current
// context.  This class is not for direct use by feature implementers.
// Instead, the OriginTrials generated namespace provides a method for each
// feature to check if it is enabled. Experimental features must be defined in
// RuntimeEnabledFeatures.in, which is used to generate OriginTrials.h/cpp.
//
// Experimental features are defined by string names, provided by the
// implementers. The framework does not maintain an enum or constant list for
// feature names. Instead, the name provided by the feature implementation
// is validated against any provided tokens.
//
// For more information, see https://github.com/jpchase/OriginTrials.
class CORE_EXPORT OriginTrialContext final : public GarbageCollectedFinalized<OriginTrialContext>, public Supplement<ExecutionContext> {
USING_GARBAGE_COLLECTED_MIXIN(OriginTrialContext)
public:
    enum CreateMode {
        CreateIfNotExists,
        DontCreateIfNotExists
    };

    OriginTrialContext(ExecutionContext*, WebTrialTokenValidator*);

    static const char* supplementName();

    // Returns the OriginTrialContext for a specific ExecutionContext. If
    // |create| is false, this returns null if no OriginTrialContext exists
    // yet for the ExecutionContext.
    static OriginTrialContext* from(ExecutionContext*, CreateMode = CreateIfNotExists);

    // Parses an Origin-Trial header as specified in
    // https://jpchase.github.io/OriginTrials/#header into individual tokens.
    // Returns null if the header value was malformed and could not be parsed.
    // If the header does not contain any tokens, this returns an empty vector.
    static std::unique_ptr<Vector<String>> parseHeaderValue(const String& headerValue);

    static void addTokensFromHeader(ExecutionContext*, const String& headerValue);
    static void addTokens(ExecutionContext*, const Vector<String>* tokens);

    // Returns the trial tokens that are active in a specific ExecutionContext.
    // Returns null if no tokens were added to the ExecutionContext.
    static std::unique_ptr<Vector<String>> getTokens(ExecutionContext*);

    void addToken(const String& token);
    void addTokens(const Vector<String>& tokens);

    // Returns true if the feature should be considered enabled for the current
    // execution context.
    bool isFeatureEnabled(const String& featureName);

    // Installs JavaScript bindings for any features which should be enabled by
    // the current set of trial tokens. This method is idempotent; only features
    // which have been enabled since the last time it was run will be installed.
    // If the V8 context for the host execution context has not been
    // initialized, then this method will return without doing anything.
    void initializePendingFeatures();

    void setFeatureBindingsInstalled(const String& featureName);
    bool featureBindingsInstalled(const String& featureName);

    DECLARE_VIRTUAL_TRACE();

private:
    void validateToken(const String& token);

    Member<ExecutionContext> m_host;
    Vector<String> m_tokens;
    HashSet<String> m_enabledFeatures;
    WebTrialTokenValidator* m_trialTokenValidator;

    // Records whether a feature has been installed into the host's V8 context,
    // for each feature name.
    HashSet<String> m_bindingsInstalled;
};

} // namespace blink

#endif // OriginTrialContext_h
