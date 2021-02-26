// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_CODE_CACHE_LOADER_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_CODE_CACHE_LOADER_H_

#include "base/callback.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "third_party/blink/public/mojom/loader/code_cache.mojom-shared.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "url/gurl.h"

namespace blink {

// WebCodeCacheLoader is an abstract class that provides the interface for
// fetching the data from code cache.
class BLINK_PLATFORM_EXPORT WebCodeCacheLoader {
 public:
  using FetchCodeCacheCallback =
      base::OnceCallback<void(base::Time, mojo_base::BigBuffer)>;
  virtual ~WebCodeCacheLoader() = default;

  static std::unique_ptr<WebCodeCacheLoader> Create(
      base::WaitableEvent* terminate_sync_load_event = nullptr);

  // Fetched code cache corresponding to |url| synchronously and returns
  // response in |response_time_out| and |data_out|. |response_time_out| and
  // |data_out| cannot be nullptrs.
  virtual void FetchFromCodeCacheSynchronously(
      const WebURL& url,
      base::Time* response_time_out,
      mojo_base::BigBuffer* data_out) = 0;
  virtual void FetchFromCodeCache(blink::mojom::CodeCacheType cache_type,
                                  const WebURL& url,
                                  FetchCodeCacheCallback) = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_CODE_CACHE_LOADER_H_
