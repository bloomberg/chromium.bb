// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/experiments/Experiments.h"

#include "core/dom/ElementTraversal.h"
#include "core/dom/ExceptionCode.h"
#include "core/html/HTMLHeadElement.h"
#include "core/html/HTMLMetaElement.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/weborigin/SecurityOrigin.h"

namespace blink {

namespace {

const char* kExperimentsMetaName = "api-experiments";

String getCurrentOrigin(ExecutionContext* executionContext)
{
    return executionContext->securityOrigin()->toString();
}

String getDisabledMessage(const String& apiName)
{
    // TODO(chasej): Update message with URL to experiments site, when live
    return "The '" + apiName + "' API is currently enabled in limited experiments. Please see [Chrome experiments website URL] for information on enabling this experiment on your site.";
}

bool hasValidAPIKey(ExecutionContext* executionContext, const String& apiName, String* errorMessage)
{
    bool foundAnyKey = false;
    String origin = getCurrentOrigin(executionContext);

    // When in a document, the API key is provided in a meta tag
    if (executionContext->isDocument()) {
        HTMLHeadElement* head = toDocument(executionContext)->head();
        for (HTMLMetaElement* metaElement = head ? Traversal<HTMLMetaElement>::firstChild(*head) : 0; metaElement; metaElement = Traversal<HTMLMetaElement>::nextSibling(*metaElement)) {
            if (equalIgnoringCase(metaElement->name(), kExperimentsMetaName)) {
                foundAnyKey = true;
                if (equalIgnoringCase(metaElement->content(), apiName))
                    return true;
            }
        }
    }

    if (errorMessage) {
        if (foundAnyKey) {
            *errorMessage = "The provided key(s) are not valid for the '" + apiName + "' API.";
        } else {
            *errorMessage = getDisabledMessage(apiName);
        }
    }
    return false;
}

} // namespace

// static
bool Experiments::isApiEnabled(ExecutionContext* executionContext, const String& apiName, String* errorMessage)
{
    if (!RuntimeEnabledFeatures::experimentalFrameworkEnabled()) {
        if (errorMessage) {
            *errorMessage = "Experimental Framework is not enabled.";
        }
        return false;
    }

    if (!executionContext) {
        ASSERT_NOT_REACHED();
        return false;
    }

    // Experiments are only enabled for secure origins
    bool isSecure = errorMessage
        ? executionContext->isSecureContext(*errorMessage)
        : executionContext->isSecureContext();
    if (!isSecure) {
        return false;
    }

    return hasValidAPIKey(executionContext, apiName, errorMessage);
}

// static
DOMException* Experiments::createApiDisabledException(const String& apiName)
{
    return DOMException::create(NotSupportedError, getDisabledMessage(apiName));
}

} // namespace blink
