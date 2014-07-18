// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RTCOfferOptions_h
#define RTCOfferOptions_h

#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"

namespace blink {

class RTCOfferOptions FINAL : public RefCounted<RTCOfferOptions> {
public:
    static PassRefPtr<RTCOfferOptions> create(int32_t offerToReceiveVideo, int32_t offerToReceiveAudio, bool voiceActivityDetection, bool iceRestart)
    {
        return adoptRef(new RTCOfferOptions(offerToReceiveVideo, offerToReceiveAudio, voiceActivityDetection, iceRestart));
    }

    int32_t offerToReceiveVideo() const { return m_offerToReceiveVideo; }
    int32_t offerToReceiveAudio() const { return m_offerToReceiveAudio; }
    bool voiceActivityDetection() const { return m_voiceActivityDetection; }
    bool iceRestart() const { return m_iceRestart; }

private:
    RTCOfferOptions(int32_t offerToReceiveVideo, int32_t offerToReceiveAudio, bool voiceActivityDetection, bool iceRestart)
        : m_offerToReceiveVideo(offerToReceiveVideo)
        , m_offerToReceiveAudio(offerToReceiveAudio)
        , m_voiceActivityDetection(voiceActivityDetection)
        , m_iceRestart(iceRestart)
    {
    }

    int32_t m_offerToReceiveVideo;
    int32_t m_offerToReceiveAudio;
    bool m_voiceActivityDetection;
    bool m_iceRestart;
};

} // namespace blink

#endif // RTCOfferOptions_h
