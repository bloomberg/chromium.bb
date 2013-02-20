// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains an implementation of the ResourceLoaderBridge class.
// The class is implemented using net::URLRequest, meaning it is a "simple"
// version that directly issues requests. The more complicated one used in the
// browser uses IPC.
//
// Because net::URLRequest only provides an asynchronous resource loading API,
// this file makes use of net::URLRequest from a background IO thread. Requests
// for cookies and synchronously loaded resources result in the main thread of
// the application blocking until the IO thread completes the operation.  (See
// GetCookies and SyncLoad)
//
// Main thread                          IO thread
// -----------                          ---------
// ResourceLoaderBridge <---o---------> RequestProxy (normal case)
//                           \            -> net::URLRequest
//                            o-------> SyncRequestProxy (synchronous case)
//                                        -> net::URLRequest
// SetCookie <------------------------> CookieSetter
//                                        -> net_util::SetCookie
// GetCookies <-----------------------> CookieGetter
//                                        -> net_util::GetCookies
//
// NOTE: The implementation in this file may be used to have WebKit fetch
// resources in-process.  For example, it is handy for building a single-
// process WebKit embedding (e.g., test_shell) that can use net::URLRequest to
// perform URL loads.  See renderer/resource_dispatcher.h for details on an
// alternate implementation that defers fetching to another process.

#include "webkit/tools/test_shell/simple_resource_loader_bridge.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/string_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "base/time.h"
#include "base/timer.h"
#include "net/base/file_stream.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/base/mime_util.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/base/network_delegate.h"
#include "net/base/static_cookie_policy.h"
#include "net/base/upload_data_stream.h"
#include "net/cookies/cookie_store.h"
#include "net/http/http_cache.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_job.h"
#include "webkit/appcache/appcache_interfaces.h"
#include "webkit/blob/blob_storage_controller.h"
#include "webkit/blob/blob_url_request_job.h"
#include "webkit/blob/shareable_file_reference.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_dir_url_request_job.h"
#include "webkit/fileapi/file_system_url_request_job.h"
#include "webkit/glue/resource_loader_bridge.h"
#include "webkit/glue/resource_request_body.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/tools/test_shell/simple_appcache_system.h"
#include "webkit/tools/test_shell/simple_file_system.h"
#include "webkit/tools/test_shell/simple_file_writer.h"
#include "webkit/tools/test_shell/simple_socket_stream_bridge.h"
#include "webkit/tools/test_shell/test_shell_request_context.h"
#include "webkit/tools/test_shell/test_shell_webblobregistry_impl.h"

#if defined(OS_MACOSX) || defined(OS_WIN)
#include "crypto/nss_util.h"
#endif

using webkit_glue::ResourceLoaderBridge;
using webkit_glue::ResourceRequestBody;
using webkit_glue::ResourceResponseInfo;
using net::StaticCookiePolicy;
using net::HttpResponseHeaders;
using webkit_blob::ShareableFileReference;

namespace {

struct TestShellRequestContextParams {
  TestShellRequestContextParams(
      const base::FilePath& in_cache_path,
      net::HttpCache::Mode in_cache_mode,
      bool in_no_proxy)
      : cache_path(in_cache_path),
        cache_mode(in_cache_mode),
        no_proxy(in_no_proxy) {}

  base::FilePath cache_path;
  net::HttpCache::Mode cache_mode;
  bool no_proxy;
};

//-----------------------------------------------------------------------------

bool g_accept_all_cookies = false;

class TestShellNetworkDelegate : public net::NetworkDelegate {
 public:
  virtual ~TestShellNetworkDelegate() {}

 protected:
  // net::NetworkDelegate implementation.
  virtual int OnBeforeURLRequest(net::URLRequest* request,
                                 const net::CompletionCallback& callback,
                                 GURL* new_url) OVERRIDE {
    return net::OK;
  }
  virtual int OnBeforeSendHeaders(net::URLRequest* request,
                                  const net::CompletionCallback& callback,
                                  net::HttpRequestHeaders* headers) OVERRIDE {
    return net::OK;
  }
  virtual void OnSendHeaders(net::URLRequest* request,
                             const net::HttpRequestHeaders& headers) OVERRIDE {}
  virtual int OnHeadersReceived(
      net::URLRequest* request,
      const net::CompletionCallback& callback,
      const net::HttpResponseHeaders* original_response_headers,
      scoped_refptr<net::HttpResponseHeaders>*
          override_response_headers) OVERRIDE {
    return net::OK;
  }
  virtual void OnBeforeRedirect(net::URLRequest* request,
                                const GURL& new_location) OVERRIDE {}
  virtual void OnResponseStarted(net::URLRequest* request) OVERRIDE {}
  virtual void OnRawBytesRead(const net::URLRequest& request,
                              int bytes_read) OVERRIDE {}
  virtual void OnCompleted(net::URLRequest* request, bool started) OVERRIDE {}
  virtual void OnURLRequestDestroyed(net::URLRequest* request) OVERRIDE {}

