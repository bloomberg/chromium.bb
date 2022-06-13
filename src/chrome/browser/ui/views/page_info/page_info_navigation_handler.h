// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_NAVIGATION_HANDLER_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_NAVIGATION_HANDLER_H_

namespace page_info {
namespace proto {
class SiteInfo;
}
}  // namespace page_info

// An interface that provides methods to navigate between pages of the page
// info. Note that `OpenMainPage` must update the set of ignored empty storage
// keys before storage usage can be displayed. This happens asynchronously and
// `initialized_callback` fires after it has finished.
class PageInfoNavigationHandler {
 public:
  virtual void OpenMainPage(base::OnceClosure initialized_callback) = 0;
  virtual void OpenSecurityPage() = 0;
  virtual void OpenPermissionPage(ContentSettingsType type) = 0;
  virtual void OpenAboutThisSitePage(
      const page_info::proto::SiteInfo& info) = 0;
  virtual void CloseBubble() = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_NAVIGATION_HANDLER_H_
