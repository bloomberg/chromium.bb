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

#include <blpwtk2_requestcontextmanager.h>

#include <base/bind.h>
#include <base/task/post_task.h>
#include <base/threading/thread_task_runner_handle.h>
#include <content/public/browser/browser_task_traits.h>
#include <content/public/browser/cors_exempt_headers.h>
#include <content/public/browser/network_service_instance.h>
#include <content/public/browser/resource_context.h>
#include <mojo/public/cpp/bindings/receiver.h>
#include <net/http/http_auth_preferences.h>
#include <services/network/network_service.h>
#include <services/network/public/cpp/features.h>
#include <services/network/public/mojom/network_service.mojom.h>
#include <services/network/url_request_context_builder_mojo.h>

// reference: headless_request_context_manager.cc
namespace blpwtk2 {

namespace {

net::NetworkTrafficAnnotationTag GetProxyConfigTrafficAnnotationTag()
{
  static net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation(
        "085C6810_CE54_400B_A7E2_AEB8462A1D71", R"(
      semantics {
        sender: "Resource Loader"
        description:
          "initiated request, which includes all resources for "
          "normal page loads, chrome URLs, and downloads."
        trigger:
          "The user navigates to a URL or downloads a file. Also when a "
          "webpage, ServiceWorker, or chrome:// uses any network communication."
        data: "Anything the initiator wants to send."
        destination: OTHER
      }
      policy {
        cookies_allowed: YES
        cookies_store: "user"
        setting: "These requests cannot be disabled in settings."
        policy_exception_justification:
          "Not implemented. Without these requests, Chrome will be unable "
          "to load any webpage."
      })");
  return traffic_annotation;
}


}  // namespace

// Tracks the ProxyConfig to use, and passes any updates to a NetworkContext's
// ProxyConfigClient.
class ProxyConfigMonitor
    : public net::ProxyConfigService::Observer,
      public ::network::mojom::ProxyConfigPollerClient
{
 public:
  static void DeleteSoon(std::unique_ptr<ProxyConfigMonitor> instance)
  {
    instance->task_runner_->DeleteSoon(FROM_HERE, instance.release());
  }

  explicit ProxyConfigMonitor(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : task_runner_(task_runner)
      , use_custom_proxy_(false)
  {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    // We must create the proxy config service on the UI loop on Linux because
    // it must synchronously run on the glib message loop.
    proxy_config_service_ =
        net::ProxyResolutionService::CreateSystemProxyConfigService(
            task_runner_);
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&net::ProxyConfigService::AddObserver,
                                  base::Unretained(proxy_config_service_.get()),
                                  base::Unretained(this)));
  }

  ~ProxyConfigMonitor() override
  {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    proxy_config_service_->RemoveObserver(this);
  }

  void SetCustomProxyConfig(const net::ProxyConfig& custom_proxy_config)
  {
    if (!proxy_config_client_)
      return;

    use_custom_proxy_ = true;

    proxy_config_client_->OnProxyConfigUpdated(
      net::ProxyConfigWithAnnotation(custom_proxy_config,
                                     GetProxyConfigTrafficAnnotationTag()));
  }

  // Populates proxy-related fields of |network_context_params|. Updated
  // ProxyConfigs will be sent to a NetworkContext created with those params
  // whenever the configuration changes. Can be called more than once to inform
  // multiple NetworkContexts of proxy changes.
  void AddToNetworkContextParams(
      ::network::mojom::NetworkContextParams* network_context_params)
  {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    DCHECK(!proxy_config_client_);
    network_context_params->proxy_config_client_receiver =
        proxy_config_client_.BindNewPipeAndPassReceiver();
    poller_receiver_.Bind(network_context_params->proxy_config_poller_client
                              .InitWithNewPipeAndPassReceiver());
    net::ProxyConfigWithAnnotation proxy_config;
    net::ProxyConfigService::ConfigAvailability availability =
        proxy_config_service_->GetLatestProxyConfig(&proxy_config);
    if (availability != net::ProxyConfigService::CONFIG_PENDING)
      network_context_params->initial_proxy_config = proxy_config;
  }