  virtual void OnPACScriptError(int line_number,
                                const string16& error) OVERRIDE {
  }
  virtual AuthRequiredResponse OnAuthRequired(
      net::URLRequest* request,
      const net::AuthChallengeInfo& auth_info,
      const AuthCallback& callback,
      net::AuthCredentials* credentials) OVERRIDE {
    return AUTH_REQUIRED_RESPONSE_NO_ACTION;
  }
  virtual bool OnCanGetCookies(const net::URLRequest& request,
                               const net::CookieList& cookie_list) OVERRIDE {
    StaticCookiePolicy::Type policy_type = g_accept_all_cookies ?
        StaticCookiePolicy::ALLOW_ALL_COOKIES :
        StaticCookiePolicy::BLOCK_SETTING_THIRD_PARTY_COOKIES;

    StaticCookiePolicy policy(policy_type);
    int rv = policy.CanGetCookies(
        request.url(), request.first_party_for_cookies());
    return rv == net::OK;
  }
  virtual bool OnCanSetCookie(const net::URLRequest& request,
                              const std::string& cookie_line,
                              net::CookieOptions* options) OVERRIDE {
    StaticCookiePolicy::Type policy_type = g_accept_all_cookies ?
        StaticCookiePolicy::ALLOW_ALL_COOKIES :
        StaticCookiePolicy::BLOCK_SETTING_THIRD_PARTY_COOKIES;

    StaticCookiePolicy policy(policy_type);
    int rv = policy.CanSetCookie(
        request.url(), request.first_party_for_cookies());
    return rv == net::OK;
  }
  virtual bool OnCanAccessFile(const net::URLRequest& request,
                               const base::FilePath& path) const OVERRIDE {
    return true;
  }
  virtual bool OnCanThrottleRequest(
      const net::URLRequest& request) const OVERRIDE {
    return false;
  }

  virtual int OnBeforeSocketStreamConnect(
      net::SocketStream* stream,
      const net::CompletionCallback& callback) OVERRIDE {
    return net::OK;
  }

  virtual void OnRequestWaitStateChange(const net::URLRequest& request,
                                        RequestWaitState state) OVERRIDE {
  }
};

TestShellRequestContextParams* g_request_context_params = NULL;
TestShellRequestContext* g_request_context = NULL;
TestShellNetworkDelegate* g_network_delegate = NULL;
base::Thread* g_cache_thread = NULL;

//-----------------------------------------------------------------------------

struct FileOverHTTPParams {
  FileOverHTTPParams(std::string in_file_path_template, GURL in_http_prefix)
      : file_path_template(in_file_path_template),
        http_prefix(in_http_prefix) {}

  std::string file_path_template;
  GURL http_prefix;
};

class FileOverHTTPPathMappings {
 public:
  FileOverHTTPPathMappings() : redirections_() {}
  void AddMapping(std::string file_path_template, GURL http_prefix) {
    redirections_.push_back(FileOverHTTPParams(file_path_template,
                                               http_prefix));
  }

  const FileOverHTTPParams* ParamsForRequest(std::string request,
                                             std::string::size_type& offset) {
    std::vector<FileOverHTTPParams>::iterator it;
    for (it = redirections_.begin(); it != redirections_.end(); ++it) {
      offset = request.find(it->file_path_template);
      if (offset != std::string::npos)
        return &*it;
    }
    return 0;
  }

  const FileOverHTTPParams* ParamsForResponse(std::string response_url) {
    std::vector<FileOverHTTPParams>::iterator it;
    for (it = redirections_.begin(); it != redirections_.end(); ++it) {
      if (response_url.find(it->http_prefix.spec()) == 0)
        return &*it;
    }
    return 0;
  }

 private:
  std::vector<FileOverHTTPParams> redirections_;
};

FileOverHTTPPathMappings* g_file_over_http_mappings = NULL;

//-----------------------------------------------------------------------------

class IOThread : public base::Thread {
 public:
  IOThread() : base::Thread("IOThread") {}

  virtual ~IOThread() {
    Stop();
  }

  virtual void Init() OVERRIDE {
    if (g_request_context_params) {
      g_request_context = new TestShellRequestContext(
          g_request_context_params->cache_path,
          g_request_context_params->cache_mode,
          g_request_context_params->no_proxy);
      delete g_request_context_params;
      g_request_context_params = NULL;
    } else {
      g_request_context = new TestShellRequestContext();
    }

    g_network_delegate = new TestShellNetworkDelegate();
    g_request_context->set_network_delegate(g_network_delegate);

    SimpleAppCacheSystem::InitializeOnIOThread(g_request_context);
    SimpleSocketStreamBridge::InitializeOnIOThread(g_request_context);
    SimpleFileWriter::InitializeOnIOThread(g_request_context);
    SimpleFileSystem::InitializeOnIOThread(
        g_request_context->blob_storage_controller());
    TestShellWebBlobRegistryImpl::InitializeOnIOThread(
        g_request_context->blob_storage_controller());
  }

