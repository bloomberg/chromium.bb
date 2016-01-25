/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "public/web/WebGeolocationController.h"

#include "modules/geolocation/GeolocationController.h"
#include "modules/geolocation/GeolocationError.h"
#include "modules/geolocation/GeolocationPosition.h"
#include "platform/heap/Handle.h"
#include "public/web/WebGeolocationError.h"
#include "public/web/WebGeolocationPosition.h"
#include "wtf/RefCounted.h"

namespace blink {

// TODO(Oilpan): once GeolocationController is always on the heap,
// shorten out this GeolocationControllerPrivate intermediary.
class GeolocationControllerPrivate final : public RefCountedWillBeGarbageCollected<GeolocationControllerPrivate> {
public:
    static PassRefPtrWillBeRawPtr<GeolocationControllerPrivate> create(GeolocationController* controller)
    {
        return adoptRefWillBeNoop(new GeolocationControllerPrivate(controller));
    }

    static GeolocationController& controller(const WebPrivatePtr<GeolocationControllerPrivate>& controller)
    {
        ASSERT(!controller.isNull());
        ASSERT(controller->m_controller);
        return *controller->m_controller;
    }

    DEFINE_INLINE_TRACE()
    {
        visitor->trace(m_controller);
    }

private:
    explicit GeolocationControllerPrivate(GeolocationController* controller)
        : m_controller(controller)
    {
    }

    // Non-Oilpan, this bare pointer is owned as a supplement and kept alive
    // by the frame of the WebLocalFrame which creates the WebGeolocationController
    // object that wraps it all up.
    RawPtrWillBeMember<GeolocationController> m_controller;
};

WebGeolocationController::WebGeolocationController(GeolocationController* controller)
    : m_private(GeolocationControllerPrivate::create(controller))
{
}

void WebGeolocationController::reset()
{
    m_private.reset();
}

void WebGeolocationController::positionChanged(const WebGeolocationPosition& webPosition)
{
    GeolocationControllerPrivate::controller(m_private).positionChanged(static_cast<GeolocationPosition*>(webPosition));
}

void WebGeolocationController::errorOccurred(const WebGeolocationError& webError)
{
    GeolocationControllerPrivate::controller(m_private).errorOccurred(static_cast<GeolocationError*>(webError));
}

} // namespace blink
