// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VRDisplay_h
#define VRDisplay_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/dom/Document.h"
#include "core/dom/FrameRequestCallback.h"
#include "device/vr/vr_service.mojom-blink.h"
#include "modules/vr/VRDisplayCapabilities.h"
#include "modules/vr/VRLayer.h"
#include "platform/Timer.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebGraphicsContext3DProvider.h"
#include "wtf/Forward.h"
#include "wtf/text/WTFString.h"

namespace gpu {
namespace gles2 {
class GLES2Interface;
}
}

namespace blink {

class NavigatorVR;
class VRController;
class VREyeParameters;
class VRFrameData;
class VRStageParameters;
class VRPose;

class WebGLRenderingContextBase;

enum VREye {
    VREyeNone,
    VREyeLeft,
    VREyeRight
};

class VRDisplay final : public GarbageCollectedFinalized<VRDisplay>, public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:
    ~VRDisplay();

    unsigned displayId() const { return m_displayId; }
    const String& displayName() const { return m_displayName; }

    VRDisplayCapabilities* capabilities() const { return m_capabilities; }
    VRStageParameters* stageParameters() const { return m_stageParameters; }

    bool isConnected() const { return m_isConnected; }
    bool isPresenting() const { return m_isPresenting; }

    bool getFrameData(VRFrameData*);
    VRPose* getPose();
    void resetPose();

    double depthNear() const { return m_depthNear; }
    double depthFar() const { return m_depthFar; }

    void setDepthNear(double value) { m_depthNear = value; }
    void setDepthFar(double value) { m_depthFar = value; }

    VREyeParameters* getEyeParameters(const String&);

    int requestAnimationFrame(FrameRequestCallback*);
    void cancelAnimationFrame(int id);

    ScriptPromise requestPresent(ScriptState*, const HeapVector<VRLayer>& layers);
    ScriptPromise exitPresent(ScriptState*);

    HeapVector<VRLayer> getLayers();

    void submitFrame();

    DECLARE_VIRTUAL_TRACE();

protected:
    friend class VRController;

    VRDisplay(NavigatorVR*);

    void update(const device::blink::VRDisplayPtr&);
    void updatePose();

    VRController* controller();

private:
    void onFullscreenCheck(TimerBase*);

    Member<NavigatorVR> m_navigatorVR;
    unsigned m_displayId;
    String m_displayName;
    bool m_isConnected;
    bool m_isPresenting;
    bool m_canUpdateFramePose;
    unsigned m_compositorHandle;
    Member<VRDisplayCapabilities> m_capabilities;
    Member<VRStageParameters> m_stageParameters;
    Member<VREyeParameters> m_eyeParametersLeft;
    Member<VREyeParameters> m_eyeParametersRight;
    device::blink::VRPosePtr m_framePose;
    VRLayer m_layer;
    double m_depthNear;
    double m_depthFar;

    Timer<VRDisplay> m_fullscreenCheckTimer;
};

using VRDisplayVector = HeapVector<Member<VRDisplay>>;

} // namespace blink

#endif // VRDisplay_h