  virtual void CleanUp() OVERRIDE {
    // In reverse order of initialization.
    TestShellWebBlobRegistryImpl::Cleanup();
    SimpleFileSystem::CleanupOnIOThread();
    SimpleFileWriter::CleanupOnIOThread();
    SimpleSocketStreamBridge::Cleanup();
    SimpleAppCacheSystem::CleanupOnIOThread();

    if (g_request_context) {
      g_request_context->set_network_delegate(NULL);
      delete g_request_context;
      g_request_context = NULL;
    }

    if (g_network_delegate) {
      delete g_network_delegate;
      g_network_delegate = NULL;
    }
  }
};

IOThread* g_io_thread = NULL;

//-----------------------------------------------------------------------------

struct RequestParams {
  std::string method;
  GURL url;
  GURL first_party_for_cookies;
  GURL referrer;
  WebKit::WebReferrerPolicy referrer_policy;
  std::string headers;
  int load_flags;
  ResourceType::Type request_type;
  int appcache_host_id;
  bool download_to_file;
  scoped_refptr<ResourceRequestBody> request_body;
};

// The interval for calls to RequestProxy::MaybeUpdateUploadProgress
static const int kUpdateUploadProgressIntervalMsec = 100;

// The RequestProxy does most of its work on the IO thread.  The Start and
// Cancel methods are proxied over to the IO thread, where an net::URLRequest
// object is instantiated.
struct DeleteOnIOThread;  // See below.
class RequestProxy
    : public net::URLRequest::Delegate,
      public base::RefCountedThreadSafe<RequestProxy, DeleteOnIOThread> {
 public:
  // Takes ownership of the params.
  RequestProxy()
      : download_to_file_(false),
        buf_(new net::IOBuffer(kDataSize)),
        last_upload_position_(0) {
  }

  void DropPeer() {
    peer_ = NULL;
  }

  void Start(ResourceLoaderBridge::Peer* peer, RequestParams* params) {
    peer_ = peer;
    owner_loop_ = MessageLoop::current();

    ConvertRequestParamsForFileOverHTTPIfNeeded(params);
    // proxy over to the io thread
    g_io_thread->message_loop()->PostTask(
        FROM_HERE,
        base::Bind(&RequestProxy::AsyncStart, this, params));
  }

  void Cancel() {
    // proxy over to the io thread
    g_io_thread->message_loop()->PostTask(
        FROM_HERE,
        base::Bind(&RequestProxy::AsyncCancel, this));
  }

 protected:
  friend class base::DeleteHelper<RequestProxy>;
  friend class base::RefCountedThreadSafe<RequestProxy>;
  friend struct DeleteOnIOThread;

  virtual ~RequestProxy() {
    // Ensure we are deleted on the IO thread because base::Timer requires that.
    // (guaranteed by the Traits class template parameter).
    DCHECK(MessageLoop::current() == g_io_thread->message_loop());
  }

  // --------------------------------------------------------------------------
  // The following methods are called on the owner's thread in response to
  // various net::URLRequest callbacks.  The event hooks, defined below, trigger
  // these methods asynchronously.

  void NotifyReceivedRedirect(const GURL& new_url,
                              const ResourceResponseInfo& info) {
    bool has_new_first_party_for_cookies = false;
    GURL new_first_party_for_cookies;
    if (peer_ && peer_->OnReceivedRedirect(new_url, info,
                                           &has_new_first_party_for_cookies,
                                           &new_first_party_for_cookies)) {
      g_io_thread->message_loop()->PostTask(
          FROM_HERE,
          base::Bind(&RequestProxy::AsyncFollowDeferredRedirect, this,
                     has_new_first_party_for_cookies,
                     new_first_party_for_cookies));
    } else {
      Cancel();
    }
  }

  void NotifyReceivedResponse(const ResourceResponseInfo& info) {
    if (peer_)
      peer_->OnReceivedResponse(info);
  }

  void NotifyReceivedData(int bytes_read) {
    if (!peer_)
      return;

    // Make a local copy of buf_, since AsyncReadData reuses it.
    scoped_array<char> buf_copy(new char[bytes_read]);
    memcpy(buf_copy.get(), buf_->data(), bytes_read);

    // Continue reading more data into buf_
    // Note: Doing this before notifying our peer ensures our load events get
    // dispatched in a manner consistent with DumpRenderTree (and also avoids a
    // race condition).  If the order of the next 2 functions were reversed, the
    // peer could generate new requests in reponse to the received data, which
    // when run on the io thread, could race against this function in doing
    // another InvokeLater.  See bug 769249.
    g_io_thread->message_loop()->PostTask(
        FROM_HERE,
        base::Bind(&RequestProxy::AsyncReadData, this));

    peer_->OnReceivedData(buf_copy.get(), bytes_read, -1);
  }

  void NotifyDownloadedData(int bytes_read) {
    if (!peer_)
      return;

    // Continue reading more data, see the comment in NotifyReceivedData.
    g_io_thread->message_loop()->PostTask(
        FROM_HERE,
        base::Bind(&RequestProxy::AsyncReadData, this));

    peer_->OnDownloadedData(bytes_read);
  }

  void NotifyCompletedRequest(int error_code,
                              const std::string& security_info,
                              const base::TimeTicks& complete_time) {
    if (peer_) {
      peer_->OnCompletedRequest(error_code, false, security_info,
                                complete_time);
      DropPeer();  // ensure no further notifications
    }
  }

  void NotifyUploadProgress(uint64 position, uint64 size) {
    if (peer_)
      peer_->OnUploadProgress(position, size);
  }

  // --------------------------------------------------------------------------
  // The following methods are called on the io thread.  They correspond to
  // actions performed on the owner's thread.

  void AsyncStart(RequestParams* params) {
    request_.reset(g_request_context->CreateRequest(params->url, this));
    request_->set_method(params->method);
    request_->set_first_party_for_cookies(params->first_party_for_cookies);
    request_->set_referrer(params->referrer.spec());
    webkit_glue::ConfigureURLRequestForReferrerPolicy(
        request_.get(), params->referrer_policy);
    net::HttpRequestHeaders headers;
    headers.AddHeadersFromString(params->headers);
    request_->SetExtraRequestHeaders(headers);
    request_->set_load_flags(params->load_flags);
    if (params->request_body) {
      request_->set_upload(make_scoped_ptr(
          params->request_body->ResolveElementsAndCreateUploadDataStream(
              static_cast<TestShellRequestContext*>(g_request_context)->
              blob_storage_controller(),
              static_cast<TestShellRequestContext*>(g_request_context)->
              file_system_context(),
              base::MessageLoopProxy::current())));
    }
    SimpleAppCacheSystem::SetExtraRequestInfo(
        request_.get(), params->appcache_host_id, params->request_type);

    download_to_file_ = params->download_to_file;
    if (download_to_file_) {
      base::FilePath path;
      if (file_util::CreateTemporaryFile(&path)) {
        downloaded_file_ = ShareableFileReference::GetOrCreate(
            path, ShareableFileReference::DELETE_ON_FINAL_RELEASE,
            base::MessageLoopProxy::current());
        file_stream_.reset(new net::FileStream(NULL));
        file_stream_->OpenSync(
            path, base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_WRITE);
      }
    }

    request_->Start();

    if (request_->has_upload() &&
        params->load_flags & net::LOAD_ENABLE_UPLOAD_PROGRESS) {
      upload_progress_timer_.Start(FROM_HERE,
          base::TimeDelta::FromMilliseconds(kUpdateUploadProgressIntervalMsec),
          this, &RequestProxy::MaybeUpdateUploadProgress);
    }

    delete params;
  }

  void AsyncCancel() {
    // This can be null in cases where the request is already done.
    if (!request_.get())
      return;

    request_->Cancel();
    Done();
  }

  void AsyncFollowDeferredRedirect(bool has_new_first_party_for_cookies,
                                   const GURL& new_first_party_for_cookies) {
    // This can be null in cases where the request is already done.
    if (!request_.get())
      return;

    if (has_new_first_party_for_cookies)
      request_->set_first_party_for_cookies(new_first_party_for_cookies);
    request_->FollowDeferredRedirect();
  }

  void AsyncReadData() {
    // This can be null in cases where the request is already done.
    if (!request_.get())
      return;

    if (request_->status().is_success()) {
      int bytes_read;
      if (request_->Read(buf_, kDataSize, &bytes_read) && bytes_read) {
        OnReceivedData(bytes_read);
      } else if (!request_->status().is_io_pending()) {
        Done();
      }  // else wait for OnReadCompleted
    } else {
      Done();
    }
  }

  // --------------------------------------------------------------------------
  // The following methods are event hooks (corresponding to net::URLRequest
  // callbacks) that run on the IO thread.  They are designed to be overridden
  // by the SyncRequestProxy subclass.

  virtual void OnReceivedRedirect(
      const GURL& new_url,
      const ResourceResponseInfo& info,
      bool* defer_redirect) {
    *defer_redirect = true;  // See AsyncFollowDeferredRedirect
    owner_loop_->PostTask(
        FROM_HERE,
        base::Bind(&RequestProxy::NotifyReceivedRedirect, this, new_url, info));
  }

  virtual void OnReceivedResponse(
      const ResourceResponseInfo& info) {
    owner_loop_->PostTask(
        FROM_HERE,
        base::Bind(&RequestProxy::NotifyReceivedResponse, this, info));
  }

  virtual void OnReceivedData(int bytes_read) {
    if (download_to_file_) {
      file_stream_->WriteSync(buf_->data(), bytes_read);
      owner_loop_->PostTask(
          FROM_HERE,
          base::Bind(&RequestProxy::NotifyDownloadedData, this, bytes_read));
      return;
    }

    owner_loop_->PostTask(
        FROM_HERE,
        base::Bind(&RequestProxy::NotifyReceivedData, this, bytes_read));
  }

  virtual void OnCompletedRequest(int error_code,
                                  const std::string& security_info,
                                  const base::TimeTicks& complete_time) {
    if (download_to_file_)
      file_stream_.reset();
    owner_loop_->PostTask(
        FROM_HERE,
        base::Bind(&RequestProxy::NotifyCompletedRequest, this, error_code,
                   security_info, complete_time));
  }

  // --------------------------------------------------------------------------
  // net::URLRequest::Delegate implementation:

  virtual void OnReceivedRedirect(net::URLRequest* request,
                                  const GURL& new_url,
                                  bool* defer_redirect) OVERRIDE {
    DCHECK(request->status().is_success());
    ResourceResponseInfo info;
    PopulateResponseInfo(request, &info);
    // For file protocol, should never have the redirect situation.
    DCHECK(!ConvertResponseInfoForFileOverHTTPIfNeeded(request, &info));
    OnReceivedRedirect(new_url, info, defer_redirect);
  }

  virtual void OnResponseStarted(net::URLRequest* request) OVERRIDE {
    if (request->status().is_success()) {
      ResourceResponseInfo info;
      PopulateResponseInfo(request, &info);
      // If encountering error when requesting the file, cancel the request.
      if (ConvertResponseInfoForFileOverHTTPIfNeeded(request, &info) &&
          failed_file_request_status_.get()) {
        AsyncCancel();
      } else {
        OnReceivedResponse(info);
        AsyncReadData();  // start reading
      }
    } else {
      Done();
    }
  }

  virtual void OnSSLCertificateError(net::URLRequest* request,
                                     const net::SSLInfo& ssl_info,
                                     bool fatal) OVERRIDE {
    // Allow all certificate errors.
    request->ContinueDespiteLastError();
  }

  virtual void OnReadCompleted(net::URLRequest* request,
                               int bytes_read) OVERRIDE {
    if (request->status().is_success() && bytes_read > 0) {
      OnReceivedData(bytes_read);
    } else {
      Done();
    }
  }

  // --------------------------------------------------------------------------
  // Helpers and data:

  void Done() {
    if (upload_progress_timer_.IsRunning()) {
      MaybeUpdateUploadProgress();
      upload_progress_timer_.Stop();
    }
    DCHECK(request_.get());
    // If |failed_file_request_status_| is not empty, which means the request
    // was a file request and encountered an error, then we need to use the
    // |failed_file_request_status_|. Otherwise use request_'s status.
    OnCompletedRequest(failed_file_request_status_.get() ?
                       failed_file_request_status_->error() :
                       request_->status().error(),
                       std::string(), base::TimeTicks());
    request_.reset();  // destroy on the io thread
  }

  // Called on the IO thread.
  void MaybeUpdateUploadProgress() {
    // If a redirect is received upload is cancelled in net::URLRequest, we
    // should try to stop the |upload_progress_timer_| timer and return.
    if (!request_->has_upload()) {
      if (upload_progress_timer_.IsRunning())
        upload_progress_timer_.Stop();
      return;
    }

    net::UploadProgress progress = request_->GetUploadProgress();
    if (progress.position() == last_upload_position_)
      return;  // no progress made since last time

    const uint64 kHalfPercentIncrements = 200;
    const base::TimeDelta kOneSecond = base::TimeDelta::FromMilliseconds(1000);

    uint64 amt_since_last = progress.position() - last_upload_position_;
    base::TimeDelta time_since_last = base::TimeTicks::Now() -
                                      last_upload_ticks_;

    bool is_finished = (progress.size() == progress.position());
    bool enough_new_progress = (amt_since_last > (progress.size() /
                                                  kHalfPercentIncrements));
    bool too_much_time_passed = time_since_last > kOneSecond;

    if (is_finished || enough_new_progress || too_much_time_passed) {
      owner_loop_->PostTask(
          FROM_HERE,
          base::Bind(&RequestProxy::NotifyUploadProgress, this,
                     progress.position(), progress.size()));
      last_upload_ticks_ = base::TimeTicks::Now();
      last_upload_position_ = progress.position();
    }
  }

  void PopulateResponseInfo(net::URLRequest* request,
                            ResourceResponseInfo* info) const {
    info->request_time = request->request_time();
    info->response_time = request->response_time();
    info->headers = request->response_headers();
    request->GetMimeType(&info->mime_type);
    request->GetCharset(&info->charset);
    info->content_length = request->GetExpectedContentSize();
    if (downloaded_file_)
      info->download_file_path = downloaded_file_->path();
    SimpleAppCacheSystem::GetExtraResponseInfo(
        request,
        &info->appcache_id,
        &info->appcache_manifest_url);
  }

  // Called on owner thread
  void ConvertRequestParamsForFileOverHTTPIfNeeded(RequestParams* params) {
    // Reset the status.
    file_url_prefix_ .clear();
    failed_file_request_status_.reset();
    // Only do this when enabling file-over-http and request is file scheme.
    if (!g_file_over_http_mappings || !params->url.SchemeIsFile())
      return;

    // For file protocol, method must be GET, POST or NULL.
    DCHECK(params->method == "GET" || params->method == "POST" ||
           params->method.empty());
    DCHECK(!params->download_to_file);

    if (params->method.empty())
      params->method = "GET";
    std::string original_request = params->url.spec();

    std::string::size_type offset = 0;
    const FileOverHTTPParams* redirection_params =
        g_file_over_http_mappings->ParamsForRequest(original_request, offset);
    if (!redirection_params)
      return;

    offset += redirection_params->file_path_template.size();
    file_url_prefix_ = original_request.substr(0, offset);
    original_request.replace(0, offset,
        redirection_params->http_prefix.spec());
    params->url = GURL(original_request);
    params->first_party_for_cookies = params->url;
    // For file protocol, nerver use cache.
    params->load_flags = net::LOAD_BYPASS_CACHE;
  }

  // Called on IO thread.
  bool ConvertResponseInfoForFileOverHTTPIfNeeded(net::URLRequest* request,
      ResourceResponseInfo* info) {
    // Only do this when enabling file-over-http and request url
    // matches the http prefix for file-over-http feature.
    if (!g_file_over_http_mappings || file_url_prefix_.empty())
      return false;

    std::string original_request = request->url().spec();
    DCHECK(!original_request.empty());

    const FileOverHTTPParams* redirection_params =
        g_file_over_http_mappings->ParamsForResponse(original_request);
    DCHECK(redirection_params);

    std::string http_prefix = redirection_params->http_prefix.spec();
    DCHECK(StartsWithASCII(original_request, http_prefix, true));

    // Get the File URL.
    original_request.replace(0, http_prefix.size(), file_url_prefix_);

    base::FilePath file_path;
    if (!net::FileURLToFilePath(GURL(original_request), &file_path)) {
      NOTREACHED();
    }

    info->mime_type.clear();
    DCHECK(info->headers);
    int status_code = info->headers->response_code();
    // File protocol does not support response headers.
    info->headers = NULL;
    if (200 == status_code) {
      // Don't use the MIME type from HTTP server, use net::GetMimeTypeFromFile
      // instead.
      net::GetMimeTypeFromFile(file_path, &info->mime_type);
    } else {
      // If the file does not exist, immediately call OnCompletedRequest with
      // setting URLRequestStatus to FAILED.
      DCHECK(status_code == 404 || status_code == 403);
      if (status_code == 404) {
        failed_file_request_status_.reset(
            new net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                      net::ERR_FILE_NOT_FOUND));
      } else {
        failed_file_request_status_.reset(
            new net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                      net::ERR_ACCESS_DENIED));
      }
    }
    return true;
  }

  scoped_ptr<net::URLRequest> request_;

  // Support for request.download_to_file behavior.
  bool download_to_file_;
  scoped_ptr<net::FileStream> file_stream_;
  scoped_refptr<ShareableFileReference> downloaded_file_;

  // Size of our async IO data buffers
  static const int kDataSize = 16*1024;

  // read buffer for async IO
  scoped_refptr<net::IOBuffer> buf_;

  MessageLoop* owner_loop_;

  // This is our peer in WebKit (implemented as ResourceHandleInternal). We do
  // not manage its lifetime, and we may only access it from the owner's
  // message loop (owner_loop_).
  ResourceLoaderBridge::Peer* peer_;

  // Timer used to pull upload progress info.
  base::RepeatingTimer<RequestProxy> upload_progress_timer_;

  // Info used to determine whether or not to send an upload progress update.
  uint64 last_upload_position_;
  base::TimeTicks last_upload_ticks_;

  // Save the real FILE URL prefix for the FILE URL which converts to HTTP URL.
  std::string file_url_prefix_;
  // Save a failed file request status to pass it to webkit.
  scoped_ptr<net::URLRequestStatus> failed_file_request_status_;
};

