/*
* Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef InspectorOverrides_inl_h
#define InspectorOverrides_inl_h

namespace WebCore {

namespace InspectorInstrumentation {

bool forcePseudoStateImpl(InstrumentingAgents*, Element*, CSSSelector::PseudoType);
bool shouldApplyScreenWidthOverrideImpl(InstrumentingAgents*);
bool shouldApplyScreenHeightOverrideImpl(InstrumentingAgents*);
bool shouldPauseDedicatedWorkerOnStartImpl(InstrumentingAgents*);
GeolocationPosition* overrideGeolocationPositionImpl(InstrumentingAgents*, GeolocationPosition*);
DeviceOrientationData* overrideDeviceOrientationImpl(InstrumentingAgents*, DeviceOrientationData*);
String getCurrentUserInitiatedProfileNameImpl(InstrumentingAgents*, bool incrementProfileNumber);

inline bool forcePseudoState(Element* element, CSSSelector::PseudoType pseudoState)
{
    FAST_RETURN_IF_NO_FRONTENDS(false);
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForElement(element))
        return forcePseudoStateImpl(instrumentingAgents, element, pseudoState);
    return false;
}

inline bool shouldApplyScreenWidthOverride(Frame* frame)
{
    FAST_RETURN_IF_NO_FRONTENDS(false);
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        return shouldApplyScreenWidthOverrideImpl(instrumentingAgents);
    return false;
}

inline bool shouldApplyScreenHeightOverride(Frame* frame)
{
    FAST_RETURN_IF_NO_FRONTENDS(false);
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        return shouldApplyScreenHeightOverrideImpl(instrumentingAgents);
    return false;
}

inline bool shouldPauseDedicatedWorkerOnStart(ScriptExecutionContext* context)
{
    FAST_RETURN_IF_NO_FRONTENDS(false);
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        return shouldPauseDedicatedWorkerOnStartImpl(instrumentingAgents);
    return false;
}

inline GeolocationPosition* overrideGeolocationPosition(Page* page, GeolocationPosition* position)
{
    FAST_RETURN_IF_NO_FRONTENDS(position);
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForPage(page))
        return overrideGeolocationPositionImpl(instrumentingAgents, position);
    return position;
}

inline DeviceOrientationData* overrideDeviceOrientation(Page* page, DeviceOrientationData* deviceOrientation)
{
    FAST_RETURN_IF_NO_FRONTENDS(deviceOrientation);
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForPage(page))
        return overrideDeviceOrientationImpl(instrumentingAgents, deviceOrientation);
    return deviceOrientation;
}

inline String getCurrentUserInitiatedProfileName(Page* page, bool incrementProfileNumber)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForPage(page))
        return getCurrentUserInitiatedProfileNameImpl(instrumentingAgents, incrementProfileNumber);
    return "";
}

}  // namespace InspectorInstrumentation

}  // namespace WebCore

#endif  // !defined(InspectorOverrides_inl_h)
