/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
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

#ifndef UserMediaController_h
#define UserMediaController_h

#include "core/page/Page.h"
#include "modules/mediastream/UserMediaClient.h"
#include "wtf/PassOwnPtr.h"

namespace WebCore {

class MediaDevicesRequest;
class UserMediaRequest;

class UserMediaController FINAL : public Supplement<Page> {
public:
    virtual ~UserMediaController();

    UserMediaClient* client() const { return m_client; }

    void requestUserMedia(PassRefPtr<UserMediaRequest>);
    void cancelUserMediaRequest(UserMediaRequest*);

    void requestMediaDevices(PassRefPtr<MediaDevicesRequest>);
    void cancelMediaDevicesRequest(MediaDevicesRequest*);

    static PassOwnPtr<UserMediaController> create(UserMediaClient*);
    static const char* supplementName();
    static UserMediaController* from(Page* page) { return static_cast<UserMediaController*>(Supplement<Page>::from(page, supplementName())); }

    virtual void trace(Visitor*) OVERRIDE { }

protected:
    explicit UserMediaController(UserMediaClient*);

private:
    UserMediaClient* m_client;
};

inline void UserMediaController::requestUserMedia(PassRefPtr<UserMediaRequest> request)
{
    m_client->requestUserMedia(request);
}

inline void UserMediaController::cancelUserMediaRequest(UserMediaRequest* request)
{
    m_client->cancelUserMediaRequest(request);
}

inline void UserMediaController::requestMediaDevices(PassRefPtr<MediaDevicesRequest> request)
{
    m_client->requestMediaDevices(request);
}

inline void UserMediaController::cancelMediaDevicesRequest(MediaDevicesRequest* request)
{
    m_client->cancelMediaDevicesRequest(request);
}

} // namespace WebCore

#endif // UserMediaController_h
