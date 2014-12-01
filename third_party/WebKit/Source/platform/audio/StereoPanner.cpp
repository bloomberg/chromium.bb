// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#if ENABLE(WEB_AUDIO)

#include "platform/audio/StereoPanner.h"

#include "platform/audio/AudioBus.h"
#include "platform/audio/AudioUtilities.h"
#include "wtf/MathExtras.h"
#include <algorithm>

// Use a 50ms smoothing / de-zippering time-constant.
const float SmoothingTimeConstant = 0.050f;

namespace blink {

// Implement equal-power panning algorithm for mono or stereo input.
// See: http://webaudio.github.io/web-audio-api/#panning-algorithm

StereoPanner::StereoPanner(float sampleRate) : Spatializer(PanningModelEqualPower)
    , m_isFirstRender(true)
    , m_gainL(0.5)
    , m_gainR(0.5)
{
    m_smoothingConstant = AudioUtilities::discreteTimeConstantForSampleRate(SmoothingTimeConstant, sampleRate);
}

void StereoPanner::panWithSampleAccurateValues(const AudioBus* inputBus, AudioBus* outputBus, const float* panValues, size_t framesToProcess)
{
    bool isInputSafe = inputBus
        && (inputBus->numberOfChannels() == 1 || inputBus->numberOfChannels() == 2)
        && framesToProcess <= inputBus->length();
    ASSERT(isInputSafe);
    if (!isInputSafe)
        return;

    unsigned numberOfInputChannels = inputBus->numberOfChannels();

    bool isOutputSafe = outputBus
        && outputBus->numberOfChannels() == 2
        && framesToProcess <= outputBus->length();
    ASSERT(isOutputSafe);
    if (!isOutputSafe)
        return;

    const float* sourceL = inputBus->channel(0)->data();
    const float* sourceR = numberOfInputChannels > 1 ? inputBus->channel(1)->data() : sourceL;
    float* destinationL = outputBus->channelByType(AudioBus::ChannelLeft)->mutableData();
    float* destinationR = outputBus->channelByType(AudioBus::ChannelRight)->mutableData();

    if (!sourceL || !sourceR || !destinationL || !destinationR)
        return;

    // Cache in local variables.
    double gainL = m_gainL;
    double gainR = m_gainR;

    int n = framesToProcess;

    if (numberOfInputChannels == 1) { // For mono source case.
        while (n--) {
            float inputL = *sourceL++;
            float panValue = clampTo(*panValues++, -1.0, 1.0);
            double panPosition = (panValue * 0.5 + 0.5) * piOverTwoDouble;
            gainL = std::cos(panPosition);
            gainR = std::sin(panPosition);
            *destinationL++ = static_cast<float>(inputL * gainL);
            *destinationR++ = static_cast<float>(inputL * gainR);
        }
    } else { // For stereo source case.
        while (n--) {
            float inputL = *sourceL++;
            float inputR = *sourceR++;
            float panValue = clampTo(*panValues++, -1.0, 1.0);
            double panPosition = panValue <= 0 ? panValue + 1 : panValue;
            panPosition *= piOverTwoDouble;
            gainL = std::cos(panPosition);
            gainR = std::sin(panPosition);
            if (panValue <= 0) {
                *destinationL++ = static_cast<float>(inputL + inputR * gainL);
                *destinationR++ = static_cast<float>(inputR * gainR);
            } else {
                *destinationL++ = static_cast<float>(inputL * gainL);
                *destinationR++ = static_cast<float>(inputR + inputL * gainR);
            }
        }
    }

    m_gainL = gainL;
    m_gainR = gainR;
}

void StereoPanner::panToTargetValue(const AudioBus* inputBus, AudioBus* outputBus, float panValue, size_t framesToProcess)
{
    bool isInputSafe = inputBus
        && (inputBus->numberOfChannels() == 1 || inputBus->numberOfChannels() == 2)
        && framesToProcess <= inputBus->length();
    ASSERT(isInputSafe);
    if (!isInputSafe)
        return;

    unsigned numberOfInputChannels = inputBus->numberOfChannels();

    bool isOutputSafe = outputBus
        && outputBus->numberOfChannels() == 2
        && framesToProcess <= outputBus->length();
    ASSERT(isOutputSafe);
    if (!isOutputSafe)
        return;

    const float* sourceL = inputBus->channel(0)->data();
    const float* sourceR = numberOfInputChannels > 1 ? inputBus->channel(1)->data() : sourceL;
    float* destinationL = outputBus->channelByType(AudioBus::ChannelLeft)->mutableData();
    float* destinationR = outputBus->channelByType(AudioBus::ChannelRight)->mutableData();

    if (!sourceL || !sourceR || !destinationL || !destinationR)
        return;

    float panValueClamped = clampTo(panValue, -1.0, 1.0);
    double panPosition;
    // Normalize pan values to [0; 1] based on 1) channel type and the value
    // polarity.
    if (numberOfInputChannels == 1) { // For mono input source.
        // Pan from left to right [-1; 1] will be normalized as [0; 1].
        panPosition = panValueClamped * 0.5 + 0.5;
    } else { // For stereo input source.
        if (panValueClamped <= 0) { // Normalize [-1; 0] to [0; 1].
            panPosition = panValueClamped + 1;
        } else { // Do nothing when [0; 1].
            panPosition = panValueClamped;
        }
    }
    panPosition *= piOverTwoDouble;
    double targetGainL = std::cos(panPosition);
    double targetGainR = std::sin(panPosition);

    // Don't de-zipper on first render call.
    if (m_isFirstRender) {
        m_isFirstRender = false;
        m_gainL = targetGainL;
        m_gainR = targetGainR;
    }

    // Cache in local variables.
    double gainL = m_gainL;
    double gainR = m_gainR;

    // Get local copy of smoothing constant.
    const double SmoothingConstant = m_smoothingConstant;

    int n = framesToProcess;

    if (numberOfInputChannels == 1) { // For mono source case.
        while (n--) {
            float inputL = *sourceL++;
            gainL += (targetGainL - gainL) * SmoothingConstant;
            gainR += (targetGainR - gainR) * SmoothingConstant;
            *destinationL++ = static_cast<float>(inputL * gainL);
            *destinationR++ = static_cast<float>(inputL * gainR);
        }
    } else { // For stereo source case.
        if (panValueClamped <= 0) {
            // When [-1; 0], keep left channel intact and equal-power pan the
            // right channel only.
            while (n--) {
                float inputL = *sourceL++;
                float inputR = *sourceR++;
                gainL += (targetGainL - gainL) * SmoothingConstant;
                gainR += (targetGainR - gainR) * SmoothingConstant;
                *destinationL++ = static_cast<float>(inputL + inputR * gainL);
                *destinationR++ = static_cast<float>(inputR * gainR);
            }
        } else {
            // When [0; 1], keep right channel intact and equal-power pan the
            // left channel only.
            while (n--) {
                float inputL = *sourceL++;
                float inputR = *sourceR++;
                gainL += (targetGainL - gainL) * SmoothingConstant;
                gainR += (targetGainR - gainR) * SmoothingConstant;
                *destinationL++ = static_cast<float>(inputL * gainL);
                *destinationR++ = static_cast<float>(inputR + inputL * gainR);
            }
        }
    }

    m_gainL = gainL;
    m_gainR = gainR;
}

} // namespace blink

#endif // ENABLE(WEB_AUDIO)
