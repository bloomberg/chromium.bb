// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CLIENT_SIDE_DETECTION_HOST_DELEGATE_H_
#define CHROME_BROWSER_SAFE_BROWSING_CLIENT_SIDE_DETECTION_HOST_DELEGATE_H_

#include "components/safe_browsing/content/browser/client_side_detection_host.h"

namespace safe_browsing {

// Delegate class which implements chrome specific bits for configuring
// the ClientSideDetectionHost class.
class ClientSideDetectionHostDelegate
    : public ClientSideDetectionHost::Delegate {
 public:
  static std::unique_ptr<ClientSideDetectionHost> CreateHost(
      content::WebContents* tab);

  explicit ClientSideDetectionHostDelegate(content::WebContents* web_contents);
  ~ClientSideDetectionHostDelegate() override;

  // ClientSideDetectionHost::Delegate implementation.
  bool HasSafeBrowsingUserInteractionObserver() override;
  PrefService* GetPrefs() override;
  scoped_refptr<SafeBrowsingDatabaseManager> GetSafeBrowsingDBManager()
      override;
  scoped_refptr<BaseUIManager> GetSafeBrowsingUIManager() override;
  ClientSideDetectionService* GetClientSideDetectionService() override;

 private:
  content::WebContents* web_contents_;

  DISALLOW_COPY_AND_ASSIGN(ClientSideDetectionHostDelegate);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CLIENT_SIDE_DETECTION_HOST_DELEGATE_H_
