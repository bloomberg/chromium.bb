// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TAB_SHARING_TAB_SHARING_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_UI_TAB_SHARING_TAB_SHARING_INFOBAR_DELEGATE_H_

#include "base/memory/raw_ptr.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "content/public/browser/global_routing_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace infobars {
class ContentInfoBarManager;
class InfoBar;
}

class TabSharingInfoBarDelegateButton;
class TabSharingUI;

// Creates an infobar for sharing a tab using desktopCapture() API; one delegate
// per tab.
//
// 1. Layout for currently shared tab:
// "Sharing this tab to |app_name_| [Stop]"
//
// 2. Layout for capturing/captured tab:
// "Sharing |shared_tab_name_| to |app_name_| [Stop] [Switch-Label]"
// Where [Switch-Label] is "Switch to tab <hostname>", with the hostname for
// in the captured tab being the capturer's, and vice versa.
//
// 3a. Layout for all other tabs:
// "Sharing |shared_tab_name_| to |app_name_| [Stop] [Share this tab instead]"
// 3b. Or if |shared_tab_name_| is empty:
// "Sharing a tab to |app_name_| [Stop] [Share this tab instead]"
class TabSharingInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  // Represents a target to which focus could be switched and its favicon.
  struct FocusTarget {
    content::GlobalRenderFrameHostId id;
    ui::ImageModel icon;
  };

  // Creates a tab sharing infobar, which has 1-2 buttons.
  //
  // The primary button is for stopping the capture. It is always present.
  //
  // If |focus_target| has a value, the secondary button switches focus.
  // The image on the secondary button is derived from |focus_target|.
  // Else, if |can_share_instead|, the secondary button changes the
  // capture target to be the tab associated with |this| object.
  // Otherwise, there is no secondary button.
  static infobars::InfoBar* Create(
      infobars::ContentInfoBarManager* infobar_manager,
      const std::u16string& shared_tab_name,
      const std::u16string& app_name,
      bool shared_tab,
      bool can_share_instead,
      absl::optional<FocusTarget> focus_target,
      TabSharingUI* ui,
      bool favicons_used_for_switch_to_tab_button = false);

  ~TabSharingInfoBarDelegate() override;

 private:
  TabSharingInfoBarDelegate(std::u16string shared_tab_name,
                            std::u16string app_name,
                            bool shared_tab,
                            bool can_share_instead,
                            absl::optional<FocusTarget> focus_target,
                            TabSharingUI* ui,
                            bool favicons_used_for_switch_to_tab_button);

  // ConfirmInfoBarDelegate:
  bool EqualsDelegate(InfoBarDelegate* delegate) const override;
  bool ShouldExpire(const NavigationDetails& details) const override;
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  std::u16string GetMessageText() const override;
  std::u16string GetButtonLabel(InfoBarButton button) const override;
  ui::ImageModel GetButtonImage(InfoBarButton button) const override;
  int GetButtons() const override;
  bool Accept() override;
  bool Cancel() override;
  bool IsCloseable() const override;
  const gfx::VectorIcon& GetVectorIcon() const override;

  const std::u16string shared_tab_name_;
  const std::u16string app_name_;
  const bool shared_tab_;

  // Creates and removes delegate's infobar; outlives delegate.
  const raw_ptr<TabSharingUI> ui_;

  // TODO(crbug.com/1224363): Re-enable favicons by default or drop the code.
  const bool favicons_used_for_switch_to_tab_button_;

  std::unique_ptr<TabSharingInfoBarDelegateButton> secondary_button_;
};

#endif  // CHROME_BROWSER_UI_TAB_SHARING_TAB_SHARING_INFOBAR_DELEGATE_H_
