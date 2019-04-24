// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/pwa_install_view.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/banners/app_banner_manager.h"
#include "chrome/browser/web_applications/components/web_app_tab_helper_base.h"
#include "chrome/grit/generated_resources.h"
#include "components/omnibox/browser/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"

PwaInstallView::PwaInstallView(CommandUpdater* command_updater,
                               PageActionIconView::Delegate* delegate)
    : PageActionIconView(command_updater, IDC_INSTALL_PWA, delegate) {
  SetVisible(false);
  SetLabel(l10n_util::GetStringUTF16(IDS_OMNIBOX_PWA_INSTALL_ICON_LABEL));
  SetUpForInOutAnimation();
}

PwaInstallView::~PwaInstallView() {}

bool PwaInstallView::Update() {
  content::WebContents* web_contents = GetWebContents();
  if (!web_contents)
    return false;

  banners::AppBannerManager* manager =
      banners::AppBannerManager::FromWebContents(web_contents);
  DCHECK(manager);

  bool is_installable = manager->IsInstallable();
  bool is_installed =
      web_app::WebAppTabHelperBase::FromWebContents(web_contents)
          ->HasAssociatedApp();
  bool show_install_button = is_installable && !is_installed;
  // TODO(crbug.com/907351): When installability is unknown and we're still in
  // the scope of a previously-determined installable site, display it as still
  // being installable.

  if (show_install_button && manager->MaybeConsumeInstallAnimation())
    AnimateIn(base::nullopt);
  else
    ResetSlideAnimation(false);

  bool was_visible = visible();
  SetVisible(show_install_button);
  return visible() != was_visible;
}

void PwaInstallView::OnExecuting(PageActionIconView::ExecuteSource source) {}

views::BubbleDialogDelegateView* PwaInstallView::GetBubble() const {
  // TODO(https://907351): Implement.
  return nullptr;
}

bool PwaInstallView::ShouldShowSeparator() const {
  return false;
}

const gfx::VectorIcon& PwaInstallView::GetVectorIcon() const {
  return omnibox::kPlusIcon;
}

base::string16 PwaInstallView::GetTextForTooltipAndAccessibleName() const {
  content::WebContents* web_contents = GetWebContents();
  if (!web_contents)
    return base::string16();
  return l10n_util::GetStringFUTF16(
      IDS_OMNIBOX_PWA_INSTALL_ICON_TOOLTIP,
      banners::AppBannerManager::GetInstallableAppName(web_contents));
}
