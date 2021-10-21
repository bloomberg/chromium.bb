// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_MAIN_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_MAIN_VIEW_H_

#include <map>
#include <vector>

#include "build/build_config.h"
#include "chrome/browser/ui/views/page_info/chosen_object_view_observer.h"
#include "chrome/browser/ui/views/page_info/permission_selector_row_observer.h"
#include "components/page_info/page_info_ui.h"
#include "device/vr/buildflags/buildflags.h"
#include "ui/views/view.h"

namespace views {
class Label;
class LabelButton;
}  // namespace views

class ChromePageInfoUiDelegate;
class ChosenObjectView;
class PageInfoHoverButton;
class PageInfoNavigationHandler;
class PageInfoSecurityContentView;
class PermissionToggleRowView;

namespace test {
class PageInfoBubbleViewTestApi;
}  // namespace test

// The main view of the page info, contains security information, permissions
// and  site-related settings. This is used in the experimental
// PageInfoNewBubbleView (under a flag PageInfoV2Desktop).
class PageInfoMainView : public views::View,
                         public PageInfoUI,
                         public PermissionSelectorRowObserver,
                         public ChosenObjectViewObserver {
 public:
  // The width of the column size for permissions and chosen object icons.
  static constexpr int kIconColumnWidth = 16;

  PageInfoMainView(PageInfo* presenter,
                   ChromePageInfoUiDelegate* ui_delegate,
                   PageInfoNavigationHandler* navigation_handler);
  ~PageInfoMainView() override;

  // PageInfoUI implementations.
  void SetCookieInfo(const CookieInfoList& cookie_info_list) override;
  void SetPermissionInfo(const PermissionInfoList& permission_info_list,
                         ChosenObjectInfoList chosen_object_info_list) override;
  void SetIdentityInfo(const IdentityInfo& identity_info) override;
  void SetPageFeatureInfo(const PageFeatureInfo& info) override;

  gfx::Size CalculatePreferredSize() const override;

  // PermissionSelectorRowObserver:
  void OnPermissionChanged(const PageInfo::PermissionInfo& permission) override;

  // ChosenObjectViewObserver:
  void OnChosenObjectDeleted(const PageInfoUI::ChosenObjectInfo& info) override;

 protected:
  // TODO(olesiamarukhno): Was used for tests, will update it after redesigning
  // moves forward.
  const std::u16string details_text() const { return details_text_; }

 private:
  friend class PageInfoBubbleViewDialogBrowserTest;
  friend class test::PageInfoBubbleViewTestApi;

  // Creates a view with vertical box layout that will used a container for
  // other views.
  std::unique_ptr<views::View> CreateContainerView() WARN_UNUSED_RESULT;

  // Creates bubble header view for this page, contains the title and the close
  // button.
  std::unique_ptr<views::View> CreateBubbleHeaderView() WARN_UNUSED_RESULT;

  // Posts a task to HandleMoreInfoRequestAsync() below.
  void HandleMoreInfoRequest(views::View* source);

  // Used to asynchronously handle clicks since these calls may cause the
  // destruction of the settings view and the base class window still needs to
  // be alive to finish handling the mouse or keyboard click.
  void HandleMoreInfoRequestAsync(int view_id);

  // Makes the permission reset button visible if there is any permission and
  // enables it if any permission is in a non-default state. Also updates
  // the label depending on the number of visible permissions.
  void UpdateResetButton(const PermissionInfoList& permission_info_list);

  // Creates 'About this site' section which contains a button that opens a
  // subpage and two separators.
  std::unique_ptr<views::View> CreateAboutThisSiteSection(
      std::u16string description) WARN_UNUSED_RESULT;

  PageInfo* presenter_;

  ChromePageInfoUiDelegate* ui_delegate_;

  PageInfoNavigationHandler* navigation_handler_;

  // The raw details of the status of the identity check for this site.
  std::u16string details_text_ = std::u16string();

  // The button that opens the "Connection" subpage.
  PageInfoHoverButton* connection_button_ = nullptr;

  // The view that contains the certificate, cookie, and permissions sections.
  views::View* site_settings_view_ = nullptr;

  // The button that opens the "Cookies" dialog.
  PageInfoHoverButton* cookie_button_ = nullptr;

  // The button that opens up "Site Settings".
  views::View* site_settings_link_ = nullptr;

  // The view that contains the scroll view with permission rows and the reset
  // button, surrounded by separators.
  views::View* permissions_view_ = nullptr;

  // The section that contains "About this site" button that opens a
  // subpage and two separators.
  views::View* about_this_site_section_ = nullptr;

  // The view that contains `SecurityInformationView` and a certificate button.
  PageInfoSecurityContentView* security_content_view_ = nullptr;

#if defined(OS_WIN) && BUILDFLAG(ENABLE_VR)
  // The view that contains ui related to features on a page, like a presenting
  // VR page.
  views::View* page_feature_info_view_ = nullptr;
#endif

  // These rows bundle together all the |View|s involved in a single row of the
  // permissions section, and keep those views updated when the underlying
  // |Permission| changes.
  std::vector<PermissionToggleRowView*> selector_rows_;

  std::vector<ChosenObjectView*> chosen_object_rows_;

  views::Label* title_ = nullptr;

  views::View* security_container_view_ = nullptr;

  views::LabelButton* reset_button_ = nullptr;

  base::WeakPtrFactory<PageInfoMainView> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_MAIN_VIEW_H_
