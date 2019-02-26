// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/network_service_client.h"

#include "base/optional.h"
#include "base/task/post_task.h"
#include "base/unguessable_token.h"
#include "content/browser/browsing_data/clear_site_data_handler.h"
#include "content/browser/devtools/devtools_url_loader_interceptor.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/ssl/ssl_client_auth_handler.h"
#include "content/browser/ssl/ssl_error_handler.h"
#include "content/browser/ssl/ssl_manager.h"
#include "content/browser/ssl_private_key_impl.h"
#include "content/browser/web_contents/web_contents_getter_registry.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/login_delegate.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/common/network_service_util.h"
#include "content/public/common/resource_type.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "net/ssl/client_cert_store.h"
#include "services/network/public/mojom/network_context.mojom.h"

#if defined(OS_ANDROID)
#include "base/android/content_uri_utils.h"
#endif

namespace content {
namespace {

class SSLErrorDelegate : public SSLErrorHandler::Delegate {
 public:
  explicit SSLErrorDelegate(
      network::mojom::NetworkServiceClient::OnSSLCertificateErrorCallback
          response)
      : response_(std::move(response)), weak_factory_(this) {}
  ~SSLErrorDelegate() override {}
  void CancelSSLRequest(int error, const net::SSLInfo* ssl_info) override {
    std::move(response_).Run(error);
    delete this;
  }
  void ContinueSSLRequest() override {
    std::move(response_).Run(net::OK);
    delete this;
  }
  base::WeakPtr<SSLErrorDelegate> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  network::mojom::NetworkServiceClient::OnSSLCertificateErrorCallback response_;
  base::WeakPtrFactory<SSLErrorDelegate> weak_factory_;
};

// This class is created on UI thread, and deleted by
// BrowserThread::DeleteSoon() after the |callback_| runs. The |callback_|
// needs to run on UI thread since it is called through the
// NetworkServiceClient interface.
//
// The |ssl_client_auth_handler_| needs to be created on IO thread, and deleted
// on the same thread by posting a BrowserThread::DeleteSoon() task to IO
// thread.
//
// ContinueWithCertificate() and CancelCertificateSelection() run on IO thread.
class SSLClientAuthDelegate : public SSLClientAuthHandler::Delegate {
 public:
  SSLClientAuthDelegate(
      network::mojom::NetworkServiceClient::OnCertificateRequestedCallback
          callback,
      ResourceRequestInfo::WebContentsGetter web_contents_getter,
      scoped_refptr<net::SSLCertRequestInfo> cert_info)
      : callback_(std::move(callback)), cert_info_(cert_info) {
    content::WebContents* web_contents = web_contents_getter.Run();
    content::BrowserContext* browser_context =
        web_contents->GetBrowserContext();
    content::ResourceContext* resource_context =
        browser_context->GetResourceContext();
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&SSLClientAuthDelegate::CreateSSLClientAuthHandler,
                       base::Unretained(this), resource_context,
                       web_contents_getter));
  }
  ~SSLClientAuthDelegate() override {}

  // SSLClientAuthHandler::Delegate:
  void ContinueWithCertificate(
      scoped_refptr<net::X509Certificate> cert,
      scoped_refptr<net::SSLPrivateKey> private_key) override {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);

    DCHECK((cert && private_key) || (!cert && !private_key));

    std::string provider_name;
    std::vector<uint16_t> algorithm_preferences;
    network::mojom::SSLPrivateKeyPtr ssl_private_key;
    auto ssl_private_key_request = mojo::MakeRequest(&ssl_private_key);