 private:
  // net::ProxyConfigService::Observer implementation:
  void OnProxyConfigChanged(
      const net::ProxyConfigWithAnnotation& config,
      net::ProxyConfigService::ConfigAvailability availability) override
  {
    if (!proxy_config_client_)
      return;

    // ignore system config change when the custom proxy is used.
    if (use_custom_proxy_)
      return;

    switch (availability) {
      case net::ProxyConfigService::CONFIG_VALID:
        proxy_config_client_->OnProxyConfigUpdated(config);
        break;
      case net::ProxyConfigService::CONFIG_UNSET:
        proxy_config_client_->OnProxyConfigUpdated(
            net::ProxyConfigWithAnnotation::CreateDirect());
        break;
      case net::ProxyConfigService::CONFIG_PENDING:
        NOTREACHED();
        break;
    }
  }

  // network::mojom::ProxyConfigPollerClient implementation:
  void OnLazyProxyConfigPoll() override { proxy_config_service_->OnLazyPoll(); }

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  std::unique_ptr<net::ProxyConfigService> proxy_config_service_;
  mojo::Receiver<::network::mojom::ProxyConfigPollerClient> poller_receiver_{
      this};
  mojo::Remote<::network::mojom::ProxyConfigClient> proxy_config_client_;

  bool use_custom_proxy_;

  DISALLOW_COPY_AND_ASSIGN(ProxyConfigMonitor);
};

// static
std::unique_ptr<RequestContextManager>
RequestContextManager::CreateSystemContext()
{
  auto manager = std::make_unique<RequestContextManager>();

  auto* network_service = content::GetNetworkService();

  network_service->CreateNetworkContext(
      manager->system_context_.InitWithNewPipeAndPassReceiver(),
      manager->CreateNetworkContextParams(/* is_system = */ true, ""));

  return manager;
}

RequestContextManager::RequestContextManager()
    : proxy_config_(nullptr),
      resource_context_(std::make_unique<content::ResourceContext>())
{
  if (!proxy_config_) {
    proxy_config_monitor_ = std::make_unique<ProxyConfigMonitor>(
        base::ThreadTaskRunnerHandle::Get());
  }
}

RequestContextManager::~RequestContextManager()
{
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (proxy_config_monitor_)
    ProxyConfigMonitor::DeleteSoon(std::move(proxy_config_monitor_));
}

mojo::Remote<::network::mojom::NetworkContext>
RequestContextManager::CreateNetworkContext(
    bool in_memory,
    const base::FilePath& relative_partition_path,
    std::string user_agent)
{
  mojo::Remote<::network::mojom::NetworkContext> network_context;
  content::GetNetworkService()->CreateNetworkContext(
      network_context.BindNewPipeAndPassReceiver(),
      CreateNetworkContextParams(/* is_system = */ false, std::move(user_agent)));
  return network_context;
}

network::mojom::NetworkContextParamsPtr
RequestContextManager::CreateNetworkContextParams(bool is_system, std::string user_agent)
{
  auto context_params = ::network::mojom::NetworkContextParams::New();
  context_params->user_agent = std::move(user_agent);
  context_params->accept_language = "en-us,en";
  context_params->primary_network_context = is_system;

  // TODO(https://crbug.com/458508): Allow
  // context_params->http_auth_static_network_context_params->allow_default_credentials
  // to be controllable by a flag.
  context_params->http_auth_static_network_context_params =
      ::network::mojom::HttpAuthStaticNetworkContextParams::New();

   if (proxy_config_) {
    context_params->initial_proxy_config = net::ProxyConfigWithAnnotation(
        *proxy_config_, GetProxyConfigTrafficAnnotationTag());
  } else {
    proxy_config_monitor_->AddToNetworkContextParams(context_params.get());
  }
  content::UpdateCorsExemptHeader(context_params.get());
  return context_params;
}

void RequestContextManager::SetCustomProxyConfig(const net::ProxyConfig& custom_proxy_config)
{
  if (proxy_config_monitor_) {
    proxy_config_monitor_->SetCustomProxyConfig(custom_proxy_config);
  }
}

}  // namespace blpwtk2
