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

#include "MockWebMediaStreamCenter.h"

#include "public/platform/WebAudioDestinationConsumer.h"
#include "public/platform/WebMediaStream.h"
#include "public/platform/WebMediaStreamCenterClient.h"
#include "public/platform/WebMediaStreamSource.h"
#include "public/platform/WebMediaStreamTrack.h"
#include "public/platform/WebMediaStreamTrackSourcesRequest.h"
#include "public/platform/WebSourceInfo.h"
#include "public/platform/WebVector.h"

using namespace WebKit;

namespace WebTestRunner {

MockWebMediaStreamCenter::MockWebMediaStreamCenter(WebMediaStreamCenterClient* client)
{
}

bool MockWebMediaStreamCenter::getMediaStreamTrackSources(const WebMediaStreamTrackSourcesRequest& request)
{
    size_t size = 2;
    WebVector<WebSourceInfo> results(size);
    results[0].initialize("MockAudioDevice#1", WebSourceInfo::SourceKindAudio, "Mock audio device", WebSourceInfo::VideoFacingModeNone);
    results[1].initialize("MockVideoDevice#1", WebSourceInfo::SourceKindVideo, "Mock video device", WebSourceInfo::VideoFacingModeEnvironment);
    request.requestSucceeded(results);
    return true;
}

void MockWebMediaStreamCenter::didEnableMediaStreamTrack(const WebMediaStream&, const WebMediaStreamTrack& component)
{
    component.source().setReadyState(WebMediaStreamSource::ReadyStateLive);
}

void MockWebMediaStreamCenter::didDisableMediaStreamTrack(const WebMediaStream&, const WebMediaStreamTrack& component)
{
    component.source().setReadyState(WebMediaStreamSource::ReadyStateMuted);
}

bool MockWebMediaStreamCenter::didAddMediaStreamTrack(const WebMediaStream&, const WebMediaStreamTrack&)
{
    return true;
}

bool MockWebMediaStreamCenter::didRemoveMediaStreamTrack(const WebMediaStream&, const WebMediaStreamTrack&)
{
    return true;
}

void MockWebMediaStreamCenter::didStopLocalMediaStream(const WebMediaStream& stream)
{
}

class MockWebAudioDestinationConsumer : public WebAudioDestinationConsumer {
public:
    virtual ~MockWebAudioDestinationConsumer() { }
    virtual void setFormat(size_t numberOfChannels, float sampleRate) OVERRIDE { }
    virtual void consumeAudio(const WebVector<const float*>&, size_t number_of_frames) OVERRIDE { }
};

void MockWebMediaStreamCenter::didCreateMediaStream(WebMediaStream& stream)
{
    WebVector<WebMediaStreamTrack> audioTracks;
    stream.audioTracks(audioTracks);
    for (size_t i = 0; i < audioTracks.size(); ++i) {
        WebMediaStreamSource source = audioTracks[i].source();
        if (source.requiresAudioConsumer()) {
            MockWebAudioDestinationConsumer* consumer = new MockWebAudioDestinationConsumer();
            source.addAudioConsumer(consumer);
            source.removeAudioConsumer(consumer);
            delete consumer;
        }
    }
}

}