    if (private_key) {
      provider_name = private_key->GetProviderName();
      algorithm_preferences = private_key->GetAlgorithmPreferences();
      mojo::MakeStrongBinding(
          std::make_unique<SSLPrivateKeyImpl>(std::move(private_key)),
          std::move(ssl_private_key_request));
    }

    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&SSLClientAuthDelegate::RunCallback,
                       base::Unretained(this), cert, std::move(provider_name),
                       std::move(algorithm_preferences),
                       std::move(ssl_private_key),
                       false /* cancel_certificate_selection */));
  }

  // SSLClientAuthHandler::Delegate:
  void CancelCertificateSelection() override {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);

    network::mojom::SSLPrivateKeyPtr ssl_private_key;
    mojo::MakeRequest(&ssl_private_key);
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&SSLClientAuthDelegate::RunCallback,
                       base::Unretained(this), nullptr, std::string(),
                       std::vector<uint16_t>(), std::move(ssl_private_key),
                       true /* cancel_certificate_selection */));
  }

  void RunCallback(scoped_refptr<net::X509Certificate> cert,
                   std::string provider_name,
                   std::vector<uint16_t> algorithm_preferences,
                   network::mojom::SSLPrivateKeyPtr ssl_private_key,
                   bool cancel_certificate_selection) {
    std::move(callback_).Run(cert, provider_name, algorithm_preferences,
                             std::move(ssl_private_key),
                             cancel_certificate_selection);
    BrowserThread::DeleteSoon(BrowserThread::IO, FROM_HERE, this);
  }

 private:
  void CreateSSLClientAuthHandler(
      content::ResourceContext* resource_context,
      ResourceRequestInfo::WebContentsGetter web_contents_getter) {
    std::unique_ptr<net::ClientCertStore> client_cert_store =
        GetContentClient()->browser()->CreateClientCertStore(resource_context);
    ssl_client_auth_handler_.reset(new SSLClientAuthHandler(
        std::move(client_cert_store), std::move(web_contents_getter),
        cert_info_.get(), this));
    ssl_client_auth_handler_->SelectCertificate();
  }

  network::mojom::NetworkServiceClient::OnCertificateRequestedCallback
      callback_;
  scoped_refptr<net::SSLCertRequestInfo> cert_info_;
  std::unique_ptr<SSLClientAuthHandler> ssl_client_auth_handler_;
};

// LoginHandlerDelegateIO handles HTTP auth on the IO thread.
//
// TODO(https://crbug.com/908926): This can be folded into LoginHandlerDelegate
// with the thread-hops simplified once CreateLoginDelegate is moved to the UI
// thread.
class LoginHandlerDelegateIO {
 public:
  LoginHandlerDelegateIO(
      LoginAuthRequiredCallback callback,
      ResourceRequestInfo::WebContentsGetter web_contents_getter,
      scoped_refptr<net::AuthChallengeInfo> auth_info,
      bool is_request_for_main_frame,
      uint32_t process_id,
      uint32_t routing_id,
      uint32_t request_id,
      const GURL& url,
      scoped_refptr<net::HttpResponseHeaders> response_headers,
      bool first_auth_attempt)
      : callback_(std::move(callback)),
        auth_info_(auth_info),
        request_id_(process_id, request_id),
        routing_id_(routing_id),
        is_request_for_main_frame_(is_request_for_main_frame),
        url_(url),
        response_headers_(std::move(response_headers)),
        first_auth_attempt_(first_auth_attempt),
        web_contents_getter_(web_contents_getter),
        weak_factory_(this) {
    // This object may be created on any thread, but it must be destroyed and
    // otherwise accessed on the IO thread.
  }

  ~LoginHandlerDelegateIO() {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    if (login_delegate_)
      login_delegate_->OnRequestCancelled();
  }

  void Start() {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    DevToolsURLLoaderInterceptor::HandleAuthRequest(
        request_id_.child_id, routing_id_, request_id_.request_id, auth_info_,
        base::BindOnce(&LoginHandlerDelegateIO::ContinueAfterInterceptor,
                       weak_factory_.GetWeakPtr()));
  }

 private:
  void ContinueAfterInterceptor(
      bool use_fallback,
      const base::Optional<net::AuthCredentials>& auth_credentials) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    DCHECK(!(use_fallback && auth_credentials.has_value()));
    if (!use_fallback) {
      RunAuthCredentials(auth_credentials);
      return;
    }

