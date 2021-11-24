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

#include <blpwtk2_inprocessresourceloaderbridge.h>
#include <blpwtk2_resourcecontext.h>
#include <blpwtk2_resourceloader.h>
#include <blpwtk2_statics.h>
#include <blpwtk2_string.h>

#include <net/base/load_flags.h>
#include <net/base/mime_sniffer.h>

#include "third_party/blink/public/mojom/loader/resource_load_info.mojom.h"
#include "third_party/blink/public/platform/web_navigation_body_loader.h"
#include "third_party/blink/public/platform/web_request_peer.h"
#include "third_party/blink/public/platform/web_resource_request_sender.h"


namespace {
    static char gHttpOK[] = "HTTP/1.1 200 OK\0\0";
}

namespace blpwtk2 {

                    // ======================
                    // class ResourceReceiver
                    // ======================

class ResourceReceiver {
  public:
    ResourceReceiver() {}
    virtual ~ResourceReceiver() {}

    virtual void OnReceivedResponse(network::mojom::URLResponseHeadPtr head) = 0;
    virtual void OnReceivedData(const char* data, std::size_t data_length) = 0;
    virtual void OnCompletedRequest(const network::URLLoaderCompletionStatus& completeStatus,
                                    const blink::WebURL&) = 0;
};

                    // =========================
                    // class RequestPeerReceiver
                    // =========================

class RequestPeerReceiver : public ResourceReceiver {
    blink::WebRequestPeer* peer_;
    blink::WebResourceRequestSender *sender_;
    network::mojom::URLResponseHeadPtr head_;
    std::vector<char> received_data_;
    scoped_refptr<base::SingleThreadTaskRunner> runner_;

  public:
    RequestPeerReceiver(
            blink::WebRequestPeer                       *peer,
            blink::WebResourceRequestSender             *sender,
            int                                          request_id,
            scoped_refptr<base::SingleThreadTaskRunner>  runner)
        : peer_(peer)
        , sender_(sender)
        , runner_(std::move(runner))
    {
    }

    void OnReceivedResponse(network::mojom::URLResponseHeadPtr head) override
    {
        head_ = std::move(head);
    }

    void OnReceivedData(const char* data, std::size_t data_length) override
    {
        received_data_.insert(received_data_.end(), data, data + data_length);
    }

    void OnCompletedRequest(
            const network::URLLoaderCompletionStatus& completeStatus,
            const blink::WebURL&) override
    {
        if (head_) {
            peer_->OnReceivedResponse(std::move(head_));
        }
        if (!received_data_.empty()) {
            mojo::ScopedDataPipeProducerHandle producer_handle;
            mojo::ScopedDataPipeConsumerHandle consumer_handle;
            CHECK_EQ(mojo::CreateDataPipe(received_data_.size(), producer_handle, consumer_handle), MOJO_RESULT_OK);

            peer_->OnStartLoadingResponseBody(std::move(consumer_handle));
            uint32_t len = received_data_.size();
            MojoResult result = producer_handle->WriteData(
                    received_data_.data(), &len, MOJO_WRITE_DATA_FLAG_NONE);
            // when peer_ does not have a client, consumer_handle would be closed that
            // results in MOJO_RESULT_FAILED_PRECONDITION
            DCHECK(result == MOJO_RESULT_OK || result == MOJO_RESULT_FAILED_PRECONDITION);
            producer_handle.reset();
        }

        peer_->OnCompletedRequest(completeStatus);
        sender_->DeletePendingRequest(runner_);
    }
};

                    // ========================
                    // class BodyLoaderReceiver
                    // ========================

class BodyLoaderReceiver : public ResourceReceiver {
    blink::WebNavigationBodyLoader::Client* client_;
  public:

    BodyLoaderReceiver(blink::WebNavigationBodyLoader::Client* client)
        : client_(client)
    {
    }

    void OnReceivedResponse(network::mojom::URLResponseHeadPtr head) override
    {
    }

    void OnReceivedData(const char* data, std::size_t data_length) override
    {
        client_->BodyDataReceived(base::span<const char>(data, data_length));
    }

