// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webaudio/IIRFilterNode.h"

#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "modules/webaudio/AudioBasicProcessorHandler.h"

namespace blink {

IIRFilterNode::IIRFilterNode(AbstractAudioContext& context, float sampleRate, const Vector<double> feedforwardCoef, const Vector<double> feedbackCoef)
    : AudioNode(context)
{
    setHandler(AudioBasicProcessorHandler::create(
        AudioHandler::NodeTypeIIRFilter, *this, sampleRate,
        adoptPtr(new IIRProcessor(sampleRate, 1, feedforwardCoef, feedbackCoef))));
}

DEFINE_TRACE(IIRFilterNode)
{
    AudioNode::trace(visitor);
}

IIRProcessor* IIRFilterNode::iirProcessor() const
{
    return static_cast<IIRProcessor*>(static_cast<AudioBasicProcessorHandler&>(handler()).processor());
}

void IIRFilterNode::getFrequencyResponse(const DOMFloat32Array* frequencyHz, DOMFloat32Array* magResponse, DOMFloat32Array* phaseResponse, ExceptionState& exceptionState)
{
    if (!frequencyHz) {
        exceptionState.throwDOMException(
            NotSupportedError,
            "frequencyHz array cannot be null");
        return;
    }

    if (!magResponse) {
        exceptionState.throwDOMException(
            NotSupportedError,
            "magResponse array cannot be null");
        return;
    }

    if (!phaseResponse) {
        exceptionState.throwDOMException(
            NotSupportedError,
            "phaseResponse array cannot be null");
        return;
    }

    unsigned frequencyHzLength = frequencyHz->length();

    if (magResponse->length() < frequencyHzLength) {
        exceptionState.throwDOMException(
            NotSupportedError,
            ExceptionMessages::indexExceedsMinimumBound(
                "magResponse length",
                magResponse->length(),
                frequencyHzLength));
        return;
    }

    if (phaseResponse->length() < frequencyHzLength) {
        exceptionState.throwDOMException(
            NotSupportedError,
            ExceptionMessages::indexExceedsMinimumBound(
                "phaseResponse length",
                phaseResponse->length(),
                frequencyHzLength));
        return;
    }

    iirProcessor()->getFrequencyResponse(frequencyHzLength, frequencyHz->data(), magResponse->data(), phaseResponse->data());
}

} // namespace blink
