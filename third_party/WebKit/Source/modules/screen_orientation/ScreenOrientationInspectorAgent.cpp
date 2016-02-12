// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/screen_orientation/ScreenOrientationInspectorAgent.h"

#include "core/frame/LocalFrame.h"
#include "modules/screen_orientation/ScreenOrientation.h"
#include "modules/screen_orientation/ScreenOrientationController.h"
#include "platform/inspector_protocol/TypeBuilder.h"

namespace blink {

namespace ScreenOrientationInspectorAgentState {
static const char angle[] = "angle";
static const char type[] = "type";
static const char overrideEnabled[] = "overrideEnabled";
}

namespace {

WebScreenOrientationType WebScreenOrientationTypeFromString(const String& type)
{
    if (type == protocol::TypeBuilder::getEnumConstantValue(protocol::TypeBuilder::ScreenOrientation::OrientationType::PortraitPrimary))
        return WebScreenOrientationPortraitPrimary;
    if (type == protocol::TypeBuilder::getEnumConstantValue(protocol::TypeBuilder::ScreenOrientation::OrientationType::PortraitSecondary))
        return WebScreenOrientationPortraitSecondary;
    if (type == protocol::TypeBuilder::getEnumConstantValue(protocol::TypeBuilder::ScreenOrientation::OrientationType::LandscapePrimary))
        return WebScreenOrientationLandscapePrimary;
    if (type == protocol::TypeBuilder::getEnumConstantValue(protocol::TypeBuilder::ScreenOrientation::OrientationType::LandscapeSecondary))
        return WebScreenOrientationLandscapeSecondary;
    return WebScreenOrientationUndefined;
}

} // namespace

// static
PassOwnPtrWillBeRawPtr<ScreenOrientationInspectorAgent> ScreenOrientationInspectorAgent::create(LocalFrame& frame)
{
    return adoptPtrWillBeNoop(new ScreenOrientationInspectorAgent(frame));
}

ScreenOrientationInspectorAgent::~ScreenOrientationInspectorAgent()
{
}

ScreenOrientationInspectorAgent::ScreenOrientationInspectorAgent(LocalFrame& frame)
    : InspectorBaseAgent<ScreenOrientationInspectorAgent, protocol::Frontend::ScreenOrientation>("ScreenOrientation")
    , m_frame(&frame)
{
}

DEFINE_TRACE(ScreenOrientationInspectorAgent)
{
    visitor->trace(m_frame);
    InspectorBaseAgent<ScreenOrientationInspectorAgent, protocol::Frontend::ScreenOrientation>::trace(visitor);
}

void ScreenOrientationInspectorAgent::setScreenOrientationOverride(ErrorString* error, int angle, const String& typeString)
{
    if (angle < 0 || angle >= 360) {
        *error = "Angle should be in [0; 360) interval";
        return;
    }
    WebScreenOrientationType type = WebScreenOrientationTypeFromString(typeString);
    if (type == WebScreenOrientationUndefined) {
        *error = "Wrong type value";
        return;
    }
    ScreenOrientationController* controller = ScreenOrientationController::from(*m_frame);
    if (!controller) {
        *error = "Cannot connect to orientation controller";
        return;
    }
    m_state->setBoolean(ScreenOrientationInspectorAgentState::overrideEnabled, true);
    m_state->setNumber(ScreenOrientationInspectorAgentState::angle, angle);
    m_state->setNumber(ScreenOrientationInspectorAgentState::type, type);
    controller->setOverride(type, angle);
}

void ScreenOrientationInspectorAgent::clearScreenOrientationOverride(ErrorString* error)
{
    ScreenOrientationController* controller = ScreenOrientationController::from(*m_frame);
    if (!controller) {
        *error = "Cannot connect to orientation controller";
        return;
    }
    m_state->setBoolean(ScreenOrientationInspectorAgentState::overrideEnabled, false);
    controller->clearOverride();
}

void ScreenOrientationInspectorAgent::disable(ErrorString*)
{
    m_state->setBoolean(ScreenOrientationInspectorAgentState::overrideEnabled, false);
    if (ScreenOrientationController* controller = ScreenOrientationController::from(*m_frame))
        controller->clearOverride();
}

void ScreenOrientationInspectorAgent::restore()
{
    if (m_state->booleanProperty(ScreenOrientationInspectorAgentState::overrideEnabled, false)) {
        long longType = static_cast<long>(WebScreenOrientationUndefined);
        m_state->getNumber(ScreenOrientationInspectorAgentState::type, &longType);
        WebScreenOrientationType type = static_cast<WebScreenOrientationType>(longType);
        int angle = 0;
        m_state->getNumber(ScreenOrientationInspectorAgentState::angle, &angle);
        if (ScreenOrientationController* controller = ScreenOrientationController::from(*m_frame))
            controller->setOverride(type, angle);
    }
}

} // namespace blink
