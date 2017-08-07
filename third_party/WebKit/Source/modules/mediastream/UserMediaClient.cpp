/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#include "modules/mediastream/UserMediaClient.h"

#include "modules/mediastream/UserMediaRequest.h"
#include "platform/wtf/RefPtr.h"
#include "public/web/WebFrameClient.h"
#include "public/web/WebMediaDeviceChangeObserver.h"
#include "public/web/WebMediaDevicesRequest.h"
#include "public/web/WebUserMediaClient.h"
#include "public/web/WebUserMediaRequest.h"

namespace blink {

UserMediaClient::UserMediaClient(WebUserMediaClient* client)
    : client_(client) {}

void UserMediaClient::RequestUserMedia(UserMediaRequest* request) {
  if (client_)
    client_->RequestUserMedia(request);
  else
    request->FailUASpecific("NotSupportedError", String(), String());
}

void UserMediaClient::CancelUserMediaRequest(UserMediaRequest* request) {
  if (client_)
    client_->CancelUserMediaRequest(WebUserMediaRequest(request));
}

void UserMediaClient::RequestMediaDevices(MediaDevicesRequest* request) {
  if (client_)
    client_->RequestMediaDevices(request);
}

void UserMediaClient::SetMediaDeviceChangeObserver(MediaDevices* observer) {
  if (client_) {
    client_->SetMediaDeviceChangeObserver(
        WebMediaDeviceChangeObserver(observer));
  }
}

}  // namespace blink
