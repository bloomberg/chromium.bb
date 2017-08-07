/*
 * Copyright (C) 2011 Ericsson AB. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Ericsson nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
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

#ifndef UserMediaClient_h
#define UserMediaClient_h

#include "modules/ModulesExport.h"
#include "modules/mediastream/UserMediaClient.h"
#include "platform/wtf/PassRefPtr.h"
#include "platform/wtf/PtrUtil.h"

namespace blink {

class LocalFrame;
class MediaDevices;
class MediaDevicesRequest;
class UserMediaRequest;
class WebUserMediaClient;

class MODULES_EXPORT UserMediaClient {
 public:
  static std::unique_ptr<UserMediaClient> Create(WebUserMediaClient* client) {
    return WTF::WrapUnique(new UserMediaClient(client));
  }

  void RequestUserMedia(UserMediaRequest*);
  void CancelUserMediaRequest(UserMediaRequest*);
  void RequestMediaDevices(MediaDevicesRequest*);
  void SetMediaDeviceChangeObserver(MediaDevices*);

 private:
  explicit UserMediaClient(WebUserMediaClient*);

  WebUserMediaClient* client_;
};

MODULES_EXPORT void ProvideUserMediaTo(LocalFrame&,
                                       std::unique_ptr<UserMediaClient>);

}  // namespace blink

#endif  // UserMediaClient_h