// Helper guaranteeing deletion on the IO thread (like
// content::BrowserThread::DeleteOnIOThread, but without the dependency).
struct DeleteOnIOThread {
  static void Destruct(const RequestProxy* obj) {
    if (MessageLoop::current() == g_io_thread->message_loop())
      delete obj;
    else
      g_io_thread->message_loop()->DeleteSoon(FROM_HERE, obj);
  }
};

//-----------------------------------------------------------------------------

class SyncRequestProxy : public RequestProxy {
 public:
  explicit SyncRequestProxy(ResourceLoaderBridge::SyncLoadResponse* result)
      : result_(result), event_(true, false) {
  }

  void WaitForCompletion() {
    event_.Wait();
  }

  // --------------------------------------------------------------------------
  // RequestProxy event hooks that run on the IO thread:

  virtual void OnReceivedRedirect(
      const GURL& new_url,
      const ResourceResponseInfo& info,
      bool* defer_redirect) OVERRIDE {
    // TODO(darin): It would be much better if this could live in WebCore, but
    // doing so requires API changes at all levels.  Similar code exists in
    // WebCore/platform/network/cf/ResourceHandleCFNet.cpp :-(
    if (new_url.GetOrigin() != result_->url.GetOrigin()) {
      DLOG(WARNING) << "Cross origin redirect denied";
      Cancel();
      return;
    }
    result_->url = new_url;
  }