    void OnCompletedRequest(
            const network::URLLoaderCompletionStatus& completeStatus,
            const blink::WebURL&                      url) override
    {

        base::Optional<blink::WebURLError> web_errorCode;
        if (completeStatus.error_code) {
            web_errorCode = blink::WebURLError(completeStatus.error_code, url);
        }

        client_->BodyLoadingFinished(base::TimeTicks::Now(),
                                     completeStatus.encoded_data_length,
                                     completeStatus.encoded_body_length,
                                     completeStatus.decoded_body_length,
                                     false,
                                     std::move(web_errorCode));
    }
};

                    // =========================
                    // class InProcessURLRequest
                    // =========================

class InProcessURLRequest : public URLRequest {
    GURL d_url;
    GURL d_firstPartyForCookies;
    int d_loadFlags;
    String d_httpMethod;
    bool d_enableUploadProgress;
    bool d_reportRawHeaders;
    bool d_hasUserGesture;
    net::RequestPriority d_priority;
    scoped_refptr<network::ResourceRequestBody> d_requestBody;
    net::HttpRequestHeaders d_requestHeaders;

  public:
    InProcessURLRequest(const network::ResourceRequest* request)
      : d_url(request->url),
        d_firstPartyForCookies(request->site_for_cookies.RepresentativeUrl()),
        d_loadFlags(request->load_flags),
        d_httpMethod(request->method),
        d_enableUploadProgress(request->enable_upload_progress),
        d_reportRawHeaders(request->report_raw_headers),
        d_hasUserGesture(request->has_user_gesture),
        d_priority(request->priority),
        d_requestBody(request->request_body)
    {
        d_requestHeaders.MergeFrom(request->headers);
    }

    ~InProcessURLRequest() final {}

    String url() const override
    {
        return String(d_url.spec());
    }

    String firstPartyForCookies() const override
    {
        return String(d_firstPartyForCookies.spec());
    }

    // see GetLoadFlagsForWebURLRequest() in web_url_request_util.cc:
    bool allowStoredCredentials() const override
    {
        static const int disallow_flags = net::LOAD_DO_NOT_SAVE_COOKIES;
        return (d_loadFlags & disallow_flags) != disallow_flags;
    }

    // see GetLoadFlagsForWebURLRequest() in web_url_request_util.cc:
    CachePolicy cachePolicy() const override
    {
        if (d_loadFlags & net::LOAD_VALIDATE_CACHE) {
            return CachePolicy::ReloadIgnoringCacheData;
        }
        else if (d_loadFlags & net::LOAD_BYPASS_CACHE) {
            return CachePolicy::ReloadBypassingCache;
        }
        else if (d_loadFlags & net::LOAD_SKIP_CACHE_VALIDATION) {
            return CachePolicy::ReturnCacheDataElseLoad;
        }
        else if (d_loadFlags & net::LOAD_ONLY_FROM_CACHE) {
            return CachePolicy::ReturnCacheDataDontLoad;
        }
        else {
            return CachePolicy::UseProtocolCachePolicy;
        }
    }

    String httpMethod() const override
    {
        return d_httpMethod;
    }

    String httpHeaderField(const StringRef& name) const override
    {
        std::string value;
        d_requestHeaders.GetHeader(name.toStdString(), &value);
        return String(value);
    }

    void visitHTTPHeaderFields(HTTPHeaderVisitor* visitor) const override
    {
        if (!d_requestHeaders.IsEmpty()) {
            net::HttpRequestHeaders::Iterator it(d_requestHeaders);
            do {
                visitor->visitHeader(StringRef(it.name()), StringRef(it.value()));
            } while (it.GetNext());
        }

        CHECK(!d_requestBody || d_requestBody->elements()->empty());
    }

    bool reportUploadProgress() const override
    {
        return d_enableUploadProgress;
    }

    bool reportRawHeaders() const override
    {
        return d_reportRawHeaders;
    }

    bool hasUserGesture() const override
    {
        return d_hasUserGesture;
    }

    // see ConvertWebKitPriorityToNetPriority() in web_url_loader_impl.cc:
    Priority priority() const override
    {
        switch (d_priority) {
            case net::HIGHEST:
                return Priority::VeryHigh;
            case net::MEDIUM:
                return Priority::High;
            case net::LOW:
                return Priority::Medium;
            case net::LOWEST:
                return Priority::Low;
            case net::IDLE:
                return Priority::VeryLow;
            default:
                return Priority::Unresolved;
        }
    }
};

