// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_MANIFEST_FETCHER_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_MANIFEST_FETCHER_H_

#include <memory>
#include "base/callback.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_private_ptr.h"

namespace blink {

class ManifestFetcher;
class WebDocument;
class WebURL;
class WebURLResponse;
class WebString;

class WebManifestFetcher {
 public:
  // This will be called asynchronously after the URL has been fetched,
  // successfully or not.  If there is a failure, response and data will both be
  // empty.  |response| and |data| are both valid until the ManifestFetcher
  // instance is destroyed.
  using Callback =
      base::OnceCallback<void(const WebURLResponse&, const WebString&)>;

  BLINK_EXPORT explicit WebManifestFetcher(const WebURL& url);
  BLINK_EXPORT ~WebManifestFetcher() { Reset(); }

  BLINK_EXPORT void Reset();

  BLINK_EXPORT void Start(WebDocument* web_document,
                          bool use_credentials,
                          Callback callback);
  BLINK_EXPORT void Cancel();

 private:
  bool IsNull() const { return private_.IsNull(); }

  WebPrivatePtr<ManifestFetcher> private_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_MANIFEST_FETCHER_H_
