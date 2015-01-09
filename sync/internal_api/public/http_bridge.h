// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_HTTP_BRIDGE_H_
#define SYNC_INTERNAL_API_PUBLIC_HTTP_BRIDGE_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/timer/timer.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "sync/base/sync_export.h"
#include "sync/internal_api/public/base/cancelation_observer.h"
#include "sync/internal_api/public/http_post_provider_factory.h"
#include "sync/internal_api/public/http_post_provider_interface.h"
#include "sync/internal_api/public/network_time_update_callback.h"
#include "url/gurl.h"

class HttpBridgeTest;

namespace base {
class MessageLoop;
}

namespace net {
class HttpResponseHeaders;
class HttpUserAgentSettings;
class URLFetcher;
class URLRequestJobFactory;
}

namespace syncer {

class CancelationSignal;

// A bridge between the syncer and Chromium HTTP layers.
// Provides a way for the sync backend to use Chromium directly for HTTP
// requests rather than depending on a third party provider (e.g libcurl).
// This is a one-time use bridge. Create one for each request you want to make.
// It is RefCountedThreadSafe because it can PostTask to the io loop, and thus
// needs to stick around across context switches, etc.
class SYNC_EXPORT_PRIVATE HttpBridge
    : public base::RefCountedThreadSafe<HttpBridge>,
      public HttpPostProviderInterface,
      public net::URLFetcherDelegate {
 public:
  // A request context used for HTTP requests bridged from the sync backend.
  // A bridged RequestContext has a dedicated in-memory cookie store and does
  // not use a cache. Thus the same type can be used for incognito mode.
  class RequestContext : public net::URLRequestContext {
   public:
    // |baseline_context| is used to obtain the accept-language
    // and proxy service information for bridged requests.
    // Typically |baseline_context| should be the net::URLRequestContext of the
    // currently active profile.
    RequestContext(
        net::URLRequestContext* baseline_context,
        const scoped_refptr<base::SingleThreadTaskRunner>&
            network_task_runner,
        const std::string& user_agent);

    // The destructor MUST be called on the IO thread.
    ~RequestContext() override;

   private:
    const scoped_refptr<base::SingleThreadTaskRunner> network_task_runner_;
    scoped_ptr<net::HttpUserAgentSettings> http_user_agent_settings_;
    scoped_ptr<net::URLRequestJobFactory> job_factory_;

    DISALLOW_COPY_AND_ASSIGN(RequestContext);
  };

  // Lazy-getter for RequestContext objects.
  class SYNC_EXPORT_PRIVATE RequestContextGetter
      : public net::URLRequestContextGetter {
   public:
    RequestContextGetter(
        net::URLRequestContextGetter* baseline_context_getter,
        const std::string& user_agent);

    // net::URLRequestContextGetter implementation.
    net::URLRequestContext* GetURLRequestContext() override;
    scoped_refptr<base::SingleThreadTaskRunner> GetNetworkTaskRunner()
        const override;

   protected:
    ~RequestContextGetter() override;

   private:
    scoped_refptr<net::URLRequestContextGetter> baseline_context_getter_;
    const scoped_refptr<base::SingleThreadTaskRunner> network_task_runner_;
    // User agent to apply to the net::URLRequestContext.
    const std::string user_agent_;

    // Lazily initialized by GetURLRequestContext().
    scoped_ptr<RequestContext> context_;

    DISALLOW_COPY_AND_ASSIGN(RequestContextGetter);
  };

  HttpBridge(RequestContextGetter* context,
             const NetworkTimeUpdateCallback& network_time_update_callback);

  // HttpPostProvider implementation.
  void SetExtraRequestHeaders(const char* headers) override;
  void SetURL(const char* url, int port) override;
  void SetPostPayload(const char* content_type,
                      int content_length,
                      const char* content) override;
  bool MakeSynchronousPost(int* error_code, int* response_code) override;
  void Abort() override;

  // WARNING: these response content methods are used to extract plain old data
  // and not null terminated strings, so you should make sure you have read
  // GetResponseContentLength() characters when using GetResponseContent. e.g
  // string r(b->GetResponseContent(), b->GetResponseContentLength()).
  int GetResponseContentLength() const override;
  const char* GetResponseContent() const override;
  const std::string GetResponseHeaderValue(
      const std::string& name) const override;

  // net::URLFetcherDelegate implementation.
  void OnURLFetchComplete(const net::URLFetcher* source) override;
  void OnURLFetchDownloadProgress(const net::URLFetcher* source,
                                  int64 current, int64 total) override;
  void OnURLFetchUploadProgress(const net::URLFetcher* source,
                                int64 current, int64 total) override;

  net::URLRequestContextGetter* GetRequestContextGetterForTest() const;

 protected:
  friend class base::RefCountedThreadSafe<HttpBridge>;

  ~HttpBridge() override;

  // Protected virtual so the unit test can override to shunt network requests.
  virtual void MakeAsynchronousPost();

 private:
  friend class SyncHttpBridgeTest;
  friend class ::HttpBridgeTest;

  // Called on the IO loop to issue the network request. The extra level
  // of indirection is so that the unit test can override this behavior but we
  // still have a function to statically pass to PostTask.
  void CallMakeAsynchronousPost() { MakeAsynchronousPost(); }

  // Used to destroy a fetcher when the bridge is Abort()ed, to ensure that
  // a reference to |this| is held while flushing any pending fetch completion
  // callbacks coming from the IO thread en route to finally destroying the
  // fetcher.
  void DestroyURLFetcherOnIOThread(net::URLFetcher* fetcher,
                                   base::Timer* fetch_timer);

  void UpdateNetworkTime();

  // Helper method to abort the request if we timed out.
  void OnURLFetchTimedOut();

  // The message loop of the thread we were created on. This is the thread that
  // will block on MakeSynchronousPost while the IO thread fetches data from
  // the network.
  // This should be the main syncer thread (SyncerThread) which is what blocks
  // on network IO through curl_easy_perform.
  base::MessageLoop* const created_on_loop_;

  // The URL to POST to.
  GURL url_for_request_;

  // POST payload information.
  std::string content_type_;
  std::string request_content_;
  std::string extra_headers_;

  // A waitable event we use to provide blocking semantics to
  // MakeSynchronousPost. We block created_on_loop_ while the IO loop fetches
  // network request.
  base::WaitableEvent http_post_completed_;

  struct URLFetchState {
    URLFetchState();
    ~URLFetchState();
    // Our hook into the network layer is a URLFetcher. USED ONLY ON THE IO
    // LOOP, so we can block created_on_loop_ while the fetch is in progress.
    // NOTE: This is not a scoped_ptr for a reason. It must be deleted on the
    // same thread that created it, which isn't the same thread |this| gets
    // deleted on. We must manually delete url_poster_ on the IO loop.
    net::URLFetcher* url_poster;

    // Start and finish time of request. Set immediately before sending
    // request and after receiving response.
    base::Time start_time;
    base::Time end_time;

    // Used to support 'Abort' functionality.
    bool aborted;

    // Cached response data.
    bool request_completed;
    bool request_succeeded;
    int http_response_code;
    int error_code;
    std::string response_content;
    scoped_refptr<net::HttpResponseHeaders> response_headers;

    // Timer to ensure http requests aren't stalled. Reset every time upload or
    // download progress is made.
    scoped_ptr<base::Timer> http_request_timeout_timer;
  };

  // This lock synchronizes use of state involved in the flow to fetch a URL
  // using URLFetcher, including |fetch_state_| and
  // |context_getter_for_request_| on any thread, for example, this flow needs
  // to be synchronized to gracefully clean up URLFetcher and return
  // appropriate values in |error_code|.
  mutable base::Lock fetch_state_lock_;
  URLFetchState fetch_state_;

  // Gets a customized net::URLRequestContext for bridged requests. See
  // RequestContext definition for details.
  scoped_refptr<RequestContextGetter> context_getter_for_request_;

  const scoped_refptr<base::SingleThreadTaskRunner> network_task_runner_;

  // Callback for updating network time.
  NetworkTimeUpdateCallback network_time_update_callback_;

  DISALLOW_COPY_AND_ASSIGN(HttpBridge);
};

class SYNC_EXPORT HttpBridgeFactory : public HttpPostProviderFactory,
                                      public CancelationObserver {
 public:
  HttpBridgeFactory(
      const scoped_refptr<net::URLRequestContextGetter>&
          baseline_context_getter,
      const NetworkTimeUpdateCallback& network_time_update_callback,
      CancelationSignal* cancelation_signal);
  ~HttpBridgeFactory() override;

  // HttpPostProviderFactory:
  void Init(const std::string& user_agent) override;
  HttpPostProviderInterface* Create() override;
  void Destroy(HttpPostProviderInterface* http) override;

  // CancelationObserver implementation:
  void OnSignalReceived() override;

 private:
  // Protects |request_context_getter_| and |baseline_request_context_getter_|.
  base::Lock context_getter_lock_;

  // This request context is the starting point for the request_context_getter_
  // that we eventually use to make requests.  During shutdown we must drop all
  // references to it before the ProfileSyncService's Shutdown() call is
  // complete.
  scoped_refptr<net::URLRequestContextGetter> baseline_request_context_getter_;

  // This request context is built on top of the baseline context and shares
  // common components. Takes a reference to the
  // baseline_request_context_getter_.  It's mostly used on sync thread when
  // creating connection but is released as soon as possible during shutdown.
  // Protected by |context_getter_lock_|.
  scoped_refptr<HttpBridge::RequestContextGetter> request_context_getter_;

  NetworkTimeUpdateCallback network_time_update_callback_;

  CancelationSignal* const cancelation_signal_;

  DISALLOW_COPY_AND_ASSIGN(HttpBridgeFactory);
};

}  //  namespace syncer

#endif  // SYNC_INTERNAL_API_PUBLIC_HTTP_BRIDGE_H_
