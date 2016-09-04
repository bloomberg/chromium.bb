// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/vr/VRDisplay.h"

#include "core/dom/DOMException.h"
#include "core/dom/Fullscreen.h"
#include "core/inspector/ConsoleMessage.h"
#include "modules/vr/NavigatorVR.h"
#include "modules/vr/VRController.h"
#include "modules/vr/VRDisplayCapabilities.h"
#include "modules/vr/VREyeParameters.h"
#include "modules/vr/VRLayer.h"
#include "modules/vr/VRPose.h"
#include "modules/vr/VRStageParameters.h"
#include "modules/webgl/WebGLRenderingContextBase.h"
#include "platform/UserGestureIndicator.h"
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

} // namespace

VRDisplay::VRDisplay(NavigatorVR* navigatorVR)
    : m_navigatorVR(navigatorVR)
    , m_displayId(0)
    , m_isConnected(false)
    , m_isPresenting(false)
    , m_canUpdateFramePose(true)
    , m_capabilities(new VRDisplayCapabilities())
    , m_eyeParametersLeft(new VREyeParameters())
    , m_eyeParametersRight(new VREyeParameters())
    , m_fullscreenCheckTimer(this, &VRDisplay::onFullscreenCheck)
{
}

VRDisplay::~VRDisplay()
{
}

VRController* VRDisplay::controller()
{
    return m_navigatorVR->controller();
}

void VRDisplay::update(const device::blink::VRDisplayPtr& display)
{
    m_displayId = display->index;
    m_displayName = display->displayName;
    m_isConnected = true;

    m_capabilities->setHasOrientation(display->capabilities->hasOrientation);
    m_capabilities->setHasPosition(display->capabilities->hasPosition);
    m_capabilities->setHasExternalDisplay(display->capabilities->hasExternalDisplay);
    m_capabilities->setCanPresent(display->capabilities->canPresent);
    m_capabilities->setMaxLayers(display->capabilities->canPresent ? 1 : 0);

    m_eyeParametersLeft->update(display->leftEye);
    m_eyeParametersRight->update(display->rightEye);

    if (!display->stageParameters.is_null()) {
        if (!m_stageParameters)
            m_stageParameters = new VRStageParameters();
        m_stageParameters->update(display->stageParameters);
    } else {
        m_stageParameters = nullptr;
    }
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
    VRPose* pose = VRPose::create();
    pose->setPose(controller()->getPose(m_displayId));
    return pose;
}

void VRDisplay::resetPose()
{
    controller()->resetPose(m_displayId);
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

    // If the VRDisplay does not advertise the ability to present reject the request.
    if (!m_capabilities->canPresent()) {
        DOMException* exception = DOMException::create(InvalidStateError, "VRDisplay cannot present.");
        resolver->reject(exception);
        return promise;
    }

    // Initiating VR presentation is only allowed in response to a user gesture.
    // If the VRDisplay is already presenting, however, repeated calls are
    // allowed outside a user gesture so that the presented content may be
    // updated.
    if (!m_isPresenting && !UserGestureIndicator::utilizeUserGesture()) {
        DOMException* exception = DOMException::create(InvalidStateError, "API can only be initiated by a user gesture.");
        resolver->reject(exception);
        return promise;
    }

    m_isPresenting = false;

    // A valid number of layers must be provided in order to present.
    if (layers.size() == 0 || layers.size() > m_capabilities->maxLayers()) {
        DOMException* exception = DOMException::create(InvalidStateError, "Invalid number of layers.");
        if (m_isPresenting) {
            exitPresent(scriptState);
        }
        resolver->reject(exception);
        return promise;
    }

    m_layer = layers[0];

    if (m_layer.source()) {
        if (!m_capabilities->hasExternalDisplay()) {
            // TODO: Need a proper VR compositor, but for the moment on mobile
            // we'll just make the canvas fullscreen so that VrShell can pick it
            // up through the standard (high latency) compositing path.
            Fullscreen::from(m_layer.source()->document()).requestFullscreen(*m_layer.source(), Fullscreen::UnprefixedRequest);

            m_isPresenting = true;

            resolver->resolve();

            m_navigatorVR->fireVRDisplayPresentChange(this);

            // Check to see if the canvas is still the current fullscreen
            // element once per second.
            m_fullscreenCheckTimer.startRepeating(1.0, BLINK_FROM_HERE);
        } else {
            DOMException* exception = DOMException::create(InvalidStateError, "VR Presentation not implemented for this VRDisplay.");
            resolver->reject(exception);
        }
    } else {
        DOMException* exception = DOMException::create(InvalidStateError, "Invalid layer source.");
        resolver->reject(exception);
    }

    return promise;
}

ScriptPromise VRDisplay::exitPresent(ScriptState* scriptState)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    if (!m_isPresenting) {
        // Can't stop presenting if we're not presenting.
        DOMException* exception = DOMException::create(InvalidStateError, "VRDisplay is not presenting.");
        resolver->reject(exception);
        return promise;
    }

    if (!m_capabilities->hasExternalDisplay()) {
        Fullscreen::fullyExitFullscreen(m_layer.source()->document());
        m_fullscreenCheckTimer.stop();
    } else {
        // Can't get into this presentation mode, so nothing to do here.
    }

    m_isPresenting = false;

    // TODO: Resolve when exit is confirmed
    resolver->resolve();

    m_navigatorVR->fireVRDisplayPresentChange(this);

    return promise;
}

HeapVector<VRLayer> VRDisplay::getLayers()
{
    HeapVector<VRLayer> layers;

    if (m_isPresenting) {
        layers.append(m_layer);
    }

    return layers;
}

void VRDisplay::submitFrame(VRPose* pose)
{
}

void VRDisplay::didProcessTask()
{
    // Pose should be stable until control is returned to the user agent.
    if (!m_canUpdateFramePose) {
        Platform::current()->currentThread()->removeTaskObserver(this);
        m_canUpdateFramePose = true;
    }
}

void VRDisplay::onFullscreenCheck(TimerBase*)
{
    // TODO: This is a temporary measure to track if fullscreen mode has been
    // exited by the UA. If so we need to end VR presentation. Soon we won't
    // depend on the Fullscreen API to fake VR presentation, so this will
    // become unnessecary. Until that point, though, this seems preferable to
    // adding a bunch of notification plumbing to Fullscreen.
    if (!Fullscreen::isCurrentFullScreenElement(*m_layer.source())) {
        m_isPresenting = false;
        m_navigatorVR->fireVRDisplayPresentChange(this);
        m_fullscreenCheckTimer.stop();
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
    visitor->trace(m_layer);
}

} // namespace blink
