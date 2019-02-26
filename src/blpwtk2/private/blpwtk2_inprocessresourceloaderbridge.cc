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

#include <blpwtk2_blob.h>
#include <blpwtk2_resourcecontext.h>
#include <blpwtk2_resourceloader.h>
#include <blpwtk2_statics.h>
#include <blpwtk2_string.h>

#include <base/bind.h>
#include <base/message_loop/message_loop.h>
#include <content/renderer/loader/sync_load_response.h>
#include <content/public/renderer/request_peer.h>
#include <net/base/load_flags.h>
#include <net/base/mime_sniffer.h>
#include <net/base/net_errors.h>
#include <net/http/http_request_headers.h>
#include <net/http/http_response_headers.h>
#include <services/network/public/cpp/resource_request.h>
#include <services/network/public/cpp/resource_response_info.h>
#include <third_party/blink/public/platform/web_url_request.h>
#include <url/gurl.h>

namespace blpwtk2 {

class ReceivedDataImpl : public content::RequestPeer::ReceivedData {
  public:
    std::vector<char> d_data;

    const char* payload() const override { return d_data.data(); }
    int length() const override { return d_data.size(); }
};

class InProcessResourceLoaderBridge::InProcessURLRequest
    : public URLRequest {
  public:

    InProcessURLRequest(
        const network::ResourceRequest *request)
    : d_url(request->url)
    , d_firstPartyForCookies(request->site_for_cookies)
    , d_loadFlags(request->load_flags)
    , d_httpMethod(request->method)
    , d_enableUploadProgress(request->enable_upload_progress)
    , d_reportRawHeaders(request->report_raw_headers)
    , d_hasUserGesture(request->has_user_gesture)
    , d_routingId(request->render_frame_id)
    , d_appCacheHostId(request->appcache_host_id)
    , d_priority(request->priority)
    , d_requestBody(request->request_body)
    {
        d_requestHeaders.MergeFrom(request->headers);
    }

    ~InProcessURLRequest() final {}

    String url() const override {
        return String(d_url.spec());
    }

    String firstPartyForCookies() const override {
        return String(d_firstPartyForCookies.spec());
    }

    // see GetLoadFlagsForWebURLRequest() in web_url_request_util.cc:
    bool allowStoredCredentials() const override {
        static const int disallow_flags =
            net::LOAD_DO_NOT_SAVE_COOKIES |
            net::LOAD_DO_NOT_SEND_COOKIES |
            net::LOAD_DO_NOT_SEND_AUTH_DATA;

        return (d_loadFlags & disallow_flags) != disallow_flags;
    }

    // see GetLoadFlagsForWebURLRequest() in web_url_request_util.cc:
    CachePolicy cachePolicy() const override {
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

    String httpMethod() const override {
        return d_httpMethod;
    }

    String httpHeaderField(const StringRef& name) const override {
        std::string value;
        d_requestHeaders.GetHeader(name.toStdString(), &value);
        return String(value);
    }

    void visitHTTPHeaderFields(HTTPHeaderVisitor* visitor) const override {
        if (!d_requestHeaders.IsEmpty()) {
            net::HttpRequestHeaders::Iterator it(d_requestHeaders);
            do {
                visitor->visitHeader(
                    StringRef(it.name()),
                    StringRef(it.value()));
            } while(it.GetNext());
        }
    }

    void visitHTTPBody(HTTPBodyVisitor* visitor) const override {
        if (!d_requestBody) {
            return;
        }

        for (const network::DataElement& elem : *(d_requestBody->elements())) {
            Blob elementBlob;
            elementBlob.makeStorageDataElement(elem);
            visitor->visitBodyElement(elementBlob);
        }
    }

    bool reportUploadProgress() const override {
        return d_enableUploadProgress;
    }

    bool reportRawHeaders() const override {
        return d_reportRawHeaders;
    }

    bool hasUserGesture() const override {
        return d_hasUserGesture;
    }

    int requesterID() const override {
        return d_routingId;
    }

    int appCacheHostID() const override {
        return d_appCacheHostId;
    }

    // see ConvertWebKitPriorityToNetPriority() in web_url_loader_impl.cc:
    Priority priority() const override {
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

  private:
    GURL d_url;
    GURL d_firstPartyForCookies;
    int d_loadFlags;
    String d_httpMethod;
    bool d_enableUploadProgress;
    bool d_reportRawHeaders;
    bool d_hasUserGesture;
    int d_routingId;
    int d_appCacheHostId;
    net::RequestPriority d_priority;

    scoped_refptr<network::ResourceRequestBody> d_requestBody;
    net::HttpRequestHeaders d_requestHeaders;
};

class InProcessResourceLoaderBridge::InProcessResourceContext
    : public base::RefCounted<InProcessResourceContext>
    , public ResourceContext {
  public:
    InProcessResourceContext(
        const network::ResourceRequest *request);

    // accessors
    const GURL& url() const;

    // manipulators
    bool start(content::RequestPeer* peer);
    void cancel();
    void dispose();

    // ResourceContext overrides
    const URLRequest* request() override;
    void replaceStatusLine(const StringRef& newStatus) override;
    void addResponseHeader(const StringRef& header) override;
    void addResponseData(const char* buffer, int length) override;
    void failed() override;
    void finish() override;

  private:
    friend class base::RefCounted<InProcessResourceContext>;
    ~InProcessResourceContext() final;

    void startLoad();
    void cancelLoad();
    void ensureResponseHeadersSent(const char* buffer, int length);

    std::unique_ptr<InProcessURLRequest> d_urlRequest;
    GURL d_url;
    scoped_refptr<net::HttpResponseHeaders> d_responseHeaders;
    content::RequestPeer* d_peer;
    void* d_userData;
    int64_t d_totalTransferSize;
    bool d_started;
    bool d_waitingForCancelLoad;  // waiting for cancelLoad()
    bool d_canceled;
    bool d_failed;
    bool d_finished;

    DISALLOW_COPY_AND_ASSIGN(InProcessResourceContext);
};

// InProcessResourceContext

InProcessResourceLoaderBridge::InProcessResourceContext::InProcessResourceContext(
    const network::ResourceRequest *request)
: d_urlRequest(new InProcessURLRequest(request))
, d_url(request->url)
, d_peer(0)
, d_userData(0)
, d_totalTransferSize(0)
, d_started(false)
, d_waitingForCancelLoad(false)
, d_canceled(false)
, d_failed(false)
, d_finished(false)
{
    DCHECK(Statics::isInApplicationMainThread());
    DCHECK(Statics::inProcessResourceLoader);
    DCHECK(Statics::inProcessResourceLoader->canHandleURL(d_url.spec()));
    d_responseHeaders = new net::HttpResponseHeaders("HTTP/1.1 200 OK\0\0");
}

InProcessResourceLoaderBridge::InProcessResourceContext::~InProcessResourceContext()
{
}

// accessors
const GURL& InProcessResourceLoaderBridge::InProcessResourceContext::url() const
{
    return d_url;
}

// manipulators
bool InProcessResourceLoaderBridge::InProcessResourceContext::start(content::RequestPeer* peer)
{
    DCHECK(Statics::isInApplicationMainThread());
    DCHECK(!d_started);
    DCHECK(!d_waitingForCancelLoad);
    DCHECK(!d_canceled);
    DCHECK(!d_failed);
    DCHECK(!d_finished);

    d_peer = peer;

    base::MessageLoop::current()->task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&InProcessResourceContext::startLoad,
                   this));
    return true;
}

void InProcessResourceLoaderBridge::InProcessResourceContext::cancel()
{
    DCHECK(Statics::isInApplicationMainThread());

    if (d_waitingForCancelLoad || d_canceled) {
        // Sometimes Cancel() gets called twice.  If we already got canceled,
        // then ignore any further calls to Cancel().
        return;
    }

    d_waitingForCancelLoad = true;
    base::MessageLoop::current()->task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&InProcessResourceContext::cancelLoad,
                   this));
}

