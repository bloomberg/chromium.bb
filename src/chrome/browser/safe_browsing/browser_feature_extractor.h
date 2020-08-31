// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// BrowserFeatureExtractor computes various browser features for client-side
// phishing detection.  For now it does a bunch of lookups in the history
// service to see whether a particular URL has been visited before by the
// user.

#ifndef CHROME_BROWSER_SAFE_BROWSING_BROWSER_FEATURE_EXTRACTOR_H_
#define CHROME_BROWSER_SAFE_BROWSING_BROWSER_FEATURE_EXTRACTOR_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/time/time.h"
#include "components/history/core/browser/history_types.h"
#include "components/security_interstitials/core/unsafe_resource.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

namespace history {
class HistoryService;
struct VisibleVisitCountToHostResult;
struct QueryURLResult;
}

namespace safe_browsing {
class ClientPhishingRequest;

struct BrowseInfo {
  // The URL we're currently browsing.
  GURL url;

  // If a SafeBrowsing interstitial was shown for the current URL
  // this will contain the UnsafeResource struct for that URL.
  std::unique_ptr<security_interstitials::UnsafeResource> unsafe_resource;

  // List of redirects that lead to the first page on the current host and
  // the current url respectively. These may be the same if the current url
  // is the first page on its host.
  std::vector<GURL> host_redirects;
  std::vector<GURL> url_redirects;

  // URL of the referrer of this URL load.
  GURL referrer;

  // The HTTP status code from this navigation.
  int http_status_code;

  BrowseInfo();
  ~BrowseInfo();
};

// All methods of this class must be called on the UI thread (including
// the constructor).
class BrowserFeatureExtractor {
 public:
  // Called when feature extraction is done.  The first argument will be
  // true iff feature extraction succeeded.  The second argument is the
  // phishing request which was modified by the feature extractor.
  using DoneCallback =
      base::OnceCallback<void(bool feature_extraction_succeeded,
                              std::unique_ptr<ClientPhishingRequest> request)>;

  // The caller keeps ownership of the tab and host objects and is
  // responsible for ensuring that they stay valid for the entire
  // lifetime of this object.
  explicit BrowserFeatureExtractor(content::WebContents* tab);

  // The destructor will cancel any pending requests.
  virtual ~BrowserFeatureExtractor();

  // Begins extraction of the browser features.  We take ownership
  // of the request object until |callback| is called (see DoneCallback above)
  // and will write the extracted features to the feature map.  Once the
  // feature extraction is complete, |callback| is run on the UI thread.  We
  // take ownership of the |callback| object.  |info| may not be valid after
  // ExtractFeatures returns.  This method must run on the UI thread.
  virtual void ExtractFeatures(const BrowseInfo* info,
                               std::unique_ptr<ClientPhishingRequest> request,
                               DoneCallback callback);

 private:
  // Synchronous browser feature extraction.
  void ExtractBrowseInfoFeatures(const BrowseInfo& info,
                                 ClientPhishingRequest* request);

  // Actually starts feature extraction (does the real work).
  void StartExtractFeatures(std::unique_ptr<ClientPhishingRequest> request,
                            DoneCallback callback);

  // HistoryService callback which is called when we're done querying URL visits
  // in the history.
  void QueryUrlHistoryDone(std::unique_ptr<ClientPhishingRequest> request,
                           DoneCallback callback,
                           history::QueryURLResult result);

  // HistoryService callback which is called when we're done querying HTTP host
  // visits in the history.
  void QueryHttpHostVisitsDone(std::unique_ptr<ClientPhishingRequest> request,
                               DoneCallback callback,
                               history::VisibleVisitCountToHostResult result);

  // HistoryService callback which is called when we're done querying HTTPS host
  // visits in the history.
  void QueryHttpsHostVisitsDone(std::unique_ptr<ClientPhishingRequest> request,
                                DoneCallback callback,
                                history::VisibleVisitCountToHostResult result);

  // Helper function which sets the host history features given the
  // number of host visits and the time of the fist host visit.  Set
  // |is_http_query| to true if the URL scheme is HTTP and to false if
  // the scheme is HTTPS.
  void SetHostVisitsFeatures(int num_visits,
                             base::Time first_visit,
                             bool is_http_query,
                             ClientPhishingRequest* request);

  // Helper function which gets the history server if possible.  If the pointer
  // is set it will return true and false otherwise.
  bool GetHistoryService(history::HistoryService** history);

  content::WebContents* tab_;
  base::CancelableTaskTracker cancelable_task_tracker_;
  base::WeakPtrFactory<BrowserFeatureExtractor> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(BrowserFeatureExtractor);
};

}  // namespace safe_browsing
#endif  // CHROME_BROWSER_SAFE_BROWSING_BROWSER_FEATURE_EXTRACTOR_H_