                    // ==============================
                    // class InProcessResourceContext
                    // ==============================

class InProcessResourceContext
    : public base::RefCounted<InProcessResourceContext>,
      public ResourceContext {

    std::unique_ptr<InProcessURLRequest> d_urlRequest;
    GURL d_url;
    scoped_refptr<net::HttpResponseHeaders> d_responseHeaders;
    std::unique_ptr<ResourceReceiver> d_peer;
    void* d_userData;
    int64_t d_totalTransferSize;
    bool d_started;
    bool d_waitingForCancelLoad;  // waiting for cancelLoad()
    bool d_canceled;
    bool d_failed;
    bool d_finished;

    friend class base::RefCounted<InProcessResourceContext>;
    ~InProcessResourceContext() final;

    void startLoad();
    void cancelLoad();
    void ensureResponseHeadersSent(const char* buffer, int length);

 public:
    InProcessResourceContext();

    // accessors
    const GURL& url() const;

    // manipulators
    bool start(std::unique_ptr<ResourceReceiver>  peer,
               network::ResourceRequest          *request);
    void cancel();

    // ResourceContext overrides
    const URLRequest* request() override;
    void replaceStatusLine(const StringRef& newStatus) override;
    void addResponseHeader(const StringRef& header) override;
    bool hasResponseHeaderValue(const StringRef& name,
                              const StringRef& value) const override;
    bool hasReponseHeader(const StringRef& name) const override;
    void addResponseData(const char* buffer, int length) override;
    void failed() override;
    void finish() override;

    // notified by its owner InProcessResourceLoaderBridge
    void OnBridgeDeleted();
};

                    // ------------------------------
                    // class InProcessResourceContext
                    // ------------------------------

InProcessResourceContext::InProcessResourceContext()
    : d_userData(0),
      d_totalTransferSize(0),
      d_started(false),
      d_waitingForCancelLoad(false),
      d_canceled(false),
      d_failed(false),
      d_finished(false)
{
    DCHECK(Statics::isInApplicationMainThread());
    DCHECK(Statics::inProcessResourceLoader);
    d_responseHeaders = base::MakeRefCounted<net::HttpResponseHeaders>(gHttpOK);
}

InProcessResourceContext::~InProcessResourceContext()
{
}

// accessors
const GURL& InProcessResourceContext::url() const
{
    return d_url;
}

// manipulators
bool InProcessResourceContext::start(std::unique_ptr<ResourceReceiver>  peer,
                                     network::ResourceRequest          *request)
{
    DCHECK(Statics::isInApplicationMainThread());
    DCHECK(!d_started);
    DCHECK(!d_waitingForCancelLoad);
    DCHECK(!d_canceled);
    DCHECK(!d_failed);
    DCHECK(!d_finished);

    d_urlRequest = std::make_unique<InProcessURLRequest>(request);
    d_peer = std::move(peer);
    d_url = request->url;

    DCHECK(Statics::inProcessResourceLoader->canHandleURL(d_url.spec()));

    base::ThreadTaskRunnerHandle::Get()->PostTask(
            FROM_HERE, base::BindOnce(&InProcessResourceContext::startLoad, this));
    return true;
}

void InProcessResourceContext::cancel()
{
    DCHECK(Statics::isInApplicationMainThread());

    if (d_waitingForCancelLoad || d_canceled) {
        // Sometimes Cancel() gets called twice.  If we already got canceled,
        // then ignore any further calls to Cancel().
        return;
    }

    // Keep this alive for ::cancelLoad callback
    AddRef();
    d_waitingForCancelLoad = true;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
            FROM_HERE, base::BindOnce(&InProcessResourceContext::cancelLoad, this));
}

// ResourceContext overrides
const URLRequest* InProcessResourceContext::request()
{
    DCHECK(Statics::isInApplicationMainThread());
    return d_urlRequest.get();
}

void InProcessResourceContext::replaceStatusLine(const StringRef& newStatus)
{
    DCHECK(Statics::isInApplicationMainThread());
    DCHECK(!d_failed);

    std::string str(newStatus.data(), newStatus.length());
    d_responseHeaders->ReplaceStatusLine(str);
}

void InProcessResourceContext::addResponseHeader(const StringRef& header)
{
    DCHECK(Statics::isInApplicationMainThread());
    DCHECK(!d_failed);

    if (d_responseHeaders) {
        std::string str(header.data(), header.length());

        const std::string::size_type name_end_index = str.find(": ");
        DCHECK(name_end_index != std::string::npos);
        const base::StringPiece name(header.data(), name_end_index);
        const std::string::size_type value_pos = name_end_index + 2;
        const base::StringPiece value(header.data() + value_pos,
                header.length() - value_pos);

        d_responseHeaders->AddHeader(name, value);
    }
}

