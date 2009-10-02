/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#include "config.h"
#include "SocketStreamHandle.h"

#if ENABLE(WEB_SOCKETS)

#include "Logging.h"
#include "NotImplemented.h"
#include "SocketStreamHandleClient.h"
#include "WebData.h"
#include "WebKit.h"
#include <wtf/PassOwnPtr.h>

using namespace WebKit;

namespace WebCore {

class SocketStreamHandleInternal {
public:
    static PassOwnPtr<SocketStreamHandleInternal> create(SocketStreamHandle* handle)
    {
        return new SocketStreamHandleInternal(handle);
    }
    virtual ~SocketStreamHandleInternal();

    // TODO(ukai): implement this.

private:
    explicit SocketStreamHandleInternal(SocketStreamHandle*);

    SocketStreamHandle* m_handle;
};

SocketStreamHandleInternal::SocketStreamHandleInternal(SocketStreamHandle* handle)
    : m_handle(handle)
{
}

SocketStreamHandleInternal::~SocketStreamHandleInternal()
{
    m_handle = 0;
}

// SocketStreamHandle ----------------------------------------------------------

SocketStreamHandle::SocketStreamHandle(const KURL& url, SocketStreamHandleClient* client)
        : SocketStreamHandleBase(url, client)
{
    m_internal = SocketStreamHandleInternal::create(this);
    notImplemented();
}

SocketStreamHandle::~SocketStreamHandle()
{
    setClient(0);
    m_internal.clear();
}

int SocketStreamHandle::platformSend(const char* buf, int len)
{
    notImplemented();
    return 0;
}

void SocketStreamHandle::platformClose()
{
    notImplemented();
}

void SocketStreamHandle::didReceiveAuthenticationChallenge(const AuthenticationChallenge& challenge)
{
    notImplemented();
}

void SocketStreamHandle::receivedCredential(const AuthenticationChallenge& challenge, const Credential& credential)
{
    notImplemented();
}

void SocketStreamHandle::receivedRequestToContinueWithoutCredential(const AuthenticationChallenge& challenge)
{
    notImplemented();
}

void SocketStreamHandle::receivedCancellation(const AuthenticationChallenge& challenge)
{
    notImplemented();
}

}  // namespace WebCore

#endif  // ENABLE(WEB_SOCKETS)
