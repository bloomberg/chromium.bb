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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef UserMediaController_h
#define UserMediaController_h

#include <memory>

#include "core/frame/LocalFrame.h"
#include "modules/mediastream/UserMediaClient.h"

namespace blink {

class ApplyConstraintsRequest;
class MediaDevices;
class MediaDevicesRequest;
class UserMediaRequest;

class UserMediaController final
    : public GarbageCollectedFinalized<UserMediaController>,
      public Supplement<LocalFrame> {
  USING_GARBAGE_COLLECTED_MIXIN(UserMediaController);

 public:
  UserMediaController(LocalFrame&, std::unique_ptr<UserMediaClient>);
  virtual void Trace(blink::Visitor*);

  UserMediaClient* Client() const { return client_.get(); }

  void RequestUserMedia(UserMediaRequest*);
  void CancelUserMediaRequest(UserMediaRequest*);
  void RequestMediaDevices(MediaDevicesRequest*);
  void SetMediaDeviceChangeObserver(MediaDevices*);
  void ApplyConstraints(ApplyConstraintsRequest*);

  static const char* SupplementName();
  static UserMediaController* From(LocalFrame* frame) {
    return static_cast<UserMediaController*>(
        Supplement<LocalFrame>::From(frame, SupplementName()));
  }

 private:
  std::unique_ptr<UserMediaClient> client_;
};

inline void UserMediaController::RequestUserMedia(UserMediaRequest* request) {
  client_->RequestUserMedia(request);
}

inline void UserMediaController::CancelUserMediaRequest(
    UserMediaRequest* request) {
  client_->CancelUserMediaRequest(request);
}

inline void UserMediaController::RequestMediaDevices(
    MediaDevicesRequest* request) {
  client_->RequestMediaDevices(request);
}

inline void UserMediaController::SetMediaDeviceChangeObserver(
    MediaDevices* observer) {
  client_->SetMediaDeviceChangeObserver(observer);
}

inline void UserMediaController::ApplyConstraints(
    ApplyConstraintsRequest* request) {
  client_->ApplyConstraints(request);
}

}  // namespace blink

#endif  // UserMediaController_h
