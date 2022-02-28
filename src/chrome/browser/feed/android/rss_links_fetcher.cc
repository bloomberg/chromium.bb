// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feed/android/rss_links_fetcher.h"

#include "base/callback.h"
#include "chrome/browser/android/tab_android.h"
#include "components/feed/mojom/rss_link_reader.mojom.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace feed {
namespace {

service_manager::InterfaceProvider* GetRenderFrameRemoteInterfaces(
    TabAndroid* tab) {
  if (!tab || !tab->web_contents() || !tab->web_contents()->GetMainFrame())
    return nullptr;

  return tab->web_contents()->GetMainFrame()->GetRemoteInterfaces();
}

mojo::Remote<feed::mojom::RssLinkReader> GetRssLinkReaderRemote(
    TabAndroid* tab) {
  mojo::Remote<feed::mojom::RssLinkReader> result;
  service_manager::InterfaceProvider* interface_provider =
      GetRenderFrameRemoteInterfaces(tab);
  if (interface_provider) {
    interface_provider->GetInterface(result.BindNewPipeAndPassReceiver());
  }
  return result;
}

class RssLinksFetcher {
 public:
  void Start(const GURL& page_url,
             mojo::Remote<feed::mojom::RssLinkReader> link_reader,
             base::OnceCallback<void(std::vector<GURL>)> callback) {
    page_url_ = page_url;
    callback_ = std::move(callback);
    link_reader_ = std::move(link_reader);
    if (link_reader_) {
      // Unretained is OK here. The `mojo::Remote` will not invoke callbacks
      // after it is destroyed.
      link_reader_.set_disconnect_handler(base::BindOnce(
          &RssLinksFetcher::SendResultAndDeleteSelf, base::Unretained(this)));
      link_reader_->GetRssLinks(base::BindOnce(
          &RssLinksFetcher::GetRssLinksComplete, base::Unretained(this)));
      return;
    }
    SendResultAndDeleteSelf();
  }

 private:
  void GetRssLinksComplete(feed::mojom::RssLinksPtr rss_links) {
    if (rss_links) {
      if (rss_links->page_url == page_url_) {
        for (GURL& link : rss_links->links) {
          if (link.is_valid() && link.SchemeIsHTTPOrHTTPS()) {
            result_.push_back(std::move(link));
          }
        }
      }
    }

    SendResultAndDeleteSelf();
  }
  void SendResultAndDeleteSelf() {
    std::move(callback_).Run(std::move(result_));
    delete this;
  }

  GURL page_url_;
  mojo::Remote<feed::mojom::RssLinkReader> link_reader_;
  std::vector<GURL> result_;
  base::OnceCallback<void(std::vector<GURL>)> callback_;
};

}  // namespace

void FetchRssLinks(const GURL& url,
                   mojo::Remote<feed::mojom::RssLinkReader> link_reader,
                   base::OnceCallback<void(std::vector<GURL>)> callback) {
  // RssLinksFetcher is self-deleting.
  auto* fetcher = new RssLinksFetcher();
  fetcher->Start(url, std::move(link_reader), std::move(callback));
}

void FetchRssLinks(const GURL& url,
                   TabAndroid* page_tab,
                   base::OnceCallback<void(std::vector<GURL>)> callback) {
  FetchRssLinks(url, GetRssLinkReaderRemote(page_tab), std::move(callback));
}

}  // namespace feed
