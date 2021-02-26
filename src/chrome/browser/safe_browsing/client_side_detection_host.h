// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CLIENT_SIDE_DETECTION_HOST_H_
#define CHROME_BROWSER_SAFE_BROWSING_CLIENT_SIDE_DETECTION_HOST_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/safe_browsing/client_side_model_loader.h"
#include "chrome/browser/safe_browsing/ui_manager.h"
#include "components/safe_browsing/content/common/safe_browsing.mojom-shared.h"
#include "components/safe_browsing/core/db/database_manager.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "url/gurl.h"

namespace base {
class TickClock;
}

namespace safe_browsing {
class ClientPhishingRequest;
class ClientSideDetectionService;

// This class is used to receive the IPC from the renderer which
// notifies the browser that a URL was classified as phishing.  This
// class relays this information to the client-side detection service
// class which sends a ping to a server to validate the verdict.
// TODO(noelutz): move all client-side detection IPCs to this class.
class ClientSideDetectionHost : public content::WebContentsObserver {
 public:
  // The caller keeps ownership of the tab object and is responsible for
  // ensuring that it stays valid until WebContentsDestroyed is called.
  static std::unique_ptr<ClientSideDetectionHost> Create(
      content::WebContents* tab);
  ~ClientSideDetectionHost() override;

  // From content::WebContentsObserver.  If we navigate away we cancel all
  // pending callbacks that could show an interstitial, and check to see whether
  // we should classify the new URL.
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // Send the model to all the render frame hosts in this WebContents.
  void SendModelToRenderFrame();

 protected:
  explicit ClientSideDetectionHost(content::WebContents* tab);

  // From content::WebContentsObserver.
  void WebContentsDestroyed() override;
  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) override;

  // Used for testing.
  void set_ui_manager(SafeBrowsingUIManager* ui_manager);
  void set_database_manager(SafeBrowsingDatabaseManager* database_manager);

 private:
  friend class ClientSideDetectionHostTestBase;
  class ShouldClassifyUrlRequest;
  friend class ShouldClassifyUrlRequest;
  FRIEND_TEST_ALL_PREFIXES(ClientSideDetectionHostBrowserTest,
                           VerifyVisualFeatureCollection);

  // Called when pre-classification checks are done for the phishing
  // classifiers.
  void OnPhishingPreClassificationDone(bool should_classify);

  // |verdict| is an encoded ClientPhishingRequest protocol message, |result| is
  // the outcome of the renderer classification.
  void PhishingDetectionDone(mojom::PhishingDetectorResult result,
                             const std::string& verdict);

  // Callback that is called when the server ping back is
  // done. Display an interstitial if |is_phishing| is true.
  // Otherwise, we do nothing. Called in UI thread. |is_from_cache| indicates
  // whether the warning is being shown due to a cached verdict or from an
  // actual server ping.
  void MaybeShowPhishingWarning(bool is_from_cache,
                                GURL phishing_url,
                                bool is_phishing);

  // Returns true if the user has seen a regular SafeBrowsing
  // interstitial for the current page.  This is only true if the user has
  // actually clicked through the warning.  This method is called on the UI
  // thread.
  bool DidShowSBInterstitial() const;

  // Used for testing.  This function does not take ownership of the service
  // class.
  void set_client_side_detection_service(ClientSideDetectionService* service);

  // Sets a test tick clock only for testing.
  void set_tick_clock_for_testing(const base::TickClock* tick_clock) {
    tick_clock_ = tick_clock;
  }

  // This pointer may be nullptr if client-side phishing detection is disabled.
  ClientSideDetectionService* csd_service_;
  // The WebContents that the class is observing.
  content::WebContents* tab_;
  // These pointers may be nullptr if SafeBrowsing is disabled.
  scoped_refptr<SafeBrowsingDatabaseManager> database_manager_;
  scoped_refptr<SafeBrowsingUIManager> ui_manager_;
  // Keep a handle to the latest classification request so that we can cancel
  // it if necessary.
  scoped_refptr<ShouldClassifyUrlRequest> classification_request_;
  // The current URL
  GURL current_url_;
  // Current host, used to help determine cur_host_redirects_.
  std::string cur_host_;
  // The currently active message pipe to the renderer PhishingDetector.
  mojo::Remote<mojom::PhishingDetector> phishing_detector_;

  // Max number of ips we save for each browse
  static const size_t kMaxIPsPerBrowse;
  bool pageload_complete_;

  // Records the start time of when phishing detection started.
  base::TimeTicks phishing_detection_start_time_;
  const base::TickClock* tick_clock_;
  base::WeakPtrFactory<ClientSideDetectionHost> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ClientSideDetectionHost);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CLIENT_SIDE_DETECTION_HOST_H_
