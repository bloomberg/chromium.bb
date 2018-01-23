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
#include "platform/wtf/HashSet.h"
#include "platform/wtf/text/WTFString.h"
#include "third_party/WebKit/common/blob/blob_url_store.mojom-blink.h"

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
  String RegisterURL(URLRegistrable*);
  // Revokes the given URL.
  void Revoke(const KURL&);

  // ContextLifecycleObserver interface.
  void ContextDestroyed(ExecutionContext*) override;

  virtual void Trace(blink::Visitor*);

 private:
  explicit PublicURLManager(ExecutionContext*);

  typedef String URLString;
  // Map from URLs to the URLRegistry they are registered with.
  typedef HashMap<URLString, URLRegistry*> URLToRegistryMap;
  URLToRegistryMap url_to_registry_;

  bool is_stopped_;

  mojom::blink::BlobURLStoreAssociatedPtr url_store_;
};

}  // namespace blink

#endif  // PublicURLManager_h
