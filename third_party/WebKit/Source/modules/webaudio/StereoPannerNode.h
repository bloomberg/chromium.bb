// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StereoPannerNode_h
#define StereoPannerNode_h

#include "modules/webaudio/AudioNode.h"
#include "modules/webaudio/AudioParam.h"
#include "platform/audio/AudioBus.h"
#include "platform/audio/Spatializer.h"

namespace blink {

// StereoPannerNode is an AudioNode with one input and one output. It is
// specifically designed for equal-power stereo panning.
class StereoPannerNode final : public AudioNode {
    DEFINE_WRAPPERTYPEINFO();
public:
    static StereoPannerNode* create(AudioContext* context, float sampleRate)
    {
        return new StereoPannerNode(context, sampleRate);
    }

    virtual ~StereoPannerNode();

    virtual void dispose() override;
    virtual void process(size_t framesToProcess) override;
    virtual void initialize() override;
    virtual void uninitialize() override;

    virtual double tailTime() const override { return 0; }
    virtual double latencyTime() const override { return 0; }

    virtual void setChannelCount(unsigned long, ExceptionState&) final;
    virtual void setChannelCountMode(const String&, ExceptionState&) final;

    DECLARE_VIRTUAL_TRACE();

    AudioParam* pan() { return m_pan.get(); }

private:
    StereoPannerNode(AudioContext*, float sampleRate);

    Member<Spatializer> m_stereoPanner;
    Member<AudioParam> m_pan;

    AudioFloatArray m_sampleAccuratePanValues;
};

} // namespace blink

#endif // StereoPannerNode_h
