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

#include "WebUserMediaClientMock.h"

#include "MockConstraints.h"
#include "public/platform/WebMediaConstraints.h"
#include "public/platform/WebMediaStream.h"
#include "public/platform/WebMediaStreamSource.h"
#include "public/platform/WebMediaStreamTrack.h"
#include "public/platform/WebVector.h"
#include "public/testing/WebTestDelegate.h"
#include "public/web/WebDocument.h"
#include "public/web/WebMediaStreamRegistry.h"
#include "public/web/WebUserMediaRequest.h"

using namespace WebKit;

namespace WebTestRunner {

class UserMediaRequestTask : public WebMethodTask<WebUserMediaClientMock> {
public:
    UserMediaRequestTask(WebUserMediaClientMock* object, const WebUserMediaRequest& request, const WebMediaStream result)
        : WebMethodTask<WebUserMediaClientMock>(object)
        , m_request(request)
        , m_result(result)
    {
        BLINK_ASSERT(!m_result.isNull());
    }

    virtual void runIfValid() OVERRIDE
    {
        m_request.requestSucceeded(m_result);
    }

private:
    WebUserMediaRequest m_request;
    WebMediaStream m_result;
};

class UserMediaRequestConstraintFailedTask : public WebMethodTask<WebUserMediaClientMock> {
public:
    UserMediaRequestConstraintFailedTask(WebUserMediaClientMock* object, const WebUserMediaRequest& request, const WebString& constraint)
        : WebMethodTask<WebUserMediaClientMock>(object)
        , m_request(request)
        , m_constraint(constraint)
    {
    }

    virtual void runIfValid() OVERRIDE
    {
        m_request.requestFailedConstraint(m_constraint);
    }

private:
    WebUserMediaRequest m_request;
    WebString m_constraint;
};

class UserMediaRequestPermissionDeniedTask : public WebMethodTask<WebUserMediaClientMock> {
public:
    UserMediaRequestPermissionDeniedTask(WebUserMediaClientMock* object, const WebUserMediaRequest& request)
        : WebMethodTask<WebUserMediaClientMock>(object)
        , m_request(request)
    {
    }

    virtual void runIfValid() OVERRIDE
    {
        m_request.requestFailed();
    }

private:
    WebUserMediaRequest m_request;
};

////////////////////////////////

class MockExtraData : public WebMediaStream::ExtraData {
public:
    int foo;
};

WebUserMediaClientMock::WebUserMediaClientMock(WebTestDelegate* delegate)
    : m_delegate(delegate)
{
}

void WebUserMediaClientMock::requestUserMedia(const WebUserMediaRequest& streamRequest)
{
    BLINK_ASSERT(!streamRequest.isNull());
    WebUserMediaRequest request = streamRequest;

    if (request.ownerDocument().isNull() || !request.ownerDocument().frame()) {
        m_delegate->postTask(new UserMediaRequestPermissionDeniedTask(this, request));
        return;
    }

    WebMediaConstraints constraints = request.audioConstraints();
    WebString failedConstraint;
    if (!constraints.isNull() && !MockConstraints::verifyConstraints(constraints, &failedConstraint)) {
        m_delegate->postTask(new UserMediaRequestConstraintFailedTask(this, request, failedConstraint));
        return;
    }
    constraints = request.videoConstraints();
    if (!constraints.isNull() && !MockConstraints::verifyConstraints(constraints, &failedConstraint)) {
        m_delegate->postTask(new UserMediaRequestConstraintFailedTask(this, request, failedConstraint));
        return;
    }

    const size_t zero = 0;
    const size_t one = 1;
    WebVector<WebMediaStreamTrack> audioTracks(request.audio() ? one : zero);
    WebVector<WebMediaStreamTrack> videoTracks(request.video() ? one : zero);

    if (request.audio()) {
        WebMediaStreamSource source;
        source.initialize("MockAudioDevice#1", WebMediaStreamSource::TypeAudio, "Mock audio device");
        audioTracks[0].initialize(source);
    }

    if (request.video()) {
        WebMediaStreamSource source;
        source.initialize("MockVideoDevice#1", WebMediaStreamSource::TypeVideo, "Mock video device");
        videoTracks[0].initialize(source);
    }

    WebMediaStream stream;
    stream.initialize(audioTracks, videoTracks);

    stream.setExtraData(new MockExtraData());

    m_delegate->postTask(new UserMediaRequestTask(this, request, stream));
}

void WebUserMediaClientMock::cancelUserMediaRequest(const WebUserMediaRequest&)
{
}

}