    // WeakPtr is not strictly necessary here due to OnRequestCancelled.
    login_delegate_ = GetContentClient()->browser()->CreateLoginDelegate(
        auth_info_.get(), web_contents_getter_, request_id_,
        is_request_for_main_frame_, url_, response_headers_,
        first_auth_attempt_,
        base::BindOnce(&LoginHandlerDelegateIO::RunAuthCredentials,
                       weak_factory_.GetWeakPtr()));
    if (!login_delegate_) {
      RunAuthCredentials(base::nullopt);
      return;
    }
  }

  void RunAuthCredentials(
      const base::Optional<net::AuthCredentials>& auth_credentials) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    std::move(callback_).Run(auth_credentials);
    // There is no need to call OnRequestCancelled in the destructor.
    login_delegate_ = nullptr;
  }

  LoginAuthRequiredCallback callback_;
  scoped_refptr<net::AuthChallengeInfo> auth_info_;
  const content::GlobalRequestID request_id_;
  const uint32_t routing_id_;
  bool is_request_for_main_frame_;
  GURL url_;
  const scoped_refptr<net::HttpResponseHeaders> response_headers_;
  bool first_auth_attempt_;
  ResourceRequestInfo::WebContentsGetter web_contents_getter_;
  scoped_refptr<LoginDelegate> login_delegate_;
  base::WeakPtrFactory<LoginHandlerDelegateIO> weak_factory_;
};

// LoginHanderDelegate manages LoginHandlerDelegateIO from the UI thread. It is
// self-owning and deletes itself when the credentials are resolved or the
// AuthChallengeResponder is cancelled.
class LoginHandlerDelegate {
 public:
  LoginHandlerDelegate(
      network::mojom::AuthChallengeResponderPtr auth_challenge_responder,
      ResourceRequestInfo::WebContentsGetter web_contents_getter,
      scoped_refptr<net::AuthChallengeInfo> auth_info,
      bool is_request_for_main_frame,
      uint32_t process_id,
      uint32_t routing_id,
      uint32_t request_id,
      const GURL& url,
      scoped_refptr<net::HttpResponseHeaders> response_headers,
      bool first_auth_attempt)
      : auth_challenge_responder_(std::move(auth_challenge_responder)),
        weak_factory_(this) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    auth_challenge_responder_.set_connection_error_handler(base::BindOnce(
        &LoginHandlerDelegate::OnRequestCancelled, base::Unretained(this)));

    login_handler_io_.reset(new LoginHandlerDelegateIO(
        base::BindOnce(&LoginHandlerDelegate::OnAuthCredentialsIO,
                       weak_factory_.GetWeakPtr()),
        std::move(web_contents_getter), std::move(auth_info),
        is_request_for_main_frame, process_id, routing_id, request_id, url,
        std::move(response_headers), first_auth_attempt));

    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&LoginHandlerDelegateIO::Start,
                       base::Unretained(login_handler_io_.get())));
  }

 private:
  void OnRequestCancelled() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    // This will destroy |login_handler_io_| on the IO thread and, if needed,
    // inform the delegate.
    delete this;
  }

  static void OnAuthCredentialsIO(
      base::WeakPtr<LoginHandlerDelegate> handler,
      const base::Optional<net::AuthCredentials>& auth_credentials) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&LoginHandlerDelegate::OnAuthCredentials, handler,
                       auth_credentials));
  }

  void OnAuthCredentials(
      const base::Optional<net::AuthCredentials>& auth_credentials) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    auth_challenge_responder_->OnAuthCredentials(auth_credentials);
    delete this;
  }

  network::mojom::AuthChallengeResponderPtr auth_challenge_responder_;
  std::unique_ptr<LoginHandlerDelegateIO, BrowserThread::DeleteOnIOThread>
      login_handler_io_;
  base::WeakPtrFactory<LoginHandlerDelegate> weak_factory_;
};

