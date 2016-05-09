// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/vr/VRDisplay.h"

#include "core/dom/DOMException.h"
#include "core/inspector/ConsoleMessage.h"
#include "modules/vr/NavigatorVR.h"
#include "modules/vr/VRController.h"
#include "modules/vr/VRDisplayCapabilities.h"
#include "modules/vr/VREyeParameters.h"
#include "modules/vr/VRLayer.h"
#include "modules/vr/VRPose.h"
#include "modules/vr/VRStageParameters.h"
#include "modules/webgl/WebGLRenderingContextBase.h"
#include "public/platform/Platform.h"

namespace blink {

namespace {

VREye stringToVREye(const String& whichEye)
{
    if (whichEye == "left")
        return VREyeLeft;
    if (whichEye == "right")
        return VREyeRight;
    return VREyeNone;
}

} // namepspace

VRDisplay::VRDisplay(NavigatorVR* navigatorVR)
    : m_navigatorVR(navigatorVR)
    , m_displayId(0)
    , m_isConnected(false)
    , m_isPresenting(false)
    , m_canUpdateFramePose(true)
    , m_capabilities(new VRDisplayCapabilities())
    , m_eyeParametersLeft(new VREyeParameters())
    , m_eyeParametersRight(new VREyeParameters())
{
}

VRDisplay::~VRDisplay()
{
}

VRController* VRDisplay::controller()
{
    return m_navigatorVR->controller();
}

void VRDisplay::updateFromWebVRDevice(const WebVRDevice& device)
{
    m_displayId = device.index;
    m_displayName = device.deviceName;
    m_isConnected = true;

    // Defaults until the VR service has been update to query these.
    m_capabilities->setHasOrientation(true);
    m_capabilities->setHasPosition(false);
    m_capabilities->setHasExternalDisplay(false);
    m_capabilities->setCanPresent(false);
    m_capabilities->setMaxLayers(0);

    if (device.flags & WebVRDeviceTypeHMD) {
        m_eyeParametersLeft->update(device.hmdInfo.leftEye);
        m_eyeParametersRight->update(device.hmdInfo.rightEye);
    }

    m_stageParameters = nullptr;
}

VRPose* VRDisplay::getPose()
{
    if (m_canUpdateFramePose) {
        m_framePose = getImmediatePose();
        Platform::current()->currentThread()->addTaskObserver(this);
        m_canUpdateFramePose = false;
    }

    return m_framePose;
}

VRPose* VRDisplay::getImmediatePose()
{
    WebHMDSensorState webPose;
    controller()->getSensorState(m_displayId, webPose);

    VRPose* pose = VRPose::create();
    pose->setPose(webPose);
    return pose;
}

void VRDisplay::resetPose()
{
    controller()->resetSensor(m_displayId);
}

VREyeParameters* VRDisplay::getEyeParameters(const String& whichEye)
{
    switch (stringToVREye(whichEye)) {
    case VREyeLeft:
        return m_eyeParametersLeft;
    case VREyeRight:
        return m_eyeParametersRight;
    default:
        return nullptr;
    }
}

int VRDisplay::requestAnimationFrame(FrameRequestCallback* callback)
{
    // TODO: Use HMD-specific rAF when an external display is present.
    callback->m_useLegacyTimeBase = false;
    if (Document* doc = m_navigatorVR->document())
        return doc->requestAnimationFrame(callback);
    return 0;
}

void VRDisplay::cancelAnimationFrame(int id)
{
    if (Document* document = m_navigatorVR->document())
        document->cancelAnimationFrame(id);
}

ScriptPromise VRDisplay::requestPresent(ScriptState* scriptState, const HeapVector<VRLayer>& layers)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    if (!m_capabilities->canPresent()) {
        DOMException* exception = DOMException::create(InvalidStateError, "VRDisplay cannot present");
        resolver->reject(exception);
        return promise;
    }

    if (layers.size() == 0 || layers.size() > m_capabilities->maxLayers()) {
        DOMException* exception = DOMException::create(InvalidStateError, "Invalid number of layers.");
        if (m_isPresenting) {
            exitPresent(scriptState);
        }
        resolver->reject(exception);
        return promise;
    }

    // TODO: Implement VR presentation
    NOTIMPLEMENTED();

    DOMException* exception = DOMException::create(InvalidStateError, "VR Presentation not implemented");
    resolver->reject(exception);

    return promise;
}

ScriptPromise VRDisplay::exitPresent(ScriptState* scriptState)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    // Can't stop presenting if we're not presenting.
    DOMException* exception = DOMException::create(InvalidStateError, "VRDisplay is not presenting");
    resolver->reject(exception);
    return promise;
}

HeapVector<VRLayer> VRDisplay::getLayers()
{
    HeapVector<VRLayer> layers;
    return layers;
}

void VRDisplay::submitFrame(VRPose* pose)
{
    NOTIMPLEMENTED();
}

void VRDisplay::didProcessTask()
{
    // Pose should be stable until control is returned to the user agent.
    if (!m_canUpdateFramePose) {
        Platform::current()->currentThread()->removeTaskObserver(this);
        m_canUpdateFramePose = true;
    }
}

DEFINE_TRACE(VRDisplay)
{
    visitor->trace(m_navigatorVR);
    visitor->trace(m_capabilities);
    visitor->trace(m_stageParameters);
    visitor->trace(m_eyeParametersLeft);
    visitor->trace(m_eyeParametersRight);
    visitor->trace(m_framePose);
}

} // namespace blink