bool InProcessResourceContext::hasResponseHeaderValue(
        const StringRef& name, const StringRef& value) const
{
    if (!d_responseHeaders) {
        return false;
    }
    return d_responseHeaders->HasHeaderValue(
            std::string(name.data(), name.length()),
            std::string(value.data(), value.length()));
}

bool InProcessResourceContext::hasReponseHeader(const StringRef& name) const
{
    if (!d_responseHeaders) {
        return false;
    }
    return d_responseHeaders->HasHeader(std::string(name.data(), name.length()));
}

void InProcessResourceContext::addResponseData(const char* buffer, int length)
{
    DCHECK(Statics::isInApplicationMainThread());
    DCHECK(!d_failed);

    if (0 == length)
        return;

    d_totalTransferSize += length;

    if (!d_peer)
        return;

    ensureResponseHeadersSent(buffer, length);
    // The bridge might have been disposed when sending the response
    // headers, so we need to check again.
    if (!d_peer)
        return;
    d_peer->OnReceivedData(buffer, length);
}

void InProcessResourceContext::failed()
{
    DCHECK(Statics::isInApplicationMainThread());
    DCHECK(!d_finished);
    d_failed = true;
}

void InProcessResourceContext::finish()
{
    DCHECK(Statics::isInApplicationMainThread());
    d_finished = true;

    if (d_waitingForCancelLoad) {
        // Application finished before we could notify it that the resource
        // was canceled.  We should wait for 'cancelLoad()' to get called,
        // where we will destroy ourself.

        // This is to balance the AddRef from startLoad().
        if(d_started) {
            Release();
        }
        return;
    }

    if (d_peer) {
        ensureResponseHeadersSent(0, 0);
    }

    // The bridge might have been disposed when the headers were sent,
    // so check this again.
    if (d_peer) {
        int errorCode =
            d_failed ? net::ERR_FAILED : d_canceled ? net::ERR_ABORTED : net::OK;
        network::URLLoaderCompletionStatus completeStatus;
        completeStatus.error_code = errorCode;
        completeStatus.exists_in_cache = false;
        completeStatus.completion_time = base::TimeTicks::Now();
        completeStatus.encoded_data_length = d_totalTransferSize;
        completeStatus.encoded_body_length = d_totalTransferSize;
        completeStatus.decoded_body_length = d_totalTransferSize;
        d_peer->OnCompletedRequest(completeStatus, d_url);
        d_responseHeaders.reset();
    } else {
        LOG(WARNING) << "d_peer has been deleted before finishing loading";
    }

    // This is to balance the AddRef from startLoad().
    Release();
}

void InProcessResourceContext::startLoad()
{
    DCHECK(Statics::isInApplicationMainThread());
    DCHECK(!d_started);
    DCHECK(!d_canceled);

    if (d_waitingForCancelLoad || Statics::isTerminating) {
        // We got canceled before we even started the resource on the loader.
        // We should wait for 'cancelLoad()' to get called, where we will
        // destroy ourself.
        return;
    }

    d_started = true;

    // Adding a reference on behalf of the embedder. This is Release'd
    // on finish().
    AddRef();
    Statics::inProcessResourceLoader->start(d_url.spec(), this, &d_userData);
}

void InProcessResourceContext::cancelLoad()
{
    DCHECK(Statics::isInApplicationMainThread());
    DCHECK(d_waitingForCancelLoad);

    if (!d_started || d_finished) {
        if (d_peer) {
            // Resource canceled before we could start it on the loader, or the
            // loader finished before we could notify it of cancellation.  We can
            // now safely destroy ourself.
            network::URLLoaderCompletionStatus completeStatus;
            completeStatus.error_code = net::ERR_ABORTED;
            completeStatus.exists_in_cache = false;
            completeStatus.completion_time = base::TimeTicks::Now();
            completeStatus.encoded_data_length = d_totalTransferSize;
            completeStatus.encoded_body_length = d_totalTransferSize;
            completeStatus.decoded_body_length = d_totalTransferSize;
            d_peer->OnCompletedRequest(completeStatus, d_url);
            d_responseHeaders.reset();
        }
        // for AddRef() in ::cancel()
        Release();
        return;
    }

    d_waitingForCancelLoad = false;
    d_canceled = true;
    Statics::inProcessResourceLoader->cancel(this, d_userData);
    // for AddRef() in ::cancel()
    Release();
}

