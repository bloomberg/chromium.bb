// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/origin_trials/OriginTrialContext.h"

#include "core/dom/ElementTraversal.h"
#include "core/dom/ExceptionCode.h"
#include "core/html/HTMLHeadElement.h"
#include "core/html/HTMLMetaElement.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/Platform.h"
#include "public/platform/WebTrialTokenValidator.h"

namespace blink {

namespace {

String getCurrentOrigin(ExecutionContext* executionContext)
{
    return executionContext->securityOrigin()->toString();
}

String getDisabledMessage(const String& featureName)
{
    // TODO(chasej): Update message with URL to experiments site, when live
    return "The '" + featureName + "' feature is currently enabled in limited trials. Please see [Phosphor console URL] for information on enabling a trial for your site.";
}

String getInvalidTokenMessage(const String& featureName)
{
    return "The provided token(s) are not valid for the '" + featureName + "' feature.";
}

} // namespace

const char OriginTrialContext::kTrialHeaderName[] = "origin-trial";

OriginTrialContext::OriginTrialContext() {}

bool OriginTrialContext::isFeatureEnabled(const String& featureName, String* errorMessage, WebTrialTokenValidator* validator)
{
    if (!RuntimeEnabledFeatures::experimentalFrameworkEnabled()) {
        // TODO(iclelland): Set an error message here, the first time the
        // context is accessed in this renderer.
        return false;
    }

    // Feature trials are only enabled for secure origins
    bool isSecure = errorMessage
        ? executionContext()->isSecureContext(*errorMessage)
        : executionContext()->isSecureContext();
    if (!isSecure) {
        // The execution context should always set a message here, if a valid
        // pointer was passed in. If it does not, then we should find out why
        // not, and decide whether the OriginTrialContext should be using its
        // own error messages for this case.
        DCHECK(!errorMessage || !errorMessage->isEmpty());
        return false;
    }

    if (!validator) {
        validator = Platform::current()->trialTokenValidator();
        if (!validator) {
            // TODO(iclelland): Set an error message here, the first time the
            // context is accessed in this renderer.
            return false;
        }
    }

    return hasValidToken(getTokens(), featureName, errorMessage, validator);
}

bool OriginTrialContext::hasValidToken(Vector<String> tokens, const String& featureName, String* errorMessage, WebTrialTokenValidator* validator)
{
    String origin = getCurrentOrigin(executionContext());

    for (const String& token : tokens) {
        // Check with the validator service to verify the signature.
        if (validator->validateToken(token, origin, featureName)) {
            return true;
        }
    }

    if (!errorMessage)
        return false;

    // If an error message has already been generated in this context, for this
    // feature, do not generate another one. (This avoids cluttering the console
    // with error messages on every attempt to access the feature.)
    if (m_errorMessageGeneratedForFeature.contains(featureName)) {
        *errorMessage = "";
        return false;
    }

    if (tokens.size()) {
        *errorMessage = getInvalidTokenMessage(featureName);
    } else {
        *errorMessage = getDisabledMessage(featureName);
    }
    m_errorMessageGeneratedForFeature.add(featureName);
    return false;
}

DEFINE_TRACE(OriginTrialContext)
{
}

} // namespace blink
