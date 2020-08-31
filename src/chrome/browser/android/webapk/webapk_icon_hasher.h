// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_ICON_HASHER_H_
#define CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_ICON_HASHER_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/timer/timer.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace network {
class SimpleURLLoader;
namespace mojom {
class URLLoaderFactory;
}  // namespace mojom
}  // namespace network

// Downloads an icon and takes a Murmur2 hash of the downloaded image.
class WebApkIconHasher {
 public:
  // Result struct for holding the downloaded icon data and its hash.
  struct Icon {
    // The result of fetching the |icon|. This is untrusted data from the web
    // and should not be processed or decoded by the browser process.
    std::string unsafe_data;

    // The murmur2 hash of |unsafe_data|.
    std::string hash;
  };

  using Murmur2HashCallback = base::OnceCallback<void(Icon)>;
  using Murmur2HashMultipleCallback =
      base::OnceCallback<void(base::Optional<std::map<std::string, Icon>>)>;

  // Creates a self-owned WebApkIconHasher instance. The instance downloads all
  // the |icon_urls| and calls |callback| with the Murmur2 hash of the
  // downloaded images. The hash is taken over the raw image bytes (no image
  // encoding/decoding beforehand). |callback| is called with a base::nullopt if
  // any image cannot not be downloaded in time (e.g. 404 HTTP error code).
  static void DownloadAndComputeMurmur2Hash(
      network::mojom::URLLoaderFactory* url_loader_factory,
      const url::Origin& request_initiator,
      const std::set<GURL>& icon_urls,
      Murmur2HashMultipleCallback callback);

  static void DownloadAndComputeMurmur2HashWithTimeout(
      network::mojom::URLLoaderFactory* url_loader_factory,
      const url::Origin& request_initiator,
      const GURL& icon_url,
      int timeout_ms,
      Murmur2HashCallback callback);

 private:
  WebApkIconHasher(network::mojom::URLLoaderFactory* url_loader_factory,
                   const url::Origin& request_initiator,
                   const GURL& icon_url,
                   int timeout_ms,
                   Murmur2HashCallback callback);
  ~WebApkIconHasher();

  void OnSimpleLoaderComplete(std::unique_ptr<std::string> response_body);

  // Called if downloading the icon takes too long.
  void OnDownloadTimedOut();

  // Calls |callback_| with |icon_murmur2_hash|. Also deletes the instance.
  void RunCallback(Icon icon_murmur2_hash);

  // Called with the image hash.
  Murmur2HashCallback callback_;

  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;

  // Fails WebApkIconHasher if the download takes too long.
  base::OneShotTimer download_timeout_timer_;

  DISALLOW_COPY_AND_ASSIGN(WebApkIconHasher);
};

#endif  // CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_ICON_HASHER_H_
