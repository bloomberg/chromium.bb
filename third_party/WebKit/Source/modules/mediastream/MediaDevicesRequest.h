/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GOOGLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef MediaDevicesRequest_h
#define MediaDevicesRequest_h

#include "bindings/core/v8/ScriptPromise.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "modules/ModulesExport.h"
#include "modules/mediastream/MediaDeviceInfo.h"
#include "platform/heap/Handle.h"

namespace blink {

class Document;
class UserMediaController;
class ScriptState;
class ScriptPromiseResolver;

class MODULES_EXPORT MediaDevicesRequest final
    : public GarbageCollectedFinalized<MediaDevicesRequest>,
      public ContextLifecycleObserver {
  USING_GARBAGE_COLLECTED_MIXIN(MediaDevicesRequest);

 public:
  static MediaDevicesRequest* Create(ScriptState*, UserMediaController*);
  virtual ~MediaDevicesRequest();

  Document* OwnerDocument();

  ScriptPromise Start();

  void Succeed(const MediaDeviceInfoVector&);

  // ContextLifecycleObserver
  void ContextDestroyed(ExecutionContext*) override;

  virtual void Trace(blink::Visitor*);

 private:
  MediaDevicesRequest(ScriptState*, UserMediaController*);

  Member<UserMediaController> controller_;
  Member<ScriptPromiseResolver> resolver_;
};

}  // namespace blink

#endif  // MediaDevicesRequest_h
