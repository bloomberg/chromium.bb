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
 *
 */

#ifndef LinkLoader_h
#define LinkLoader_h

#include "core/CoreExport.h"
#include "core/loader/LinkLoaderClient.h"
#include "platform/CrossOriginAttributeValue.h"
#include "platform/PrerenderClient.h"
#include "platform/loader/fetch/ResourceOwner.h"
#include "platform/wtf/Optional.h"

namespace blink {

class Document;
class LinkRelAttribute;
class LocalFrame;
class NetworkHintsInterface;
class PrerenderHandle;
struct ViewportDescriptionWrapper;

// The LinkLoader can load link rel types icon, dns-prefetch, prefetch, and
// prerender.
class CORE_EXPORT LinkLoader final
    : public GarbageCollectedFinalized<LinkLoader>,
      public PrerenderClient {
  USING_GARBAGE_COLLECTED_MIXIN(LinkLoader);

 public:
  static LinkLoader* Create(LinkLoaderClient* client) {
    return new LinkLoader(client, client->GetLoadingTaskRunner());
  }
  ~LinkLoader() override;

  // from PrerenderClient
  void DidStartPrerender() override;
  void DidStopPrerender() override;
  void DidSendLoadForPrerender() override;
  void DidSendDOMContentLoadedForPrerender() override;

  void Abort();
  bool LoadLink(const LinkRelAttribute&,
                CrossOriginAttributeValue,
                const String& type,
                const String& as,
                const String& media,
                ReferrerPolicy,
                const KURL&,
                Document&,
                const NetworkHintsInterface&);
  enum CanLoadResources {
    kOnlyLoadResources,
    kDoNotLoadResources,
    kLoadResourcesAndPreconnect
  };
  // Media links cannot be preloaded until the first chunk is parsed. The rest
  // can be preloaded at commit time.
  enum MediaPreloadPolicy { kLoadAll, kOnlyLoadNonMedia, kOnlyLoadMedia };
  static void LoadLinksFromHeader(const String& header_value,
                                  const KURL& base_url,
                                  LocalFrame&,
                                  Document*,  // can be nullptr
                                  const NetworkHintsInterface&,
                                  CanLoadResources,
                                  MediaPreloadPolicy,
                                  ViewportDescriptionWrapper*);
  static WTF::Optional<Resource::Type> GetResourceTypeFromAsAttribute(
      const String& as);

  Resource* GetResourceForTesting();

  DECLARE_TRACE();

 private:
  class FinishObserver;
  LinkLoader(LinkLoaderClient*, RefPtr<WebTaskRunner>);

  void NotifyFinished();

  Member<FinishObserver> finish_observer_;
  Member<LinkLoaderClient> client_;

  Member<PrerenderHandle> prerender_;
};

}  // namespace blink

#endif