void InProcessResourceLoaderBridge::InProcessResourceContext::dispose()
{
    DCHECK(Statics::isInApplicationMainThread());
    d_peer = NULL;
}

// ResourceContext overrides
const URLRequest* InProcessResourceLoaderBridge::InProcessResourceContext::request()
{
    DCHECK(Statics::isInApplicationMainThread());

    return d_urlRequest.get();
}

void InProcessResourceLoaderBridge::InProcessResourceContext::replaceStatusLine(
    const StringRef& newStatus)
{
    DCHECK(Statics::isInApplicationMainThread());
    DCHECK(!d_failed);
    DCHECK(d_responseHeaders.get());

    std::string str(newStatus.data(), newStatus.length());
    d_responseHeaders->ReplaceStatusLine(str);
}

void InProcessResourceLoaderBridge::InProcessResourceContext::addResponseHeader(const StringRef& header)
{
    DCHECK(Statics::isInApplicationMainThread());
    DCHECK(!d_failed);
    DCHECK(d_responseHeaders.get());

    std::string str(header.data(), header.length());
    d_responseHeaders->AddHeader(str);
}

void InProcessResourceLoaderBridge::InProcessResourceContext::addResponseData(const char* buffer,
                                                                              int length)
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

    // TODO: Remove the need to copy here.
    std::unique_ptr<ReceivedDataImpl> copiedData(new ReceivedDataImpl());
    copiedData->d_data.assign(buffer, buffer + length);

    d_peer->OnReceivedData(std::move(copiedData));
}