void HandleFileUploadRequest(
    uint32_t process_id,
    bool async,
    const std::vector<base::FilePath>& file_paths,
    NetworkServiceClient::OnFileUploadRequestedCallback callback,
    scoped_refptr<base::TaskRunner> task_runner) {
  std::vector<base::File> files;
  uint32_t file_flags = base::File::FLAG_OPEN | base::File::FLAG_READ |
                        (async ? base::File::FLAG_ASYNC : 0);
  ChildProcessSecurityPolicy* cpsp = ChildProcessSecurityPolicy::GetInstance();
  for (const auto& file_path : file_paths) {
    if (process_id != network::mojom::kBrowserProcessId &&
        !cpsp->CanReadFile(process_id, file_path)) {
      task_runner->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), net::ERR_ACCESS_DENIED,
                                    std::vector<base::File>()));
      return;
    }
#if defined(OS_ANDROID)
    if (file_path.IsContentUri()) {
      files.push_back(base::OpenContentUriForRead(file_path));
    } else {
      files.emplace_back(file_path, file_flags);
    }
#else
    files.emplace_back(file_path, file_flags);
#endif
    if (!files.back().IsValid()) {
      task_runner->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(callback),
                         net::FileErrorToNetError(files.back().error_details()),
                         std::vector<base::File>()));
      return;
    }
  }
  task_runner->PostTask(FROM_HERE, base::BindOnce(std::move(callback), net::OK,
                                                  std::move(files)));
}

base::RepeatingCallback<WebContents*(void)> GetWebContentsFromRegistry(
    const base::UnguessableToken& window_id) {
  return WebContentsGetterRegistry::GetInstance()->Get(window_id);
}

WebContents* GetWebContents(int process_id, int routing_id) {
  if (process_id != network::mojom::kBrowserProcessId) {
    return WebContentsImpl::FromRenderFrameHostID(process_id, routing_id);
  }
  return WebContents::FromFrameTreeNodeId(routing_id);
}

BrowserContext* GetBrowserContext(int process_id, int routing_id) {
  WebContents* web_contents = GetWebContents(process_id, routing_id);
  if (web_contents)
    return web_contents->GetBrowserContext();
  // Some requests such as service worker updates are not associated with
  // a WebContents so we can't use it to obtain the BrowserContext.
  // TODO(dullweber): Could we always use RenderProcessHost?
  RenderProcessHost* process_host = RenderProcessHostImpl::FromID(process_id);
  if (process_host)
    return process_host->GetBrowserContext();

  return nullptr;
}

void OnCertificateRequestedContinuation(
    uint32_t process_id,
    uint32_t routing_id,
    uint32_t request_id,
    const scoped_refptr<net::SSLCertRequestInfo>& cert_info,
    network::mojom::NetworkServiceClient::OnCertificateRequestedCallback
        callback,
    base::RepeatingCallback<WebContents*(void)> web_contents_getter) {
  if (!web_contents_getter) {
    web_contents_getter =
        base::BindRepeating(GetWebContents, process_id, routing_id);
  }
  if (!web_contents_getter.Run()) {
    network::mojom::SSLPrivateKeyPtr ssl_private_key;
    mojo::MakeRequest(&ssl_private_key);
    std::move(callback).Run(nullptr, std::string(), std::vector<uint16_t>(),
                            std::move(ssl_private_key),
                            true /* cancel_certificate_selection */);
    return;
  }
  new SSLClientAuthDelegate(std::move(callback), std::move(web_contents_getter),
                            cert_info);  // deletes self
}

}  // namespace

NetworkServiceClient::NetworkServiceClient(
    network::mojom::NetworkServiceClientRequest network_service_client_request)
    : binding_(this, std::move(network_service_client_request))
