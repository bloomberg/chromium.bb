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

#ifndef UserMediaRequest_h
#define UserMediaRequest_h

#include "core/dom/SuspendableObject.h"
#include "modules/ModulesExport.h"
#include "modules/mediastream/NavigatorUserMediaErrorCallback.h"
#include "modules/mediastream/NavigatorUserMediaSuccessCallback.h"
#include "platform/mediastream/MediaStreamSource.h"
#include "platform/wtf/Forward.h"
#include "public/platform/WebMediaConstraints.h"

namespace blink {

class Document;
class MediaErrorState;
class MediaStreamConstraints;
class MediaStreamDescriptor;
class UserMediaController;

class MODULES_EXPORT UserMediaRequest final
    : public GarbageCollectedFinalized<UserMediaRequest>,
      public ContextLifecycleObserver {
  USING_GARBAGE_COLLECTED_MIXIN(UserMediaRequest);

 public:
  static UserMediaRequest* Create(ExecutionContext*,
                                  UserMediaController*,
                                  const MediaStreamConstraints& options,
                                  NavigatorUserMediaSuccessCallback*,
                                  NavigatorUserMediaErrorCallback*,
                                  MediaErrorState&);
  static UserMediaRequest* CreateForTesting(const WebMediaConstraints& audio,
                                            const WebMediaConstraints& video);
  virtual ~UserMediaRequest();

  NavigatorUserMediaSuccessCallback* SuccessCallback() const {
    return success_callback_.Get();
  }
  NavigatorUserMediaErrorCallback* ErrorCallback() const {
    return error_callback_.Get();
  }
  Document* OwnerDocument();

  void Start();

  void Succeed(MediaStreamDescriptor*);
  void FailPermissionDenied(const String& message);
  void FailConstraint(const String& constraint_name, const String& message);
  void FailUASpecific(const String& name,
                      const String& message,
                      const String& constraint_name);

  bool Audio() const;
  bool Video() const;
  WebMediaConstraints AudioConstraints() const;
  WebMediaConstraints VideoConstraints() const;

  // errorMessage is only set if requestIsPrivilegedContext() returns |false|.
  // Caller is responsible for properly setting errors and canceling request.
  bool IsSecureContextUse(String& error_message);

  // ContextLifecycleObserver
  void ContextDestroyed(ExecutionContext*) override;

  virtual void Trace(blink::Visitor*);

 private:
  UserMediaRequest(ExecutionContext*,
                   UserMediaController*,
                   WebMediaConstraints audio,
                   WebMediaConstraints video,
                   NavigatorUserMediaSuccessCallback*,
                   NavigatorUserMediaErrorCallback*);

  WebMediaConstraints audio_;
  WebMediaConstraints video_;

  Member<UserMediaController> controller_;

  Member<NavigatorUserMediaSuccessCallback> success_callback_;
  Member<NavigatorUserMediaErrorCallback> error_callback_;
};

}  // namespace blink

#endif  // UserMediaRequest_h
