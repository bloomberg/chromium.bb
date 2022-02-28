// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_VCARD_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_VCARD_TAB_HELPER_H_

#import <Foundation/Foundation.h>
#include <set>

#include "base/containers/unique_ptr_adapters.h"
#include "ios/web/public/download/download_task_observer.h"
#import "ios/web/public/web_state_user_data.h"

@protocol VcardTabHelperDelegate;
namespace web {
class DownloadTask;
class WebState;
}  // namespace web

// TabHelper which manages Vcard.
class VcardTabHelper : public web::DownloadTaskObserver,
                       public web::WebStateUserData<VcardTabHelper> {
 public:
  explicit VcardTabHelper(web::WebState* web_state);

  VcardTabHelper(const VcardTabHelper&) = delete;
  VcardTabHelper& operator=(const VcardTabHelper&) = delete;

  ~VcardTabHelper() override;

  id<VcardTabHelperDelegate> delegate() { return delegate_; }

  // |delegate| is not retained by this instance.
  void set_delegate(id<VcardTabHelperDelegate> delegate) {
    delegate_ = delegate;
  }

  // Asynchronously downloads the Vcard file using the given |task|. Asks
  // delegate to open the Vcard when the download is complete.
  virtual void Download(std::unique_ptr<web::DownloadTask> task);

 private:
  friend class web::WebStateUserData<VcardTabHelper>;

  // web::DownloadTaskObserver overrides:
  void OnDownloadUpdated(web::DownloadTask* task) override;

  __weak id<VcardTabHelperDelegate> delegate_ = nil;

  // Set of unfinished download tasks.
  std::set<std::unique_ptr<web::DownloadTask>, base::UniquePtrComparator>
      tasks_;

  WEB_STATE_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_VCARD_TAB_HELPER_H_
