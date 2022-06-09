// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_AUCTION_DOWNLOADER_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_AUCTION_DOWNLOADER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace network {
class SimpleURLLoader;
}

namespace auction_worklet {

// Download utility for auction scripts and JSON data. Creates requests and
// blocks responses.
class AuctionDownloader {
 public:
  // Mime type to use for Accept header. Any response without a matching
  // Content-Type header is rejected.
  enum class MimeType {
    kJavascript,
    kJson,
    kWebAssembly,
  };

  // Passes in nullptr on failure. Always invoked asynchronously. Will not be
  // invoked after the AuctionDownloader is destroyed.
  using AuctionDownloaderCallback =
      base::OnceCallback<void(std::unique_ptr<std::string> response_body,
                              absl::optional<std::string> error)>;

  // Starts loading the worklet script on construction. Callback will be invoked
  // asynchronously once the data has been fetched or an error has occurred.
  AuctionDownloader(network::mojom::URLLoaderFactory* url_loader_factory,
                    const GURL& source_url,
                    MimeType mime_type,
                    AuctionDownloaderCallback auction_downloader_callback);
  explicit AuctionDownloader(const AuctionDownloader&) = delete;
  AuctionDownloader& operator=(const AuctionDownloader&) = delete;
  ~AuctionDownloader();

 private:
  void OnBodyReceived(std::unique_ptr<std::string> body);

  void OnRedirect(const net::RedirectInfo& redirect_info,
                  const network::mojom::URLResponseHead& response_head,
                  std::vector<std::string>* removed_headers);

  const GURL source_url_;
  const MimeType mime_type_;
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;
  AuctionDownloaderCallback auction_downloader_callback_;
};

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_AUCTION_DOWNLOADER_H_
