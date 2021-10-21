// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/phone_hub_recent_apps_view.h"

#include <numeric>
#include <vector>

#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/phonehub/phone_hub_recent_app_button.h"
#include "ash/system/phonehub/phone_hub_view_ids.h"
#include "ash/system/phonehub/ui_constants.h"
#include "ash/system/tray/tray_constants.h"
#include "base/cxx17_backports.h"
#include "chromeos/components/phonehub/notification.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// Appearance constants in DIPs.
constexpr gfx::Insets kRecentAppButtonFocusPadding(4);
constexpr int kHeaderLabelLineHeight = 30;
constexpr int kRecentAppButtonDefaultSpacing = 42;
constexpr int kRecentAppButtonMinSpacing = 4;
constexpr int kRecentAppButtonSize = 32;
constexpr int kRecentAppButtonsViewTopPadding = 12;

// Typography.
constexpr int kHeaderTextFontSizeDip = 15;

class HeaderView : public views::Label {
 public:
  HeaderView() {
    SetText(l10n_util::GetStringUTF16(IDS_ASH_PHONE_HUB_RECENT_APPS_TITLE));
    SetLineHeight(kHeaderLabelLineHeight);
    SetFontList(font_list()
                    .DeriveWithSizeDelta(kHeaderTextFontSizeDip -
                                         font_list().GetFontSize())
                    .DeriveWithWeight(gfx::Font::Weight::MEDIUM));
    SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
    SetVerticalAlignment(gfx::VerticalAlignment::ALIGN_MIDDLE);
    SetAutoColorReadabilityEnabled(false);
    SetSubpixelRenderingEnabled(false);
    SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kTextColorPrimary));
  }
  ~HeaderView() override = default;
  HeaderView(HeaderView&) = delete;
  HeaderView operator=(HeaderView&) = delete;

  // views::View:
  const char* GetClassName() const override { return "HeaderView"; }
};

}  // namespace

PhoneHubRecentAppsView::PhoneHubRecentAppsView(
    chromeos::phonehub::RecentAppsInteractionHandler*
        recent_apps_interaction_handler)
    : recent_apps_interaction_handler_(recent_apps_interaction_handler) {
  SetID(PhoneHubViewID::kPhoneHubRecentAppsView);
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);
  AddChildView(std::make_unique<HeaderView>());
  recent_app_buttons_view_ =
      AddChildView(std::make_unique<RecentAppButtonsView>());

  Update();
}

PhoneHubRecentAppsView::~PhoneHubRecentAppsView() {}

const char* PhoneHubRecentAppsView::GetClassName() const {
  return "PhoneHubRecentAppsView";
}

PhoneHubRecentAppsView::RecentAppButtonsView::RecentAppButtonsView() = default;

PhoneHubRecentAppsView::RecentAppButtonsView::~RecentAppButtonsView() = default;

void PhoneHubRecentAppsView::RecentAppButtonsView::AddRecentAppButton(
    views::View* recent_app_button) {
  AddChildView(recent_app_button);
}

// views::View:
gfx::Size PhoneHubRecentAppsView::RecentAppButtonsView::CalculatePreferredSize()
    const {
  int width = kTrayMenuWidth - kBubbleHorizontalSidePaddingDip * 2;
  int height = kRecentAppButtonSize + kRecentAppButtonFocusPadding.height() +
               kRecentAppButtonsViewTopPadding;
  return gfx::Size(width, height);
}

void PhoneHubRecentAppsView::RecentAppButtonsView::Layout() {
  const gfx::Rect child_area = GetContentsBounds();
  views::View::Views visible_children;
  std::copy_if(children().cbegin(), children().cend(),
               std::back_inserter(visible_children), [](const auto* v) {
                 return v->GetVisible() && (v->GetPreferredSize().width() > 0);
               });
  if (visible_children.empty())
    return;
  const int visible_child_width =
      std::accumulate(visible_children.cbegin(), visible_children.cend(), 0,
                      [](int width, const auto* v) {
                        return width + v->GetPreferredSize().width();
                      });
  int spacing = 0;
  if (visible_children.size() > 1) {
    spacing = (child_area.width() - visible_child_width) /
              (static_cast<int>(visible_children.size()) - 1);
    spacing = base::clamp(spacing, kRecentAppButtonMinSpacing,
                          kRecentAppButtonDefaultSpacing);
  }

  int x_delta = child_area.x();
  int child_y = child_area.y() + kRecentAppButtonsViewTopPadding +
                kRecentAppButtonFocusPadding.bottom();
  for (auto* child : visible_children) {
    // Most recent apps be added to the right and shift left as the other apps
    // are streamed.
    int child_x = child_area.width() - x_delta - kRecentAppButtonSize;
    int width = child->GetPreferredSize().width();
    child->SetBounds(child_x, child_y, width, child->GetHeightForWidth(width));
    x_delta += width + spacing;
  }
}

const char* PhoneHubRecentAppsView::RecentAppButtonsView::GetClassName() const {
  return "RecentAppButtonView";
}

void PhoneHubRecentAppsView::RecentAppButtonsView::Reset() {
  RemoveAllChildViews();
}

void PhoneHubRecentAppsView::Update() {
  recent_app_buttons_view_->Reset();
  recent_app_button_list_.clear();

  std::vector<chromeos::phonehub::Notification::AppMetadata> recent_apps_list =
      recent_apps_interaction_handler_->FetchRecentAppMetadataList();
  if (recent_apps_list.empty()) {
    SetVisible(false);
    return;
  }

  for (const auto& recent_app : recent_apps_list) {
    auto pressed_callback = base::BindRepeating(
        &chromeos::phonehub::RecentAppsInteractionHandler::
            NotifyRecentAppClicked,
        base::Unretained(recent_apps_interaction_handler_),
        recent_app.package_name, recent_app.visible_app_name);
    recent_app_button_list_.push_back(std::make_unique<PhoneHubRecentAppButton>(
        recent_app.icon, pressed_callback));
    recent_app_buttons_view_->AddRecentAppButton(
        recent_app_button_list_.back().get());
  }

  PreferredSizeChanged();
  SetVisible(true);
}

}  // namespace ash
