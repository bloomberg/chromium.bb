/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ResourceLoaderOptions_h
#define ResourceLoaderOptions_h

#include "core/fetch/FetchInitiatorInfo.h"

namespace WebCore {

enum SendCallbackPolicy {
    SendCallbacks,
    DoNotSendCallbacks
};

enum ContentSniffingPolicy {
    SniffContent,
    DoNotSniffContent
};

enum DataBufferingPolicy {
    BufferData,
    DoNotBufferData
};

enum ClientCrossOriginCredentialPolicy {
    AskClientForCrossOriginCredentials,
    DoNotAskClientForCrossOriginCredentials
};

enum SecurityCheckPolicy {
    SkipSecurityCheck,
    DoSecurityCheck
};

enum ContentSecurityPolicyCheck {
    CheckContentSecurityPolicy,
    DoNotCheckContentSecurityPolicy
};

enum RequestOriginPolicy {
    UseDefaultOriginRestrictionsForType,
    RestrictToSameOrigin,
    PotentiallyCrossOriginEnabled // Indicates "potentially CORS-enabled fetch" in HTML standard.
};

enum RequestInitiatorContext {
    DocumentContext,
    WorkerContext,
};

enum StoredCredentials {
    AllowStoredCredentials,
    DoNotAllowStoredCredentials
};

// APIs like XMLHttpRequest and EventSource let the user decide
// whether to send credentials, but they're always sent for
// same-origin requests. Additional information is needed to handle
// cross-origin redirects correctly.
enum CredentialRequest {
    ClientRequestedCredentials,
    ClientDidNotRequestCredentials
};

enum MixedContentBlockingTreatment {
    TreatAsDefaultForType,
    TreatAsPassiveContent,
    TreatAsActiveContent,
    TreatAsAlwaysAllowedContent
};

enum SynchronousPolicy {
    RequestSynchronously,
    RequestAsynchronously
};

struct ResourceLoaderOptions {
    ResourceLoaderOptions()
        : sendLoadCallbacks(DoNotSendCallbacks)
        , sniffContent(DoNotSniffContent)
        , dataBufferingPolicy(BufferData)
        , allowCredentials(DoNotAllowStoredCredentials)
        , credentialsRequested(ClientDidNotRequestCredentials)
        , crossOriginCredentialPolicy(DoNotAskClientForCrossOriginCredentials)
        , securityCheck(DoSecurityCheck)
        , contentSecurityPolicyOption(CheckContentSecurityPolicy)
        , requestOriginPolicy(UseDefaultOriginRestrictionsForType)
        , requestInitiatorContext(DocumentContext)
        , mixedContentBlockingTreatment(TreatAsDefaultForType)
        , synchronousPolicy(RequestAsynchronously)
    {
    }

    ResourceLoaderOptions(
        SendCallbackPolicy sendLoadCallbacks,
        ContentSniffingPolicy sniffContent,
        DataBufferingPolicy dataBufferingPolicy,
        StoredCredentials allowCredentials,
        CredentialRequest credentialsRequested,
        ClientCrossOriginCredentialPolicy crossOriginCredentialPolicy,
        SecurityCheckPolicy securityCheck,
        ContentSecurityPolicyCheck contentSecurityPolicyOption,
        RequestOriginPolicy requestOriginPolicy,
        RequestInitiatorContext requestInitiatorContext)
        : sendLoadCallbacks(sendLoadCallbacks)
        , sniffContent(sniffContent)
        , dataBufferingPolicy(dataBufferingPolicy)
        , allowCredentials(allowCredentials)
        , credentialsRequested(credentialsRequested)
        , crossOriginCredentialPolicy(crossOriginCredentialPolicy)
        , securityCheck(securityCheck)
        , contentSecurityPolicyOption(contentSecurityPolicyOption)
        , requestOriginPolicy(requestOriginPolicy)
        , requestInitiatorContext(requestInitiatorContext)
        , mixedContentBlockingTreatment(TreatAsDefaultForType)
        , synchronousPolicy(RequestAsynchronously)
    {
    }

    SendCallbackPolicy sendLoadCallbacks;
    ContentSniffingPolicy sniffContent;
    DataBufferingPolicy dataBufferingPolicy;
    StoredCredentials allowCredentials; // Whether HTTP credentials and cookies are sent with the request.
    CredentialRequest credentialsRequested; // Whether the client (e.g. XHR) wanted credentials in the first place.
    ClientCrossOriginCredentialPolicy crossOriginCredentialPolicy; // Whether we will ask the client for credentials (if we allow credentials at all).
    SecurityCheckPolicy securityCheck;
    ContentSecurityPolicyCheck contentSecurityPolicyOption;
    FetchInitiatorInfo initiatorInfo;
    RequestOriginPolicy requestOriginPolicy;
    RequestInitiatorContext requestInitiatorContext;
    MixedContentBlockingTreatment mixedContentBlockingTreatment;
    SynchronousPolicy synchronousPolicy;
};

} // namespace WebCore

#endif // ResourceLoaderOptions_h