void InProcessResourceContext::ensureResponseHeadersSent(const char* buffer,
                                                         int         length)
{
    DCHECK(Statics::isInApplicationMainThread());
    DCHECK(d_peer);

    if (!d_responseHeaders.get() || !d_peer) {
        return;
    }

    network::mojom::URLResponseHeadPtr responseInfo =
        network::mojom::URLResponseHead::New();
    responseInfo->headers = d_responseHeaders;
    responseInfo->content_length = d_responseHeaders->GetContentLength();
    d_responseHeaders->GetMimeTypeAndCharset(&responseInfo->mime_type,
            &responseInfo->charset);
    if (responseInfo->mime_type.empty() && length > 0) {
        const base::StringPiece content(buffer,
                std::min(length, net::kMaxBytesToSniff));
        net::SniffMimeType(content, d_url,
                "", net::ForceSniffFileUrlsForHtml::kDisabled,
                &responseInfo->mime_type);
    }

    d_peer->OnReceivedResponse(std::move(responseInfo));
}

void InProcessResourceContext::OnBridgeDeleted()
{
    d_peer.reset();
}

                    // ==========================
                    // class NavigationBodyLoader
                    // ==========================

class NavigationBodyLoader : public blink::WebNavigationBodyLoader {
    scoped_refptr<InProcessResourceContext> d_context;
    network::ResourceRequest d_request;


  public:
    NavigationBodyLoader(network::ResourceRequest request)
        : d_context(base::MakeRefCounted<InProcessResourceContext>())
        , d_request(request)
    {
    }

    ~NavigationBodyLoader() override
    {
        d_context->OnBridgeDeleted();
    }

    void SetDefersLoading(blink::WebURLLoader::DeferType defers) override {}

    void StartLoadingBody(blink::WebNavigationBodyLoader::Client* client,
                          bool) override
    {
        DCHECK(Statics::isInApplicationMainThread());
        d_context->start(std::make_unique<BodyLoaderReceiver>(client), &d_request);
    }
};

                    // -----------------------------------
                    // class InProcessResourceLoaderBridge
                    // -----------------------------------

InProcessResourceLoaderBridge::InProcessResourceLoaderBridge()
{
    DCHECK(Statics::isInApplicationMainThread());
    DCHECK(Statics::inProcessResourceLoader);
}

InProcessResourceLoaderBridge::~InProcessResourceLoaderBridge()
{
}

void InProcessResourceLoaderBridge::OnRequestComplete()
{
}

scoped_refptr<blink::WebRequestPeer>
InProcessResourceLoaderBridge::OnReceivedResponse(
        scoped_refptr<blink::WebRequestPeer> current_peer,
        const blink::WebString&              mime_type,
        const blink::WebURL&                 url)
{
    return current_peer;
}

bool InProcessResourceLoaderBridge::CanHandleURL(const std::string& url)
{
    return Statics::inProcessResourceLoader &&
           Statics::inProcessResourceLoader->canHandleURL(StringRef(url));
}

std::unique_ptr<blink::WebNavigationBodyLoader>
InProcessResourceLoaderBridge::CreateBodyLoader(network::ResourceRequest* request)
{
    return std::make_unique<NavigationBodyLoader>(*request);
}

void InProcessResourceLoaderBridge::Start(
        blink::WebRequestPeer                       *peer,
        blink::WebResourceRequestSender             *sender,
        network::ResourceRequest                    *request,
        int                                          request_id,
        scoped_refptr<base::SingleThreadTaskRunner>  runner)
{
    DCHECK(Statics::isInApplicationMainThread());

    scoped_refptr<InProcessResourceContext> context =
        base::MakeRefCounted<InProcessResourceContext>();

    context->start(std::make_unique<RequestPeerReceiver>(
                peer, sender, request_id, runner), request);

    d_contexts[request_id] = context;
}

void InProcessResourceLoaderBridge::Cancel(int request_id)
{
    DCHECK(Statics::isInApplicationMainThread());

    auto it = d_contexts.find(request_id);
    DCHECK(it != d_contexts.end());

    if (it != d_contexts.end()) {
        it->second->cancel();
        d_contexts.erase(it);
    }
}


}  // namespace blpwtk2