#if defined(OS_ANDROID)
      ,
      app_status_listener_(base::android::ApplicationStatusListener::New(
          base::BindRepeating(&NetworkServiceClient::OnApplicationStateChange,
                              base::Unretained(this))))
#endif
{
  if (IsOutOfProcessNetworkService()) {
    net::CertDatabase::GetInstance()->AddObserver(this);
    memory_pressure_listener_ =
        std::make_unique<base::MemoryPressureListener>(base::BindRepeating(
            &NetworkServiceClient::OnMemoryPressure, base::Unretained(this)));
  }
}

NetworkServiceClient::~NetworkServiceClient() {
  if (IsOutOfProcessNetworkService())
    net::CertDatabase::GetInstance()->RemoveObserver(this);
}

void NetworkServiceClient::OnAuthRequired(
    uint32_t process_id,
    uint32_t routing_id,
    uint32_t request_id,
    const GURL& url,
    const GURL& site_for_cookies,
    bool first_auth_attempt,
    const scoped_refptr<net::AuthChallengeInfo>& auth_info,
    int32_t resource_type,
    const base::Optional<network::ResourceResponseHead>& head,
    network::mojom::AuthChallengeResponderPtr auth_challenge_responder) {
  base::Callback<WebContents*(void)> web_contents_getter =
      base::BindRepeating(GetWebContents, process_id, routing_id);

  if (!web_contents_getter.Run()) {
    std::move(auth_challenge_responder)->OnAuthCredentials(base::nullopt);
    return;
  }

  if (ResourceDispatcherHostImpl::Get()->DoNotPromptForLogin(
          static_cast<ResourceType>(resource_type), url, site_for_cookies)) {
    std::move(auth_challenge_responder)->OnAuthCredentials(base::nullopt);
    return;
  }

  bool is_request_for_main_frame =
      static_cast<ResourceType>(resource_type) == RESOURCE_TYPE_MAIN_FRAME;
  new LoginHandlerDelegate(std::move(auth_challenge_responder),
                           std::move(web_contents_getter), auth_info,
                           is_request_for_main_frame, process_id, routing_id,
                           request_id, url, head ? head->headers : nullptr,
                           first_auth_attempt);  // deletes self
}

void NetworkServiceClient::OnCertificateRequested(
    const base::Optional<base::UnguessableToken>& window_id,
    uint32_t process_id,
    uint32_t routing_id,
    uint32_t request_id,
    const scoped_refptr<net::SSLCertRequestInfo>& cert_info,
    network::mojom::NetworkServiceClient::OnCertificateRequestedCallback
        callback) {
  base::RepeatingCallback<WebContents*(void)> web_contents_getter;

  // Use |window_id| if it's provided.
  if (window_id) {
    base::PostTaskWithTraitsAndReplyWithResult(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&GetWebContentsFromRegistry, *window_id),
        base::BindOnce(&OnCertificateRequestedContinuation, process_id,
                       routing_id, request_id, cert_info, std::move(callback)));
    return;
  }

  OnCertificateRequestedContinuation(process_id, routing_id, request_id,
                                     cert_info, std::move(callback), {});
}

void NetworkServiceClient::OnSSLCertificateError(
    uint32_t process_id,
    uint32_t routing_id,
    uint32_t request_id,
    int32_t resource_type,
    const GURL& url,
    const net::SSLInfo& ssl_info,
    bool fatal,
    OnSSLCertificateErrorCallback response) {
  SSLErrorDelegate* delegate =
      new SSLErrorDelegate(std::move(response));  // deletes self
  base::Callback<WebContents*(void)> web_contents_getter =
      base::BindRepeating(GetWebContents, process_id, routing_id);
  SSLManager::OnSSLCertificateError(
      delegate->GetWeakPtr(), static_cast<ResourceType>(resource_type), url,
      std::move(web_contents_getter), ssl_info, fatal);
}

