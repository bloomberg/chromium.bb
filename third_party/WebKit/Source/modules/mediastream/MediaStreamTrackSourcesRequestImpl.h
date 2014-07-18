/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GOOGLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef MediaStreamTrackSourcesRequestImpl_h
#define MediaStreamTrackSourcesRequestImpl_h

#include "modules/mediastream/SourceInfo.h"
#include "platform/mediastream/MediaStreamTrackSourcesRequest.h"

namespace blink {
class WebSourceInfo;
template<typename T> class WebVector;
}

namespace blink {

class ExecutionContext;
class MediaStreamTrackSourcesCallback;

class MediaStreamTrackSourcesRequestImpl FINAL : public MediaStreamTrackSourcesRequest {
public:
    static MediaStreamTrackSourcesRequestImpl* create(ExecutionContext&, PassOwnPtr<MediaStreamTrackSourcesCallback>);
    ~MediaStreamTrackSourcesRequestImpl();

    virtual String origin() OVERRIDE;
    virtual void requestSucceeded(const blink::WebVector<blink::WebSourceInfo>&) OVERRIDE;

    virtual void trace(Visitor*) OVERRIDE;

private:
    MediaStreamTrackSourcesRequestImpl(ExecutionContext&, PassOwnPtr<MediaStreamTrackSourcesCallback>);

    void performCallback();

    OwnPtr<MediaStreamTrackSourcesCallback> m_callback;
    RefPtrWillBeMember<ExecutionContext> m_executionContext;
    SourceInfoVector m_sourceInfos;
};

} // namespace blink

#endif // MediaStreamTrackSourcesRequestImpl_h
