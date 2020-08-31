// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Helper class which handles communication with the SafeBrowsing backends for
// client-side phishing detection.  This class is used to fetch the client-side
// model and send it to all renderers.  This class is also used to send a ping
// back to Google to verify if a particular site is really phishing or not.
//
// This class is not thread-safe and expects all calls to be made on the UI
// thread.  We also expect that the calling thread runs a message loop.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CLIENT_SIDE_DETECTION_SERVICE_H_
#define CHROME_BROWSER_SAFE_BROWSING_CLIENT_SIDE_DETECTION_SERVICE_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_forward.h"
#include "base/containers/queue.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/safe_browsing/client_side_model_loader.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

class Profile;

namespace content {
class RenderProcessHost;
}

namespace network {
class SimpleURLLoader;
class SharedURLLoaderFactory;
}  // namespace network

namespace safe_browsing {
class ClientPhishingRequest;

// Main service which pushes models to the renderers, responds to classification
// requests. This owns two ModelLoader objects.
class ClientSideDetectionService : public content::NotificationObserver,
                                   public KeyedService {
 public:
  // void(GURL phishing_url, bool is_phishing).
  typedef base::Callback<void(GURL, bool)> ClientReportPhishingRequestCallback;

  explicit ClientSideDetectionService(Profile* profile);

  // Create a ClientSideDetectionService with no associated profile, for tests.
  explicit ClientSideDetectionService(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader);
  ~ClientSideDetectionService() override;

  void Shutdown() override;

  bool enabled() const {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    return enabled_;
  }

  void OnURLLoaderComplete(network::SimpleURLLoader* url_loader,
                           std::unique_ptr<std::string> response_body);

  // content::NotificationObserver overrides:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // Sends a request to the SafeBrowsing servers with the ClientPhishingRequest.
  // The URL scheme of the |url()| in the request should be HTTP.  This method
  // takes ownership of the |verdict| as well as the |callback| and calls the
  // the callback once the result has come back from the server or if an error
  // occurs during the fetch.  |is_extended_reporting| and
  // |is_enhanced_protection| should be set based on the active profile setting.
  // If the service is disabled or an error occurs the phishing verdict will
  // always be false.  The callback is always called after
  // SendClientReportPhishingRequest() returns and on the same thread as
  // SendClientReportPhishingRequest() was called.  You may set |callback| to
  // NULL if you don't care about the server verdict.
  virtual void SendClientReportPhishingRequest(
      ClientPhishingRequest* verdict,
      bool is_extended_reporting,
      bool is_enhanced_protection,
      const ClientReportPhishingRequestCallback& callback);

  // Returns true if the given IP address string falls within a private
  // (unroutable) network block.  Pages which are hosted on these IP addresses
  // are exempt from client-side phishing detection.  This is called by the
  // ClientSideDetectionHost prior to sending the renderer a
  // SafeBrowsingMsg_StartPhishingDetection IPC.
  //
  // ip_address should be a dotted IPv4 address, or an unbracketed IPv6
  // address.
  virtual bool IsPrivateIPAddress(const std::string& ip_address) const;

  // Returns true and sets is_phishing if url is in the cache and valid.
  virtual bool GetValidCachedResult(const GURL& url, bool* is_phishing);

  // Returns true if the url is in the cache.
  virtual bool IsInCache(const GURL& url);

  // Returns true if we have sent more than kMaxReportsPerInterval phishing
  // reports in the last kReportsInterval.
  virtual bool OverPhishingReportLimit();

  // Sends a model to each renderer.
  virtual void SendModelToRenderers();

  base::WeakPtr<ClientSideDetectionService> GetWeakPtr();

  // Get the model status for the given client-side model (extended reporting or
  // regular).
  ModelLoader::ClientModelStatus GetLastModelStatus(bool use_extended_model);

 private:
  friend class ClientSideDetectionServiceTest;
  FRIEND_TEST_ALL_PREFIXES(ClientSideDetectionServiceTest,
                           SetEnabledAndRefreshState);
  FRIEND_TEST_ALL_PREFIXES(ClientSideDetectionServiceTest,
                           ServiceObjectDeletedBeforeCallbackDone);
  FRIEND_TEST_ALL_PREFIXES(ClientSideDetectionServiceTest,
                           SendClientReportPhishingRequest);

  // CacheState holds all information necessary to respond to a caller without
  // actually making a HTTP request.
  struct CacheState {
    bool is_phishing;
    base::Time timestamp;

    CacheState(bool phish, base::Time time);
  };

  static const char kClientReportPhishingUrl[];
  static const int kMaxReportsPerInterval;
  static const int kInitialClientModelFetchDelayMs;
  static const int kReportsIntervalDays;
  static const int kNegativeCacheIntervalDays;
  static const int kPositiveCacheIntervalMinutes;

  // Called when the prefs have changed in a way we may need to respond to.
  void OnPrefsUpdated();

  // Enables or disables the service, and refreshes the state of all renderers.
  // Disabling cancels any pending requests; existing ClientSideDetectionHosts
  // will have their callbacks called with "false" verdicts.  Enabling starts
  // downloading the model after a delay.  In all cases, each render process is
  // updated to match the state
  void SetEnabledAndRefreshState(bool enabled);

  // Starts sending the request to the client-side detection frontends.
  // This method takes ownership of both pointers.
  void StartClientReportPhishingRequest(
      ClientPhishingRequest* verdict,
      bool is_extended_reporting,
      bool is_enhanced_protection,
      const ClientReportPhishingRequestCallback& callback);

  // Called by OnURLFetchComplete to handle the server response from
  // sending the client-side phishing request.
  void HandlePhishingVerdict(network::SimpleURLLoader* source,
                             const GURL& url,
                             int net_error,
                             int response_code,
                             const std::string& data);

  // Invalidate cache results which are no longer useful.
  void UpdateCache();

  // Get the number of phishing reports that we have sent over kReportsInterval.
  int GetPhishingNumReports();

  // Get the number of reports that we have sent over kReportsInterval, and
  // trims off the old elements.
  int GetNumReports(base::queue<base::Time>* report_times);

  // Send the model to the given renderer.
  void SendModelToProcess(content::RenderProcessHost* process);

  // Returns the URL that will be used for phishing requests.
  static GURL GetClientReportUrl(const std::string& report_url);

  // The profile this ClientSideDetectionService is attached to.
  Profile* profile_;

  // Whether the service is running or not.  When the service is not running,
  // it won't download the model nor report detected phishing URLs.
  bool enabled_;

  // We load two models: One for stadard Safe Browsing profiles,
  // and one for those opted into extended reporting.
  std::unique_ptr<ModelLoader> model_loader_standard_;
  std::unique_ptr<ModelLoader> model_loader_extended_;

  // Map of client report phishing request to the corresponding callback that
  // has to be invoked when the request is done.
  struct ClientPhishingReportInfo;
  std::map<const network::SimpleURLLoader*,
           std::unique_ptr<ClientPhishingReportInfo>>
      client_phishing_reports_;

  // Cache of completed requests. Used to satisfy requests for the same urls
  // as long as the next request falls within our caching window (which is
  // determined by kNegativeCacheInterval and kPositiveCacheInterval). The
  // size of this cache is limited by kMaxReportsPerDay *
  // ceil(InDays(max(kNegativeCacheInterval, kPositiveCacheInterval))).
  // TODO(gcasto): Serialize this so that it doesn't reset on browser restart.
  std::map<GURL, std::unique_ptr<CacheState>> cache_;

  // Timestamp of when we sent a phishing request. Used to limit the number
  // of phishing requests that we send in a day.
  // TODO(gcasto): Serialize this so that it doesn't reset on browser restart.
  base::queue<base::Time> phishing_report_times_;

  // The URLLoaderFactory we use to issue network requests.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  content::NotificationRegistrar registrar_;

  // PrefChangeRegistrar used to track when the Safe Browsing pref changes.
  PrefChangeRegistrar pref_change_registrar_;

  // Used to asynchronously call the callbacks for
  // SendClientReportPhishingRequest.
  base::WeakPtrFactory<ClientSideDetectionService> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ClientSideDetectionService);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CLIENT_SIDE_DETECTION_SERVICE_H_
