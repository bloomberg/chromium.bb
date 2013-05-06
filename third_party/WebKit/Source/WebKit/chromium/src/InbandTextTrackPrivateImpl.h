/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef InbandTextTrackPrivateImpl_h
#define InbandTextTrackPrivateImpl_h

#include "core/platform/graphics/InbandTextTrackPrivate.h"
#include "WebInbandTextTrackClient.h"
#include <wtf/OwnPtr.h>

namespace WebCore {
class InbandTextTrackPrivateClient;
}

namespace WebKit {

class WebInbandTextTrack;

class InbandTextTrackPrivateImpl : public WebCore::InbandTextTrackPrivate,
                                   public WebInbandTextTrackClient {
public:
    explicit InbandTextTrackPrivateImpl(WebInbandTextTrack*);
    virtual ~InbandTextTrackPrivateImpl();

    // InbandTextTrackPrivate methods.
    void setClient(WebCore::InbandTextTrackPrivateClient*);

    virtual void setMode(Mode);
    virtual InbandTextTrackPrivate::Mode mode() const;

    virtual Kind kind() const;
    virtual bool isClosedCaptions() const;

    virtual AtomicString label() const;
    virtual AtomicString language() const;
    virtual bool isDefault() const;

    virtual int textTrackIndex() const;

    // WebInbandTextTrackClient methods.
    virtual void addWebVTTCue(double start,
                              double end,
                              const WebString& id,
                              const WebString& content,
                              const WebString& settings);

private:
    OwnPtr<WebInbandTextTrack> m_track;
};

}

#endif