  virtual void OnReceivedResponse(const ResourceResponseInfo& info) OVERRIDE {
    *static_cast<ResourceResponseInfo*>(result_) = info;
  }

  virtual void OnReceivedData(int bytes_read) OVERRIDE {
    if (download_to_file_)
      file_stream_->WriteSync(buf_->data(), bytes_read);
    else
      result_->data.append(buf_->data(), bytes_read);
    AsyncReadData();  // read more (may recurse)
  }

  virtual void OnCompletedRequest(
      int error_code,
      const std::string& security_info,
      const base::TimeTicks& complete_time) OVERRIDE {
    if (download_to_file_)
      file_stream_.reset();
    result_->error_code = error_code;
    event_.Signal();
  }

 protected:
  virtual ~SyncRequestProxy() {}

 private:
  ResourceLoaderBridge::SyncLoadResponse* result_;
  base::WaitableEvent event_;
};

//-----------------------------------------------------------------------------

class ResourceLoaderBridgeImpl : public ResourceLoaderBridge {
 public:
  ResourceLoaderBridgeImpl(
      const webkit_glue::ResourceLoaderBridge::RequestInfo& request_info)
      : params_(new RequestParams),
        proxy_(NULL) {
    params_->method = request_info.method;
    params_->url = request_info.url;
    params_->first_party_for_cookies = request_info.first_party_for_cookies;
    params_->referrer = request_info.referrer;
    params_->referrer_policy = request_info.referrer_policy;
    params_->headers = request_info.headers;
    params_->load_flags = request_info.load_flags;
    params_->request_type = request_info.request_type;
    params_->appcache_host_id = request_info.appcache_host_id;
    params_->download_to_file = request_info.download_to_file;
  }

