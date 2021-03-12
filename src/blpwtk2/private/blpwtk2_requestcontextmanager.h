/*
 * Copyright (C) 2019 Bloomberg Finance L.P.
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

#ifndef INCLUDED_BLPWTK2_REQUESTCONTEXTMANAGER_H
#define INCLUDED_BLPWTK2_REQUESTCONTEXTMANAGER_H

#include <base/files/file_path.h>
#include <base/macros.h>
#include <base/memory/ref_counted.h>
#include <content/public/browser/browser_context.h>
#include <mojo/public/cpp/bindings/pending_remote.h>
#include <mojo/public/cpp/bindings/remote.h>
#include <services/network/public/mojom/network_context.mojom.h>
#include <services/network/public/mojom/network_service.mojom.h>

#include <string>

namespace content {
class ResourceContext;
}

namespace blpwtk2 {

class BrowserContextOptions;
class ProxyConfigMonitor;

class RequestContextManager {
 public:
  static std::unique_ptr<RequestContextManager> CreateSystemContext();

  RequestContextManager();
  ~RequestContextManager();

  void ConfigureNetworkContextParams(
    std::string user_agent,
    network::mojom::NetworkContextParams* network_context_params);

  content::ResourceContext* GetResourceContext()
  {
    return resource_context_.get();
  }

  // Set the custom proxy config to override the system proxy config
  void SetCustomProxyConfig(const net::ProxyConfig& custom_proxy_config);

 private:
  network::mojom::NetworkContextParamsPtr CreateNetworkContextParams(std::string user_agent);
  std::unique_ptr<net::ProxyConfig> proxy_config_;
  std::unique_ptr<ProxyConfigMonitor> proxy_config_monitor_;

  mojo::PendingRemote<::network::mojom::NetworkContext> system_context_;
  std::unique_ptr<content::ResourceContext> resource_context_;

  DISALLOW_COPY_AND_ASSIGN(RequestContextManager);
};

}  // namespace blpwtk2

#endif  // INCLUDED_BLPWTK2_REQUESTCONTEXTMANAGER_H
