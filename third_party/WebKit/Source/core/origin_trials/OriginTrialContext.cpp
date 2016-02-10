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

const char kTrialMetaTagName[] = "origin-trials";

String getCurrentOrigin(ExecutionContext* executionContext)
{
    return executionContext->securityOrigin()->toString();
}

String getDisabledMessage(const String& featureName)
{
    // TODO(chasej): Update message with URL to experiments site, when live
    return "The '" + featureName + "' feature is currently enabled in limited trials. Please see [Phosphor console URL] for information on enabling a trial for your site.";
}

bool hasValidToken(ExecutionContext* executionContext, const String& featureName, String* errorMessage, WebTrialTokenValidator* validator)
{
    bool foundAnyToken = false;
    String origin = getCurrentOrigin(executionContext);

    // When in a document, the token is provided in a meta tag
    if (executionContext->isDocument()) {
        HTMLHeadElement* head = toDocument(executionContext)->head();
        for (HTMLMetaElement* metaElement = head ? Traversal<HTMLMetaElement>::firstChild(*head) : 0; metaElement; metaElement = Traversal<HTMLMetaElement>::nextSibling(*metaElement)) {
            if (equalIgnoringCase(metaElement->name(), kTrialMetaTagName)) {
                foundAnyToken = true;
                String tokenString = metaElement->content();
                // Check with the validator service to verify the signature.
                if (validator->validateToken(tokenString, origin, featureName)) {
                    return true;
                }
            }
        }
    }

    if (errorMessage) {
        if (foundAnyToken) {
            *errorMessage = "The provided token(s) are not valid for the '" + featureName + "' feature.";
        } else {
            *errorMessage = getDisabledMessage(featureName);
        }
    }
    return false;
}

} // namespace

// static
bool OriginTrialContext::isFeatureEnabled(ExecutionContext* executionContext, const String& featureName, String* errorMessage, WebTrialTokenValidator* validator)
{
    if (!RuntimeEnabledFeatures::experimentalFrameworkEnabled()) {
        // TODO(iclelland): Set an error message here, the first time the
        // context is accessed in this renderer.
        return false;
    }

    if (!executionContext) {
        ASSERT_NOT_REACHED();
        return false;
    }

    // Feature trials are only enabled for secure origins
    bool isSecure = errorMessage
        ? executionContext->isSecureContext(*errorMessage)
        : executionContext->isSecureContext();
    if (!isSecure) {
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

    return hasValidToken(executionContext, featureName, errorMessage, validator);
}

} // namespace blink
