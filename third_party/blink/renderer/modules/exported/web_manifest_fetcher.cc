// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/web_manifest_fetcher.h"

#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/modules/manifest/manifest_fetcher.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

WebManifestFetcher::WebManifestFetcher(const WebURL& url)
    : private_(MakeGarbageCollected<ManifestFetcher>(url)) {}

void WebManifestFetcher::Start(WebDocument* web_document,
                               bool use_credentials,
                               Callback callback) {
  DCHECK(!IsNull());

  Document* document = web_document->Unwrap<Document>();
  private_->Start(*document, use_credentials, std::move(callback));
}

void WebManifestFetcher::Cancel() {
  DCHECK(!IsNull());
  private_->Cancel();
}

void WebManifestFetcher::Reset() {
  private_.Reset();
}

}  // namespace blink
