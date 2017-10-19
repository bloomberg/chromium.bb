/*
 * Copyright (C) 2012 Motorola Mobility Inc.
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

#ifndef PublicURLManager_h
#define PublicURLManager_h

#include "core/dom/ContextLifecycleObserver.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class KURL;
class ExecutionContext;
class URLRegistry;
class URLRegistrable;

class PublicURLManager final
    : public GarbageCollectedFinalized<PublicURLManager>,
      public ContextLifecycleObserver {
  USING_GARBAGE_COLLECTED_MIXIN(PublicURLManager);

 public:
  static PublicURLManager* Create(ExecutionContext*);

  // Generates a new Blob URL and registers the URLRegistrable to the
  // corresponding URLRegistry with the Blob URL. Returns the serialization
  // of the Blob URL.
  //
  // |uuid| can be used for revoke() to revoke all URLs associated with the
  // |uuid|. It's not the UUID generated and appended to the BlobURL, but an
  // identifier for the object to which URL(s) are generated e.g. ones
  // returned by blink::Blob::uuid().
  String RegisterURL(ExecutionContext*, URLRegistrable*, const String& uuid);
  // Revokes the given URL.
  void Revoke(const KURL&);
  // Revokes all URLs associated with |uuid|.
  void Revoke(const String& uuid);

  // ContextLifecycleObserver interface.
  void ContextDestroyed(ExecutionContext*) override;

  virtual void Trace(blink::Visitor*);

 private:
  explicit PublicURLManager(ExecutionContext*);

  // One or more URLs can be associated with the same unique ID.
  // Objects need be revoked by unique ID in some cases.
  typedef String URLString;
  typedef HashMap<URLString, String> URLMap;
  // Map from URLRegistry instances to the maps which store association
  // between URLs registered with the URLRegistry and UUIDs assigned for
  // each of the URLs.
  typedef HashMap<URLRegistry*, URLMap> RegistryURLMap;

  RegistryURLMap registry_to_url_;
  bool is_stopped_;
};

}  // namespace blink

#endif  // PublicURLManager_h
