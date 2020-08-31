// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_router/cast_dialog_no_sinks_view.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/strings/string16.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/hover_button.h"
#include "chrome/browser/ui/views/media_router/cast_dialog_helper.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"
#include "url/gurl.h"

namespace {

auto CreateHelpIcon(views::ButtonListener* listener) {
  auto help_icon = views::CreateVectorImageButtonWithNativeTheme(
      listener, vector_icons::kHelpOutlineIcon);
  help_icon->SetInstallFocusRingOnFocus(true);
  help_icon->SetFocusForPlatform();
  help_icon->SetBorder(
      views::CreateEmptyBorder(media_router::kPrimaryIconBorder));
  help_icon->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_MEDIA_ROUTER_NO_DEVICES_FOUND_BUTTON));
  help_icon->SetInkDropMode(views::InkDropHostView::InkDropMode::OFF);
  return help_icon;
}

}  // namespace

namespace media_router {

constexpr base::TimeDelta CastDialogNoSinksView::kSearchWaitTime;

CastDialogNoSinksView::CastDialogNoSinksView(Profile* profile)
    : profile_(profile) {
  // Use horizontal button padding to ensure consistent spacing with the
  // CastDialogView and its sink views that are implemented as Buttons.
  const int horizontal_padding = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUTTON_HORIZONTAL_PADDING);

  // Maintain required padding between the throbber / icon and the label.
  const int icon_label_spacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_LABEL_HORIZONTAL);

  auto* layout_manager = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets(0, horizontal_padding), icon_label_spacing));
  layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  icon_ = AddChildView(CreateThrobber());
  label_ = AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_MEDIA_ROUTER_STATUS_LOOKING_FOR_DEVICES)));

  timer_.Start(FROM_HERE, kSearchWaitTime,
               base::BindOnce(&CastDialogNoSinksView::SetHelpIconView,
                              base::Unretained(this)));
}

CastDialogNoSinksView::~CastDialogNoSinksView() = default;

void CastDialogNoSinksView::ButtonPressed(views::Button* sender,
                                          const ui::Event& event) {
  // Opens the help center article for troubleshooting sinks not found in a
  // new tab. Called when |help_icon| is clicked.
  NavigateParams params(profile_, GURL(chrome::kCastNoDestinationFoundURL),
                        ui::PAGE_TRANSITION_LINK);
  Navigate(&params);
}

void CastDialogNoSinksView::SetHelpIconView() {
  // Replace the throbber with the help icon.
  RemoveChildViewT(icon_);
  icon_ = AddChildViewAt(CreateHelpIcon(this), 0);

  label_->SetText(
      l10n_util::GetStringUTF16(IDS_MEDIA_ROUTER_STATUS_NO_DEVICES_FOUND));
}

}  // namespace media_router