void InProcessResourceLoaderBridge::InProcessResourceContext::failed()
{
    DCHECK(Statics::isInApplicationMainThread());
    DCHECK(!d_finished);
    d_failed = true;
}

void InProcessResourceLoaderBridge::InProcessResourceContext::finish()
{
    DCHECK(Statics::isInApplicationMainThread());
    DCHECK(!d_finished);

    d_finished = true;

    if (d_waitingForCancelLoad) {
        // Application finished before we could notify it that the resource
        // was canceled.  We should wait for 'cancelLoad()' to get called,
        // where we will destroy ourself.

        // This is to balance the AddRef from startLoad().
        Release();
        return;
    }

    if (d_peer) {
        ensureResponseHeadersSent(0, 0);
    }

    // The bridge might have been disposed when the headers were sent,
    // so check this again.
    if (d_peer) {
        int errorCode = d_failed ? net::ERR_FAILED
                                 : d_canceled ? net::ERR_ABORTED
                                              : net::OK;
        network::URLLoaderCompletionStatus completeStatus;
        completeStatus.error_code = errorCode;
        completeStatus.exists_in_cache = false;
        completeStatus.completion_time = base::TimeTicks::Now();
        completeStatus.encoded_data_length = d_totalTransferSize;
        completeStatus.encoded_body_length = d_totalTransferSize;
        completeStatus.decoded_body_length = d_totalTransferSize;
        // InProcessResourceLoaderBridge will get deleted inside this callback.
        d_peer->OnCompletedRequest(completeStatus);
    }

    // This is to balance the AddRef from startLoad().
    Release();
}

void InProcessResourceLoaderBridge::InProcessResourceContext::startLoad()
{
    DCHECK(Statics::isInApplicationMainThread());
    DCHECK(!d_started);
    DCHECK(!d_canceled);

    if (d_waitingForCancelLoad) {
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

void InProcessResourceLoaderBridge::InProcessResourceContext::cancelLoad()
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

            // InProcessResourceLoaderBridge will get deleted inside this callback.
            d_peer->OnCompletedRequest(completeStatus);
        }
        return;
    }

    d_waitingForCancelLoad = false;
    d_canceled = true;
    Statics::inProcessResourceLoader->cancel(this, d_userData);
}

void InProcessResourceLoaderBridge::InProcessResourceContext::ensureResponseHeadersSent(
    const char* buffer,
    int length)
{
    DCHECK(Statics::isInApplicationMainThread());
    DCHECK(d_peer);

    if (!d_responseHeaders.get()) {
        return;
    }

    network::ResourceResponseInfo responseInfo;
    responseInfo.headers = d_responseHeaders;
    responseInfo.content_length = d_responseHeaders->GetContentLength();
    d_responseHeaders->GetMimeTypeAndCharset(&responseInfo.mime_type,
                                             &responseInfo.charset);
    d_responseHeaders = 0;

    if (responseInfo.mime_type.empty() && length > 0) {
        net::SniffMimeType(buffer,
                           std::min(length, net::kMaxBytesToSniff),
                           d_url,
                           "",
                           net::ForceSniffFileUrlsForHtml::kDisabled,
                           &responseInfo.mime_type);
    }

    d_peer->OnReceivedResponse(responseInfo);
}


// InProcessResourceLoaderBridge

InProcessResourceLoaderBridge::InProcessResourceLoaderBridge(
    const network::ResourceRequest *request):
    d_context(base::MakeRefCounted<InProcessResourceContext>(request))
{
    DCHECK(Statics::isInApplicationMainThread());
    DCHECK(Statics::inProcessResourceLoader);
}

InProcessResourceLoaderBridge::~InProcessResourceLoaderBridge()
{
    d_context->dispose();
    d_context->Release();
}

// content::ResourceLoaderBridge overrides

bool InProcessResourceLoaderBridge::Start(content::RequestPeer* peer)
{
    DCHECK(Statics::isInApplicationMainThread());
    return d_context->start(peer);
}

void InProcessResourceLoaderBridge::Cancel()
{
    DCHECK(Statics::isInApplicationMainThread());
    d_context->cancel();
}

void InProcessResourceLoaderBridge::SyncLoad(content::SyncLoadResponse* response)
{
    LOG(ERROR) << "Synchronous requests not supported: url("
               << d_context->url() << ")";
    response->error_code = net::ERR_FAILED;
}


}  // close namespace blpwtk2
