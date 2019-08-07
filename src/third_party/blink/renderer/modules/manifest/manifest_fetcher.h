// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MANIFEST_MANIFEST_FETCHER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MANIFEST_MANIFEST_FETCHER_H_

#include <memory>

#include "base/callback.h"
#include "third_party/blink/public/web/web_manifest_fetcher.h"
#include "third_party/blink/renderer/core/loader/threadable_loader.h"
#include "third_party/blink/renderer/core/loader/threadable_loader_client.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

class Document;
class KURL;
class TextResourceDecoder;

// Helper class to download a Web Manifest. When an instance is created, the
// caller need to call Start() and wait for the passed callback to be executed.
// If the fetch fails, the callback will be called with two empty objects.
class ManifestFetcher final : public GarbageCollectedFinalized<ManifestFetcher>,
                              public ThreadableLoaderClient {
  USING_GARBAGE_COLLECTED_MIXIN(ManifestFetcher);

 public:
  explicit ManifestFetcher(const KURL& url);
  ~ManifestFetcher() override;

  void Start(Document& document,
             bool use_credentials,
             WebManifestFetcher::Callback callback);
  void Cancel();

  // ThreadableLoaderClient
  void DidReceiveResponse(uint64_t, const ResourceResponse&) override;
  void DidReceiveData(const char*, unsigned) override;
  void DidFinishLoading(uint64_t) override;
  void DidFail(const ResourceError&) override;
  void DidFailRedirectCheck() override;

  void Trace(Visitor* visitor) override;

 private:
  KURL url_;
  bool completed_;
  WebManifestFetcher::Callback callback_;
  ResourceResponse response_;
  std::unique_ptr<TextResourceDecoder> decoder_;
  StringBuilder data_;
  Member<ThreadableLoader> loader_;

  DISALLOW_COPY_AND_ASSIGN(ManifestFetcher);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MANIFEST_MANIFEST_FETCHER_H_
