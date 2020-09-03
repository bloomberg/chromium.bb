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
#include <blpwtk2_config.h>
#include <content/renderer/loader/resource_loader_bridge.h>

namespace content {
struct RequestInfo;
}  // namespace content

namespace network {
struct ResourceRequest;
}  // namespace network

namespace blpwtk2 {

class InProcessResourceLoaderBridge : public content::ResourceLoaderBridge {
 public:
  InProcessResourceLoaderBridge(
      const content::ResourceRequestInfoProvider& request_info_provider);
  ~InProcessResourceLoaderBridge() final;

  // Override for blink::WebNavigationBodyLoader
  // While deferred, no more data will be read and no notifications
  // will be called on the client. This method can be called
  // multiples times, at any moment.
  void SetDefersLoading(bool defers) override;

  // Starts loading the body. Client must be non-null, and will receive
  // the body, code cache and final result.
  void StartLoadingBody(blink::WebNavigationBodyLoader::Client* client,
                        bool use_isolated_code_cache) override;

  // Override of the rest functions in content::ResourceLoaderBridge
  void Start(std::unique_ptr<content::ResourceReceiver> receiver) override;
  void Cancel() override;
  void SyncLoad(content::SyncLoadResponse* response) override;

 private:
  class InProcessURLRequest;
  class InProcessResourceContext;
  scoped_refptr<InProcessResourceContext> d_context;

  DISALLOW_COPY_AND_ASSIGN(InProcessResourceLoaderBridge);
};

}  // namespace blpwtk2

#endif  // INCLUDED_BLPWTK2_INPROCESSRESOURCELOADERBRIDGE_H
