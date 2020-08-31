// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFE_BROWSING_URL_CHECKER_DELEGATE_IMPL_H_
#define IOS_CHROME_BROWSER_SAFE_BROWSING_URL_CHECKER_DELEGATE_IMPL_H_

#include "base/memory/ref_counted.h"
#include "components/safe_browsing/core/browser/url_checker_delegate.h"

namespace safe_browsing {
class SafeBrowsingDatabaseManager;
}  // namespace safe_browsing

// This is an iOS implementation of the delegate interface for
// SafeBrowsingUrlCheckerImpl.
class UrlCheckerDelegateImpl : public safe_browsing::UrlCheckerDelegate {
 public:
  UrlCheckerDelegateImpl(
      scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager>
          database_manager);

  UrlCheckerDelegateImpl(const UrlCheckerDelegateImpl&) = delete;
  UrlCheckerDelegateImpl& operator=(const UrlCheckerDelegateImpl&) = delete;

 private:
  ~UrlCheckerDelegateImpl() override;

  // safe_browsing::UrlCheckerDelegate implementation
  void MaybeDestroyPrerenderContents(
      base::OnceCallback<content::WebContents*()> web_contents_getter) override;
  void StartDisplayingBlockingPageHelper(
      const security_interstitials::UnsafeResource& resource,
      const std::string& method,
      const net::HttpRequestHeaders& headers,
      bool is_main_frame,
      bool has_user_gesture) override;
  void StartObservingInteractionsForDelayedBlockingPageHelper(
      const security_interstitials::UnsafeResource& resource,
      bool is_main_frame) override;
  bool IsUrlWhitelisted(const GURL& url) override;
  bool ShouldSkipRequestCheck(const GURL& original_url,
                              int frame_tree_node_id,
                              int render_process_id,
                              int render_frame_id,
                              bool originated_from_service_worker) override;

  // This function is unused on iOS, since iOS cannot use content/.
  // TODO(crbug.com/1069047): Refactor SafeBrowsingUrlCheckerImpl and
  // UrlCheckerDelegate to extract the functionality that can be shared across
  // platforms, and move methods used only by content/ to classes used only by
  // content/.
  void NotifySuspiciousSiteDetected(
      const base::RepeatingCallback<content::WebContents*()>&
          web_contents_getter) override;
  const safe_browsing::SBThreatTypeSet& GetThreatTypes() override;
  safe_browsing::SafeBrowsingDatabaseManager* GetDatabaseManager() override;
  safe_browsing::BaseUIManager* GetUIManager() override;

  scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager> database_manager_;
  safe_browsing::SBThreatTypeSet threat_types_;
};

#endif  // IOS_CHROME_BROWSER_SAFE_BROWSING_URL_CHECKER_DELEGATE_IMPL_H_
