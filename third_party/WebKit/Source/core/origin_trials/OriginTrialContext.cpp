// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/origin_trials/OriginTrialContext.h"

#include "core/dom/ExecutionContext.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/Platform.h"
#include "public/platform/WebSecurityOrigin.h"
#include "public/platform/WebTrialTokenValidator.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

namespace {

String getDisabledMessage(const String& featureName)
{
    return "The '" + featureName + "' feature is currently enabled in limited trials. Please see https://bit.ly/OriginTrials for information on enabling a trial for your site.";
}

String getInvalidTokenMessage(const String& featureName)
{
    return "The provided token(s) are not valid for the '" + featureName + "' feature.";
}

bool isWhitespace(UChar chr)
{
    return (chr == ' ') || (chr == '\t');
}

bool skipWhiteSpace(const String& str, unsigned& pos)
{
    unsigned len = str.length();
    while (pos < len && isWhitespace(str[pos]))
        ++pos;
    return pos < len;
}

// Extracts a quoted or unquoted token from an HTTP header. If the token was a
// quoted string, this also removes the quotes and unescapes any escaped
// characters. Also skips all whitespace before and after the token.
String extractTokenOrQuotedString(const String& headerValue, unsigned& pos)
{
    unsigned len = headerValue.length();
    String result;
    if (!skipWhiteSpace(headerValue, pos))
        return String();

    if (headerValue[pos] == '\'' || headerValue[pos] == '"') {
        StringBuilder out;
        // Quoted string, append characters until matching quote is found,
        // unescaping as we go.
        UChar quote = headerValue[pos++];
        while (pos < len && headerValue[pos] != quote) {
            if (headerValue[pos] == '\\')
                pos++;
            if (pos < len)
                out.append(headerValue[pos++]);
        }
        if (pos < len)
            pos++;
        result = out.toString();
    } else {
        // Unquoted token. Consume all characters until whitespace or comma.
        int startPos = pos;
        while (pos < len && !isWhitespace(headerValue[pos]) && headerValue[pos] != ',')
            pos++;
        result = headerValue.substring(startPos, pos - startPos);
    }
    skipWhiteSpace(headerValue, pos);
    return result;
}

} // namespace

OriginTrialContext::OriginTrialContext(ExecutionContext* host) : m_host(host)
{
}

// static
const char* OriginTrialContext::supplementName()
{
    return "OriginTrialContext";
}

// static
OriginTrialContext* OriginTrialContext::from(ExecutionContext* host)
{
    OriginTrialContext* originTrials = static_cast<OriginTrialContext*>(Supplement<ExecutionContext>::from(host, supplementName()));
    if (!originTrials) {
        originTrials = new OriginTrialContext(host);
        Supplement<ExecutionContext>::provideTo(*host, supplementName(), originTrials);
    }
    return originTrials;
}

// static
std::unique_ptr<Vector<String>> OriginTrialContext::parseHeaderValue(const String& headerValue)
{
    std::unique_ptr<Vector<String>> tokens(new Vector<String>);
    unsigned pos = 0;
    unsigned len = headerValue.length();
    while (pos < len) {
        String token = extractTokenOrQuotedString(headerValue, pos);
        if (!token.isEmpty())
            tokens->append(token);
        // Make sure tokens are comma-separated.
        if (pos < len && headerValue[pos++] != ',')
            return nullptr;
    }
    return tokens;
}

// static
void OriginTrialContext::addTokensFromHeader(ExecutionContext* host, const String& headerValue)
{
    if (headerValue.isEmpty())
        return;
    std::unique_ptr<Vector<String>> tokens(parseHeaderValue(headerValue));
    if (!tokens)
        return;
    OriginTrialContext* context = from(host);
    for (const String& token : *tokens) {
        context->addToken(token);
    }
}

void OriginTrialContext::addToken(const String& token)
{
    if (!token.isEmpty())
        m_tokens.append(token);
}

bool OriginTrialContext::isFeatureEnabled(const String& featureName, String* errorMessage, WebTrialTokenValidator* validator)
{
    if (!RuntimeEnabledFeatures::experimentalFrameworkEnabled()) {
        // TODO(iclelland): Set an error message here, the first time the
        // context is accessed in this renderer.
        return false;
    }

    // Feature trials are only enabled for secure origins
    bool isSecure = errorMessage
        ? m_host->isSecureContext(*errorMessage)
        : m_host->isSecureContext();
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


    WebSecurityOrigin origin(m_host->getSecurityOrigin());
    for (const String& token : m_tokens) {
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

    if (m_tokens.size()) {
        *errorMessage = getInvalidTokenMessage(featureName);
    } else {
        *errorMessage = getDisabledMessage(featureName);
    }
    m_errorMessageGeneratedForFeature.add(featureName);
    return false;
}

DEFINE_TRACE(OriginTrialContext)
{
    visitor->trace(m_host);
}

} // namespace blink