  virtual ~ResourceLoaderBridgeImpl() {
    if (proxy_) {
      proxy_->DropPeer();
      // Let the proxy die on the IO thread
      g_io_thread->message_loop()->ReleaseSoon(FROM_HERE, proxy_);
    }
  }

  // --------------------------------------------------------------------------
  // ResourceLoaderBridge implementation:

  virtual void SetRequestBody(ResourceRequestBody* request_body) OVERRIDE {
    DCHECK(params_.get());
    DCHECK(!params_->request_body);
    params_->request_body = request_body;
  }

  virtual bool Start(Peer* peer) OVERRIDE {
    DCHECK(!proxy_);

    if (!SimpleResourceLoaderBridge::EnsureIOThread())
      return false;

    proxy_ = new RequestProxy();
    proxy_->AddRef();

    proxy_->Start(peer, params_.release());

    return true;  // Any errors will be reported asynchronously.
  }

  virtual void Cancel() OVERRIDE {
    DCHECK(proxy_);
    proxy_->Cancel();
  }

  virtual void SetDefersLoading(bool value) OVERRIDE {
    // TODO(darin): implement me
  }

  virtual void SyncLoad(SyncLoadResponse* response) OVERRIDE {
    DCHECK(!proxy_);

    if (!SimpleResourceLoaderBridge::EnsureIOThread())
      return;

    // this may change as the result of a redirect
    response->url = params_->url;

    proxy_ = new SyncRequestProxy(response);
    proxy_->AddRef();

    proxy_->Start(NULL, params_.release());

    static_cast<SyncRequestProxy*>(proxy_)->WaitForCompletion();
  }

