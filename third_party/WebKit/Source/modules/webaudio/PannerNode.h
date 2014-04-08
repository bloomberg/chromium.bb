/*
 * Copyright (C) 2010, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef PannerNode_h
#define PannerNode_h

#include "platform/audio/AudioBus.h"
#include "platform/audio/Cone.h"
#include "platform/audio/Distance.h"
#include "platform/audio/HRTFDatabaseLoader.h"
#include "platform/audio/Panner.h"
#include "modules/webaudio/AudioListener.h"
#include "modules/webaudio/AudioNode.h"
#include "modules/webaudio/AudioParam.h"
#include "platform/geometry/FloatPoint3D.h"
#include "wtf/HashMap.h"
#include "wtf/OwnPtr.h"

namespace WebCore {

// PannerNode is an AudioNode with one input and one output.
// It positions a sound in 3D space, with the exact effect dependent on the panning model.
// It has a position and an orientation in 3D space which is relative to the position and orientation of the context's AudioListener.
// A distance effect will attenuate the gain as the position moves away from the listener.
// A cone effect will attenuate the gain as the orientation moves away from the listener.
// All of these effects follow the OpenAL specification very closely.

class PannerNode FINAL : public AudioNode {
public:
    // These must be defined as in the .idl file and must match those in the Panner class.
    enum {
        EQUALPOWER = 0,
        HRTF = 1,
    };

    // These must be defined as in the .idl file and must match those
    // in the DistanceEffect class.
    enum {
        LINEAR_DISTANCE = 0,
        INVERSE_DISTANCE = 1,
        EXPONENTIAL_DISTANCE = 2,
    };

    static PassRefPtr<PannerNode> create(AudioContext* context, float sampleRate)
    {
        return adoptRef(new PannerNode(context, sampleRate));
    }

    virtual ~PannerNode();

    // AudioNode
    virtual void process(size_t framesToProcess) OVERRIDE;
    virtual void pullInputs(size_t framesToProcess) OVERRIDE;
    virtual void initialize() OVERRIDE;
    virtual void uninitialize() OVERRIDE;

    // AudioContext's listener
    AudioListener* listener();

    // Panning model
    String panningModel() const;
    void setPanningModel(const String&);

    // Position
    void setPosition(float x, float y, float z);

    // Orientation
    void setOrientation(float x, float y, float z);

    // Velocity
    void setVelocity(float x, float y, float z);

    // Distance parameters
    String distanceModel() const;
    void setDistanceModel(const String&);

    double refDistance() { return m_distanceEffect.refDistance(); }
    void setRefDistance(double refDistance) { m_distanceEffect.setRefDistance(refDistance); }

    double maxDistance() { return m_distanceEffect.maxDistance(); }
    void setMaxDistance(double maxDistance) { m_distanceEffect.setMaxDistance(maxDistance); }

    double rolloffFactor() { return m_distanceEffect.rolloffFactor(); }
    void setRolloffFactor(double rolloffFactor) { m_distanceEffect.setRolloffFactor(rolloffFactor); }

    // Sound cones - angles in degrees
    double coneInnerAngle() const { return m_coneEffect.innerAngle(); }
    void setConeInnerAngle(double angle) { m_coneEffect.setInnerAngle(angle); }

    double coneOuterAngle() const { return m_coneEffect.outerAngle(); }
    void setConeOuterAngle(double angle) { m_coneEffect.setOuterAngle(angle); }

    double coneOuterGain() const { return m_coneEffect.outerGain(); }
    void setConeOuterGain(double angle) { m_coneEffect.setOuterGain(angle); }

    // It must be called on audio thread, currently called only process() in AudioBufferSourceNode.
    double dopplerRate();

    virtual double tailTime() const OVERRIDE { return m_panner ? m_panner->tailTime() : 0; }
    virtual double latencyTime() const OVERRIDE { return m_panner ? m_panner->latencyTime() : 0; }

private:
    PannerNode(AudioContext*, float sampleRate);

    bool setPanningModel(unsigned); // Returns true on success.
    bool setDistanceModel(unsigned); // Returns true on success.
    void calculateAzimuthElevation(double* outAzimuth, double* outElevation);
    // Returns the combined distance and cone gain attenuation.
    float calculateDistanceConeGain();
    double calculateDopplerRate();

    void azimuthElevation(double* outAzimuth, double* outElevation);
    float distanceConeGain();

    bool isAzimuthElevationDirty();
    bool isDistanceConeGainDirty();
    bool isDopplerRateDirty();

    // Notifies any AudioBufferSourceNodes connected to us either directly or indirectly about our existence.
    // This is in order to handle the pitch change necessary for the doppler shift.
    void notifyAudioSourcesConnectedToNode(AudioNode*, HashMap<AudioNode*, bool> &visitedNodes);

    void updateCachedListener();
    void updateCachedSourceLocationInfo();

    OwnPtr<Panner> m_panner;
    unsigned m_panningModel;
    unsigned m_distanceModel;

    // Current source location information
    FloatPoint3D m_position;
    FloatPoint3D m_orientation;
    FloatPoint3D m_velocity;

    // Cached source location information
    FloatPoint3D m_cachedPosition;
    FloatPoint3D m_cachedOrientation;
    FloatPoint3D m_cachedVelocity;

    // Gain
    RefPtr<AudioParam> m_distanceGain;
    RefPtr<AudioParam> m_coneGain;
    DistanceEffect m_distanceEffect;
    ConeEffect m_coneEffect;
    float m_lastGain;

    double m_cachedAzimuth;
    double m_cachedElevation;
    float m_cachedDistanceConeGain;
    double m_cachedDopplerRate;

    // Cached listener parameters after processing.
    RefPtr<AudioListener> m_cachedListener;

    RefPtr<HRTFDatabaseLoader> m_hrtfDatabaseLoader;

    // AudioContext's connection count
    unsigned m_connectionCount;

    // Synchronize process() with setting of the panning model, distance model and caching of the source location/orientation info.
    mutable Mutex m_processLock;
};

} // namespace WebCore

#endif // PannerNode_h
