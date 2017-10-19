/*
 * Copyright (C) 2013 Google Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/html/parser/HTMLResourcePreloader.h"

#include "core/dom/Document.h"
#include "core/frame/Deprecation.h"
#include "core/frame/Settings.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/resource/CSSStyleSheetResource.h"
#include "platform/loader/fetch/Resource.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include <memory>

namespace blink {

HTMLResourcePreloader::HTMLResourcePreloader(Document& document)
    : document_(document) {}

HTMLResourcePreloader* HTMLResourcePreloader::Create(Document& document) {
  return new HTMLResourcePreloader(document);
}

void HTMLResourcePreloader::Trace(blink::Visitor* visitor) {
  visitor->Trace(document_);
  visitor->Trace(css_preloaders_);
}

int HTMLResourcePreloader::CountPreloads() {
  if (document_->Loader())
    return document_->Loader()->Fetcher()->CountPreloads();
  return 0;
}

static void PreconnectHost(
    PreloadRequest* request,
    const NetworkHintsInterface& network_hints_interface) {
  DCHECK(request);
  DCHECK(request->IsPreconnect());
  KURL host(request->BaseURL(), request->ResourceURL());
  if (!host.IsValid() || !host.ProtocolIsInHTTPFamily())
    return;
  network_hints_interface.PreconnectHost(host, request->CrossOrigin());
}

void HTMLResourcePreloader::Preload(
    std::unique_ptr<PreloadRequest> preload,
    const NetworkHintsInterface& network_hints_interface) {
  if (preload->IsPreconnect()) {
    PreconnectHost(preload.get(), network_hints_interface);
    return;
  }
  // TODO(yoichio): Should preload if document is imported.
  if (!document_->Loader())
    return;

  Resource* resource = preload->Start(document_);

  if (resource && !resource->IsLoaded() &&
      preload->ResourceType() == Resource::kCSSStyleSheet) {
    Settings* settings = document_->GetSettings();
    if (settings && (settings->GetCSSExternalScannerNoPreload() ||
                     settings->GetCSSExternalScannerPreload()))
      css_preloaders_.insert(new CSSPreloaderResourceClient(resource, this));
  }
}

}  // namespace blink