  virtual void DidChangePriority(net::RequestPriority new_priority) {
    // Not really needed for DRT.
  }

 private:
  // Ownership of params_ is transfered to the proxy when the proxy is created.
  scoped_ptr<RequestParams> params_;

  // The request proxy is allocated when we start the request, and then it
  // sticks around until this ResourceLoaderBridge is destroyed.
  RequestProxy* proxy_;
};

//-----------------------------------------------------------------------------

class CookieSetter : public base::RefCountedThreadSafe<CookieSetter> {
 public:
  void Set(const GURL& url, const std::string& cookie) {
    DCHECK(MessageLoop::current() == g_io_thread->message_loop());
    g_request_context->cookie_store()->SetCookieWithOptionsAsync(
        url, cookie, net::CookieOptions(),
        net::CookieStore::SetCookiesCallback());
  }

 private:
  friend class base::RefCountedThreadSafe<CookieSetter>;
  ~CookieSetter() {}
};

class CookieGetter : public base::RefCountedThreadSafe<CookieGetter> {
 public:
  CookieGetter() : event_(false, false) {
  }

  void Get(const GURL& url) {
    g_request_context->cookie_store()->GetCookiesWithOptionsAsync(
        url, net::CookieOptions(),
        base::Bind(&CookieGetter::OnGetCookies, this));
  }

