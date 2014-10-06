/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef MixedContentChecker_h
#define MixedContentChecker_h

#include "platform/heap/Handle.h"
#include "platform/network/ResourceRequest.h"
#include "public/platform/WebURLRequest.h"
#include "wtf/text/WTFString.h"

namespace blink {

class FrameLoaderClient;
class LocalFrame;
class KURL;
class SecurityOrigin;

class MixedContentChecker FINAL {
    WTF_MAKE_NONCOPYABLE(MixedContentChecker);
    DISALLOW_ALLOCATION();
public:
    explicit MixedContentChecker(LocalFrame*);

    static bool shouldBlockFetch(LocalFrame* frame, const ResourceRequest& request, const KURL& url)
    {
        return shouldBlockFetch(frame, request.requestContext(), request.frameType(), url);
    }
    static bool shouldBlockFetch(LocalFrame*, WebURLRequest::RequestContext, WebURLRequest::FrameType, const KURL&);

    bool canDisplayInsecureContent(SecurityOrigin* securityOrigin, const KURL& url) const
    {
        return canDisplayInsecureContentInternal(securityOrigin, url, MixedContentChecker::Display);
    }

    bool canRunInsecureContent(SecurityOrigin* securityOrigin, const KURL& url) const
    {
        return canRunInsecureContentInternal(securityOrigin, url, MixedContentChecker::Execution);
    }

    bool canSubmitToInsecureForm(SecurityOrigin*, const KURL&) const;
    bool canConnectInsecureWebSocket(SecurityOrigin*, const KURL&) const;
    bool canFrameInsecureContent(SecurityOrigin*, const KURL&) const;
    static bool isMixedContent(SecurityOrigin*, const KURL&);

    static void checkMixedPrivatePublic(LocalFrame*, const AtomicString& resourceIPAddress);

    void trace(Visitor*);

private:
    enum MixedContentType {
        Display,
        Execution,
        WebSocket,
        Submission
    };

    enum ContextType {
        ContextTypeBlockable,
        ContextTypeOptionallyBlockable,
        ContextTypeShouldBeBlockable,
        ContextTypeBlockableUnlessLax
    };

    static LocalFrame* inWhichFrameIsThisContentMixed(LocalFrame*, WebURLRequest::RequestContext, WebURLRequest::FrameType, const KURL&);

    static ContextType contextTypeFromContext(WebURLRequest::RequestContext);
    static const char* typeNameFromContext(WebURLRequest::RequestContext);
    static void logToConsole(LocalFrame*, const KURL&, WebURLRequest::RequestContext, bool allowed);

    // FIXME: This should probably have a separate client from FrameLoader.
    FrameLoaderClient* client() const;

    bool canDisplayInsecureContentInternal(SecurityOrigin*, const KURL&, const MixedContentType) const;

    bool canRunInsecureContentInternal(SecurityOrigin*, const KURL&, const MixedContentType) const;

    void logWarning(bool allowed, const KURL& i, const MixedContentType) const;

    RawPtrWillBeMember<LocalFrame> m_frame;
};

} // namespace blink

#endif // MixedContentChecker_h
