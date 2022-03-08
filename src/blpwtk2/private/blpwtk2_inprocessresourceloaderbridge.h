/*
 * Copyright (C) 2014 Bloomberg Finance L.P.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS," WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef INCLUDED_BLPWTK2_INPROCESSRESOURCELOADERBRIDGE_H
#define INCLUDED_BLPWTK2_INPROCESSRESOURCELOADERBRIDGE_H

#include <base/memory/scoped_refptr.h>
#include <third_party/blink/public/platform/web_resource_request_sender_delegate.h>
#include <memory>


namespace blpwtk2 {

class InProcessResourceContext;

class InProcessResourceLoaderBridge final : public blink::WebResourceRequestSenderDelegate {
 public:
  InProcessResourceLoaderBridge();
  ~InProcessResourceLoaderBridge() override;
  InProcessResourceLoaderBridge(const InProcessResourceLoaderBridge&) = delete;
  InProcessResourceLoaderBridge& operator=(const InProcessResourceLoaderBridge&) = delete;

  void OnRequestComplete() override;

  scoped_refptr<blink::WebRequestPeer> OnReceivedResponse(
      scoped_refptr<blink::WebRequestPeer> current_peer,
      const blink::WebString& mime_type,
      const blink::WebURL& url) override;

  bool CanHandleURL(const std::string& url) override;

  std::unique_ptr<blink::WebNavigationBodyLoader>
      CreateBodyLoader(network::ResourceRequest* request) override;

  void Start(blink::WebRequestPeer* peer,
             blink::WebResourceRequestSender* sender,
             network::ResourceRequest* request,
             int request_id,
             scoped_refptr<base::SingleThreadTaskRunner> runner) override;

  void Cancel(int request_id) override;

 private:
  std::unordered_map<int,scoped_refptr<InProcessResourceContext>> d_contexts;
};

}  // namespace blpwtk2

#endif  // INCLUDED_BLPWTK2_INPROCESSRESOURCELOADERBRIDGE_H
