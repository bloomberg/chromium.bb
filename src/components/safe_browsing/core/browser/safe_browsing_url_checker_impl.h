// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_SAFE_BROWSING_URL_CHECKER_IMPL_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_SAFE_BROWSING_URL_CHECKER_IMPL_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/timer/timer.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/safe_browsing/core/common/proto/realtimeapi.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_url_checker.mojom.h"
#include "components/security_interstitials/core/unsafe_resource.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

namespace safe_browsing {

class UrlCheckerDelegate;

class RealTimeUrlLookupServiceBase;

// A SafeBrowsingUrlCheckerImpl instance is used to perform SafeBrowsing check
// for a URL and its redirect URLs. It implements Mojo interface so that it can
// be used to handle queries from renderers. But it is also used to handle
// queries from the browser. In that case, the public methods are called
// directly instead of through Mojo.
//
// To be considered "safe", a URL must not appear in the SafeBrowsing blocklists
// (see SafeBrowsingService for details).
//
// Note that the SafeBrowsing check takes at most kCheckUrlTimeoutMs
// milliseconds. If it takes longer than this, then the system defaults to
// treating the URL as safe.
//
// If the URL is classified as dangerous, a warning interstitial page is
// displayed. In that case, the user can click through the warning page if they
// decides to procced with loading the URL anyway.
class SafeBrowsingUrlCheckerImpl : public mojom::SafeBrowsingUrlChecker,
                                   public SafeBrowsingDatabaseManager::Client {
 public:
  // Interface via which a client of this class can surface relevant events in
  // WebUI. All methods must be called on the UI thread.
  class WebUIDelegate {
   public:
    virtual ~WebUIDelegate() = default;

    // Adds the new ping to the set of RT lookup pings. Returns a token that can
    // be used in |AddToRTLookupResponses| to correlate a ping and response.
    virtual int AddToRTLookupPings(const RTLookupRequest request,
                                   const std::string oauth_token) = 0;

    // Adds the new response to the set of RT lookup pings.
    virtual void AddToRTLookupResponses(int token,
                                        const RTLookupResponse response) = 0;
  };

  using NativeUrlCheckNotifier =
      base::OnceCallback<void(bool /* proceed */,
                              bool /* showed_interstitial */)>;

  // If |slow_check_notifier| is not null, the callback is supposed to update
  // this output parameter with a callback to receive complete notification. In
  // that case, |proceed| and |showed_interstitial| should be ignored.
  using NativeCheckUrlCallback =
      base::OnceCallback<void(NativeUrlCheckNotifier* /* slow_check_notifier */,
                              bool /* proceed */,
                              bool /* showed_interstitial */)>;

  // Constructor for SafeBrowsingUrlCheckerImpl. |real_time_lookup_enabled|
  // indicates whether or not the profile has enabled real time URL lookups, as
  // computed by the RealTimePolicyEngine. This must be computed in advance,
  // since this class only exists on the IO thread.
  // |can_rt_check_subresource_url| indicates whether or not the profile has
  // enabled real time URL lookups for subresource URLs.
  // |real_time_lookup_enabled| must be true if |can_rt_check_subresource_url|
  // is true.
  // |last_committed_url| is used for obtaining the page load token when the URL
  // being checked is not a mainframe URL. Only used when real time lookup is
  // performed.
  // |webui_delegate_| is allowed to be null. If non-null, it must outlive this
  // object.
  SafeBrowsingUrlCheckerImpl(
      const net::HttpRequestHeaders& headers,
      int load_flags,
      network::mojom::RequestDestination request_destination,
      bool has_user_gesture,
      scoped_refptr<UrlCheckerDelegate> url_checker_delegate,
      const base::RepeatingCallback<content::WebContents*()>&
          web_contents_getter,
      security_interstitials::UnsafeResource::RenderProcessId render_process_id,
      security_interstitials::UnsafeResource::RenderFrameId render_frame_id,
      security_interstitials::UnsafeResource::FrameTreeNodeId
          frame_tree_node_id,
      bool real_time_lookup_enabled,
      bool can_rt_check_subresource_url,
      bool can_check_db,
      GURL last_committed_url,
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
      base::WeakPtr<RealTimeUrlLookupServiceBase> url_lookup_service_on_ui,
      WebUIDelegate* webui_delegate);

  // Constructor that takes only a RequestDestination, a UrlCheckerDelegate, and
  // real-time lookup-related arguments, omitting other arguments that never
  // have non-default values on iOS.
  // TODO(crbug.com/1103222): Add an iOS-specific WebUIDelegate implementation
  // and pass it here to log RT requests/responses on open
  // chrome://safe-browsing pages once chrome://safe-browsing works on iOS, or
  // else to log those requests/responses to stderr.
  SafeBrowsingUrlCheckerImpl(
      network::mojom::RequestDestination request_destination,
      scoped_refptr<UrlCheckerDelegate> url_checker_delegate,
      const base::RepeatingCallback<web::WebState*()>& web_state_getter,
      bool real_time_lookup_enabled,
      bool can_rt_check_subresource_url,
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
      base::WeakPtr<RealTimeUrlLookupServiceBase> url_lookup_service_on_ui);

  SafeBrowsingUrlCheckerImpl(const SafeBrowsingUrlCheckerImpl&) = delete;
  SafeBrowsingUrlCheckerImpl& operator=(const SafeBrowsingUrlCheckerImpl&) =
      delete;

  ~SafeBrowsingUrlCheckerImpl() override;

  // mojom::SafeBrowsingUrlChecker implementation.
  // NOTE: |callback| could be run synchronously before this method returns. Be
  // careful if |callback| could destroy this object.
  void CheckUrl(const GURL& url,
                const std::string& method,
                CheckUrlCallback callback) override;

  // NOTE: |callback| could be run synchronously before this method returns. Be
  // careful if |callback| could destroy this object.
  virtual void CheckUrl(const GURL& url,
                        const std::string& method,
                        NativeCheckUrlCallback callback);

 private:
  class Notifier {
   public:
    explicit Notifier(CheckUrlCallback callback);
    explicit Notifier(NativeCheckUrlCallback native_callback);

    ~Notifier();

    Notifier(Notifier&& other);
    Notifier& operator=(Notifier&& other);

    void OnStartSlowCheck();
    void OnCompleteCheck(bool proceed, bool showed_interstitial);

   private:
    // Used in the mojo interface case.
    CheckUrlCallback callback_;
    mojo::Remote<mojom::UrlCheckNotifier> slow_check_notifier_;

    // Used in the native call case.
    NativeCheckUrlCallback native_callback_;
    NativeUrlCheckNotifier native_slow_check_notifier_;
  };

  // SafeBrowsingDatabaseManager::Client implementation:
  void OnCheckBrowseUrlResult(const GURL& url,
                              SBThreatType threat_type,
                              const ThreatMetadata& metadata) override;
  void OnCheckUrlForHighConfidenceAllowlist(bool did_match_allowlist) override;

  void OnTimeout();

  void OnUrlResult(const GURL& url,
                   SBThreatType threat_type,
                   const ThreatMetadata& metadata,
                   bool is_from_real_time_check);

  void CheckUrlImpl(const GURL& url,
                    const std::string& method,
                    Notifier notifier);

  // NOTE: this method runs callbacks which could destroy this object.
  void ProcessUrls();

  // NOTE: this method runs callbacks which could destroy this object.
  void BlockAndProcessUrls(bool showed_interstitial);

  void OnBlockingPageComplete(bool proceed, bool showed_interstitial);

  // Helper method that checks whether |url|'s reputation can be checked using
  // real time lookups.
  bool CanPerformFullURLLookup(const GURL& url);

  SBThreatType CheckWebUIUrls(const GURL& url);

  // Returns false if this object has been destroyed by the callback. In that
  // case none of the members of this object should be touched again.
  bool RunNextCallback(bool proceed, bool showed_interstitial);

  // Perform the hash based check for the url.
  void PerformHashBasedCheck(const GURL& url);

  // This function has to be static because it is called in UI thread.
  // This function starts a real time url check if |url_lookup_service_on_ui| is
  // available and is not in backoff mode. Otherwise, hop back to IO thread and
  // perform hash based check.
  static void StartLookupOnUIThread(
      base::WeakPtr<SafeBrowsingUrlCheckerImpl> weak_checker_on_io,
      const GURL& url,
      const GURL& last_committed_url,
      bool is_mainframe,
      base::WeakPtr<RealTimeUrlLookupServiceBase> url_lookup_service_on_ui,
      scoped_refptr<SafeBrowsingDatabaseManager> database_manager,
      scoped_refptr<base::SequencedTaskRunner> io_task_runner);

  // Called when the |request| from the real-time lookup service is sent.
  void OnRTLookupRequest(std::unique_ptr<RTLookupRequest> request,
                         std::string oauth_token);

  // Called when the |response| from the real-time lookup service is received.
  // |is_rt_lookup_successful| is true if the response code is OK and the
  // response body is successfully parsed.
  // |is_cached_response| is true if the response is a cache hit. In such a
  // case, fall back to hash-based checks if the cached verdict is |SAFE|.
  void OnRTLookupResponse(bool is_rt_lookup_successful,
                          bool is_cached_response,
                          std::unique_ptr<RTLookupResponse> response);

  // Logs |request| on any open chrome://safe-browsing pages.
  void LogRTLookupRequest(const RTLookupRequest& request,
                          const std::string& oauth_token);

  // Logs |response| on any open chrome://safe-browsing pages.
  void LogRTLookupResponse(const RTLookupResponse& response);

  void SetWebUIToken(int token);

  security_interstitials::UnsafeResource MakeUnsafeResource(
      const GURL& url,
      SBThreatType threat_type,
      const ThreatMetadata& metadata,
      bool is_from_real_time_check);

  enum State {
    // Haven't started checking or checking is complete.
    STATE_NONE,
    // We have one outstanding URL-check.
    STATE_CHECKING_URL,
    // A warning must be shown, but it's delayed because of the Delayed Warnings
    // experiment.
    STATE_DELAYED_BLOCKING_PAGE,
    // We're displaying a blocking page.
    STATE_DISPLAYING_BLOCKING_PAGE,
    // The blocking page has returned *not* to proceed.
    STATE_BLOCKED
  };

  struct UrlInfo {
    UrlInfo(const GURL& url,
            const std::string& method,
            Notifier notifier,
            bool is_cached_safe_url);
    UrlInfo(UrlInfo&& other);

    ~UrlInfo();

    GURL url;
    std::string method;
    Notifier notifier;
    // If the URL is classified as safe in cache manager during real time
    // lookup.
    bool is_cached_safe_url;
  };

  SEQUENCE_CHECKER(sequence_checker_);
  const net::HttpRequestHeaders headers_;
  const int load_flags_;
  const network::mojom::RequestDestination request_destination_;
  const bool has_user_gesture_;
  // TODO(crbug.com/1069047): |web_state_getter| is only used on iOS, and
  // |web_contents_getter_|, |render_process_id_|, |render_frame_id_|, and
  // |frame_tree_node_id_| are used on all other platforms.  This class should
  // be refactored to use only the common functionality can be shared across
  // platforms.
  base::RepeatingCallback<content::WebContents*()> web_contents_getter_;
  const security_interstitials::UnsafeResource::RenderProcessId
      render_process_id_ =
          security_interstitials::UnsafeResource::kNoRenderProcessId;
  const security_interstitials::UnsafeResource::RenderFrameId render_frame_id_ =
      security_interstitials::UnsafeResource::kNoRenderFrameId;
  const security_interstitials::UnsafeResource::FrameTreeNodeId
      frame_tree_node_id_ =
          security_interstitials::UnsafeResource::kNoFrameTreeNodeId;
  base::RepeatingCallback<web::WebState*()> web_state_getter_;
  scoped_refptr<UrlCheckerDelegate> url_checker_delegate_;
  scoped_refptr<SafeBrowsingDatabaseManager> database_manager_;

  // The redirect chain for this resource, including the original URL and
  // subsequent redirect URLs.
  std::vector<UrlInfo> urls_;
  // |urls_| before |next_index_| have been checked. If |next_index_| is smaller
  // than the size of |urls_|, the URL at |next_index_| is being processed.
  size_t next_index_ = 0;

  // Token used for displaying url real time lookup pings. A single token is
  // sufficient since real time check only happens on main frame url.
  int url_web_ui_token_ = -1;

  State state_ = STATE_NONE;

  // Timer to abort the SafeBrowsing check if it takes too long.
  base::OneShotTimer timer_;

  // Whether real time lookup is enabled for this request.
  bool real_time_lookup_enabled_;

  // Whether non mainframe url can be checked for this profile.
  bool can_rt_check_subresource_url_;

  // Whether safe browsing database can be checked. It is set to false when
  // enterprise real time URL lookup is enabled and safe browsing is disabled
  // for this profile.
  bool can_check_db_;

  // The last committed URL when the checker is constructed. It is used to
  // obtain page load token when the URL being checked is not a mainframe URL.
  // Only used when real time lookup is performed.
  GURL last_committed_url_;

  // The task runner for the UI thread.
  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;

  // This object is used to perform real time url check. Can only be accessed in
  // UI thread.
  base::WeakPtr<RealTimeUrlLookupServiceBase> url_lookup_service_on_ui_;

  // May be null on certain platforms that don't support chrome://safe-browsing
  // and in unit tests. If non-null, guaranteed to outlive this object by
  // contract.
  raw_ptr<WebUIDelegate> webui_delegate_ = nullptr;

  base::WeakPtrFactory<SafeBrowsingUrlCheckerImpl> weak_factory_{this};
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_SAFE_BROWSING_URL_CHECKER_IMPL_H_
