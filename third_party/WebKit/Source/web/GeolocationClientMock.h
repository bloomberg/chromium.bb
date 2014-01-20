/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2012 Apple Inc. All Rights Reserved.
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

#ifndef GeolocationClientMock_h
#define GeolocationClientMock_h

#include "modules/geolocation/GeolocationClient.h"
#include "platform/Timer.h"
#include "wtf/HashSet.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/text/WTFString.h"

namespace WebCore {
class GeolocationController;
class GeolocationPosition;
}

namespace blink {

// Provides a mock object for the geolocation client.
class GeolocationClientMock FINAL : public WebCore::GeolocationClient {
public:
    GeolocationClientMock();
    virtual ~GeolocationClientMock();

    void reset();
    void setController(WebCore::GeolocationController*);

    void setPosition(PassRefPtr<WebCore::GeolocationPosition>);
    void setPositionUnavailableError(const String& errorMessage);
    void setPermission(bool allowed);
    int numberOfPendingPermissionRequests() const;

    // GeolocationClient
    virtual void geolocationDestroyed() OVERRIDE;
    virtual void startUpdating() OVERRIDE;
    virtual void stopUpdating() OVERRIDE;
    virtual void setEnableHighAccuracy(bool) OVERRIDE;
    virtual WebCore::GeolocationPosition* lastPosition() OVERRIDE;
    virtual void requestPermission(WebCore::Geolocation*) OVERRIDE;
    virtual void cancelPermissionRequest(WebCore::Geolocation*) OVERRIDE;

private:
    void asyncUpdateController();
    void controllerTimerFired(WebCore::Timer<GeolocationClientMock>*);

    void asyncUpdatePermission();
    void permissionTimerFired(WebCore::Timer<GeolocationClientMock>*);

    void clearError();

    WebCore::GeolocationController* m_controller;
    RefPtr<WebCore::GeolocationPosition> m_lastPosition;
    bool m_hasError;
    String m_errorMessage;
    WebCore::Timer<GeolocationClientMock> m_controllerTimer;
    WebCore::Timer<GeolocationClientMock> m_permissionTimer;
    bool m_isActive;

    enum PermissionState {
        PermissionStateUnset,
        PermissionStateAllowed,
        PermissionStateDenied,
    } m_permissionState;
    typedef WTF::HashSet<RefPtr<WebCore::Geolocation> > GeolocationSet;
    GeolocationSet m_pendingPermission;
};

}

#endif