#if defined(OS_CHROMEOS)
void NetworkServiceClient::OnTrustAnchorUsed(const std::string& username_hash) {
  GetContentClient()->browser()->OnTrustAnchorUsed(username_hash);
}
#endif

void NetworkServiceClient::OnFileUploadRequested(
    uint32_t process_id,
    bool async,
    const std::vector<base::FilePath>& file_paths,
    OnFileUploadRequestedCallback callback) {
  base::PostTaskWithTraits(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&HandleFileUploadRequest, process_id, async, file_paths,
                     std::move(callback),
                     base::SequencedTaskRunnerHandle::Get()));
}

void NetworkServiceClient::OnCookiesRead(int process_id,
                                         int routing_id,
                                         const GURL& url,
                                         const GURL& first_party_url,
                                         const net::CookieList& cookie_list,
                                         bool blocked_by_policy) {
  GetContentClient()->browser()->OnCookiesRead(process_id, routing_id, url,
                                               first_party_url, cookie_list,
                                               blocked_by_policy);
}

void NetworkServiceClient::OnCookieChange(int process_id,
                                          int routing_id,
                                          const GURL& url,
                                          const GURL& first_party_url,
                                          const net::CanonicalCookie& cookie,
                                          bool blocked_by_policy) {
  GetContentClient()->browser()->OnCookieChange(
      process_id, routing_id, url, first_party_url, cookie, blocked_by_policy);
}

void NetworkServiceClient::OnLoadingStateUpdate(
    std::vector<network::mojom::LoadInfoPtr> infos,
    OnLoadingStateUpdateCallback callback) {
  auto rdh_infos = std::make_unique<ResourceDispatcherHostImpl::LoadInfoList>();

  // TODO(jam): once ResourceDispatcherHost is gone remove the translation
  // (other than adding the WebContents callback).
  for (auto& info : infos) {
    ResourceDispatcherHostImpl::LoadInfo load_info;
    load_info.host = std::move(info->host);
    load_info.load_state.state = static_cast<net::LoadState>(info->load_state);
    load_info.load_state.param = std::move(info->state_param);
    load_info.upload_position = info->upload_position;
    load_info.upload_size = info->upload_size;
    load_info.web_contents_getter =
        base::BindRepeating(GetWebContents, info->process_id, info->routing_id);
    rdh_infos->push_back(std::move(load_info));
  }

  auto* rdh = ResourceDispatcherHostImpl::Get();
  ResourceDispatcherHostImpl::UpdateLoadStateOnUI(rdh->loader_delegate_,
                                                  std::move(rdh_infos));

  std::move(callback).Run();
}

void NetworkServiceClient::OnClearSiteData(int process_id,
                                           int routing_id,
                                           const GURL& url,
                                           const std::string& header_value,
                                           int load_flags,
                                           OnClearSiteDataCallback callback) {
  auto browser_context_getter =
      base::BindRepeating(GetBrowserContext, process_id, routing_id);
  auto web_contents_getter =
      base::BindRepeating(GetWebContents, process_id, routing_id);
  ClearSiteDataHandler::HandleHeader(browser_context_getter,
                                     web_contents_getter, url, header_value,
                                     load_flags, std::move(callback));
}

void NetworkServiceClient::OnCertDBChanged() {
  GetNetworkService()->OnCertDBChanged();
}

void NetworkServiceClient::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level) {
  GetNetworkService()->OnMemoryPressure(memory_pressure_level);
}

#if defined(OS_ANDROID)
void NetworkServiceClient::OnApplicationStateChange(
    base::android::ApplicationState state) {
  GetNetworkService()->OnApplicationStateChange(state);
}
#endif

void NetworkServiceClient::OnDataUseUpdate(
    int32_t network_traffic_annotation_id_hash,
    int64_t recv_bytes,
    int64_t sent_bytes) {
  GetContentClient()->browser()->OnNetworkServiceDataUseUpdate(
      network_traffic_annotation_id_hash, recv_bytes, sent_bytes);
}

}  // namespace content