  std::string GetResult() {
    event_.Wait();
    return result_;
  }

 private:
  friend class base::RefCountedThreadSafe<CookieGetter>;
  ~CookieGetter() {}

  void OnGetCookies(const std::string& cookie_line) {
    result_ = cookie_line;
    event_.Signal();
  }

  base::WaitableEvent event_;
  std::string result_;
};

}  // anonymous namespace

//-----------------------------------------------------------------------------

// static
void SimpleResourceLoaderBridge::Init(
    const base::FilePath& cache_path,
    net::HttpCache::Mode cache_mode,
    bool no_proxy) {
  // Make sure to stop any existing IO thread since it may be using the
  // current request context.
  Shutdown();

  DCHECK(!g_request_context_params);
  DCHECK(!g_request_context);
  DCHECK(!g_network_delegate);
  DCHECK(!g_io_thread);

  g_request_context_params = new TestShellRequestContextParams(
      cache_path, cache_mode, no_proxy);
}

// static
void SimpleResourceLoaderBridge::Shutdown() {
  if (g_io_thread) {
    delete g_io_thread;
    g_io_thread = NULL;

    DCHECK(g_cache_thread);
    delete g_cache_thread;
    g_cache_thread = NULL;

    DCHECK(!g_request_context) << "should have been nulled by thread dtor";
    DCHECK(!g_network_delegate) << "should have been nulled by thread dtor";
  } else {
    delete g_request_context_params;
    g_request_context_params = NULL;

    delete g_file_over_http_mappings;
    g_file_over_http_mappings = NULL;
  }
}

// static
void SimpleResourceLoaderBridge::SetCookie(const GURL& url,
                                           const GURL& first_party_for_cookies,
                                           const std::string& cookie) {
  // Proxy to IO thread to synchronize w/ network loading.

  if (!EnsureIOThread()) {
    NOTREACHED();
    return;
  }

  scoped_refptr<CookieSetter> cookie_setter(new CookieSetter());
  g_io_thread->message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&CookieSetter::Set, cookie_setter.get(), url, cookie));
}

// static
std::string SimpleResourceLoaderBridge::GetCookies(
    const GURL& url, const GURL& first_party_for_cookies) {
  // Proxy to IO thread to synchronize w/ network loading

  if (!EnsureIOThread()) {
    NOTREACHED();
    return std::string();
  }

  scoped_refptr<CookieGetter> getter(new CookieGetter());

  g_io_thread->message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&CookieGetter::Get, getter.get(), url));

  return getter->GetResult();
}

// static
bool SimpleResourceLoaderBridge::EnsureIOThread() {
  if (g_io_thread)
    return true;

#if defined(OS_MACOSX) || defined(OS_WIN)
  // We want to be sure to init NSPR on the main thread.
  crypto::EnsureNSPRInit();
#endif

  // Create the cache thread. We want the cache thread to outlive the IO thread,
  // so its lifetime is bonded to the IO thread lifetime.
  DCHECK(!g_cache_thread);
  g_cache_thread = new base::Thread("cache");
  CHECK(g_cache_thread->StartWithOptions(
      base::Thread::Options(MessageLoop::TYPE_IO, 0)));

  g_io_thread = new IOThread();
  base::Thread::Options options;
  options.message_loop_type = MessageLoop::TYPE_IO;
  return g_io_thread->StartWithOptions(options);
}

// static
void SimpleResourceLoaderBridge::SetAcceptAllCookies(bool accept_all_cookies) {
  g_accept_all_cookies = accept_all_cookies;
}

// static
scoped_refptr<base::MessageLoopProxy>
    SimpleResourceLoaderBridge::GetCacheThread() {
  return g_cache_thread->message_loop_proxy();
}

// static
scoped_refptr<base::MessageLoopProxy>
    SimpleResourceLoaderBridge::GetIoThread() {
  if (!EnsureIOThread()) {
    LOG(DFATAL) << "Failed to create IO thread.";
    return NULL;
  }
  return g_io_thread->message_loop_proxy();
}

// static
void SimpleResourceLoaderBridge::AllowFileOverHTTP(
    const std::string& file_path_template, const GURL& http_prefix) {
  DCHECK(!file_path_template.empty());
  DCHECK(http_prefix.is_valid() &&
         (http_prefix.SchemeIs("http") || http_prefix.SchemeIs("https")));
  if (!g_file_over_http_mappings)
    g_file_over_http_mappings = new FileOverHTTPPathMappings();
  g_file_over_http_mappings->AddMapping(file_path_template, http_prefix);
}

// static
webkit_glue::ResourceLoaderBridge* SimpleResourceLoaderBridge::Create(
    const webkit_glue::ResourceLoaderBridge::RequestInfo& request_info) {
  return new ResourceLoaderBridgeImpl(request_info);
}
