// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sharing_hub/preview_view.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/share/share_features.h"
#include "chrome/browser/ui/sharing_hub/sharing_hub_bubble_controller.h"
#include "components/url_formatter/elide_url.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/view_class_properties.h"

namespace sharing_hub {

namespace {

struct LayoutVariant {
  gfx::Insets interior_margin;
  gfx::Insets default_margin;
  gfx::Size image_size;

  static LayoutVariant FromFeatureConfig();
};

// These values are all directly from the Figma redlines. See
// https://crbug.com/1314486 and https://crbug.com/1316473.
constexpr LayoutVariant kVariant16{gfx::Insets::VH(8, 8),
                                   gfx::Insets::VH(0, 16), gfx::Size(16, 16)};

constexpr LayoutVariant kVariant40{gfx::Insets::VH(8, 8),
                                   gfx::Insets::VH(0, 16), gfx::Size(40, 40)};

constexpr LayoutVariant kVariant72{gfx::Insets::VH(8, 8),
                                   gfx::Insets::VH(0, 16), gfx::Size(72, 72)};

LayoutVariant LayoutVariant::FromFeatureConfig() {
  switch (share::GetDesktopSharePreviewVariant()) {
    case share::DesktopSharePreviewVariant::kEnabled16:
      return kVariant16;
    case share::DesktopSharePreviewVariant::kEnabled40:
      return kVariant40;
    case share::DesktopSharePreviewVariant::kEnabled72:
      return kVariant72;
    case share::DesktopSharePreviewVariant::kDisabled:
      NOTREACHED();
      return kVariant16;
  }
}

class UrlLabel : public views::Label {
 public:
  UrlLabel(GURL url, int context, int style)
      : views::Label(base::UTF8ToUTF16(url.spec()), context, style), url_(url) {
    // Never use the elided URL for the accessible name or tooltip - both of
    // these are allowed to be of arbitrary length (since they aren't
    // constrained by the visual layout) and should give the user the full URL.
    SetAccessibleName(GetText());
    SetTooltipText(GetText());
  }
  ~UrlLabel() override = default;

  void OnBoundsChanged(const gfx::Rect& previous) override {
    // Danger! Do not repurpose this behavior for your own use!
    //
    // It is safe to change the text in response to a layout like this *only*
    // because the UrlLabel is included in a fixed-width bubble. If the bubble
    // instead had variable width, or this view's width could otherwise change,
    // then it would be very easy to get into an infinite loop between setting
    // the text (thus prompting a relayout) and the layout setting the width
    // (thus causing the text to be set again). Since the bubble containing the
    // UrlLabel has a fixed width, this infinite recurrence can't happen.
    views::Label::OnBoundsChanged(previous);
    SetText(url_formatter::ElideUrl(url_, font_list(), width()));
  }

 private:
  GURL url_;
};

}  // namespace

// This view uses two nested FlexLayouts, a horizontal outer one and a vertical
// inner one, to create a composite layout where the icon is aligned with the
// title and URL taken together. The resulting View tree looks like this:
//   PreviewView [FlexLayout, horizontal]
//     ImageView
//     View [FlexLayout, vertical]
//       Label (title)
//       Label (URL)
PreviewView::PreviewView(share::ShareAttempt attempt, ui::ImageModel image) {
  auto variant = LayoutVariant::FromFeatureConfig();

  auto* layout = SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetMainAxisAlignment(views::LayoutAlignment::kStart)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
      .SetInteriorMargin(variant.interior_margin)
      .SetDefault(views::kMarginsKey, variant.default_margin)
      .SetCollapseMargins(true);

  image_ = AddChildView(std::make_unique<views::ImageView>(image));
  image_->SetPreferredSize(variant.image_size);

  auto* labels_container = AddChildView(std::make_unique<views::View>());
  labels_container->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero));
  auto* labels_layout =
      labels_container->SetLayoutManager(std::make_unique<views::FlexLayout>());
  labels_layout->SetOrientation(views::LayoutOrientation::kVertical)
      .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStart)
      .SetDefault(
          views::kFlexBehaviorKey,
          views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero));

  // TODO(ellyjones): These do not exactly match the redlines, which call for
  // 14pt Roboto specifically. We should probably update the redlines to not
  // use a hardcoded font, but we could also specify the font more explicitly
  // here.
  title_ = labels_container->AddChildView(std::make_unique<views::Label>(
      attempt.title, views::style::CONTEXT_DIALOG_TITLE));
  title_->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  url_ = labels_container->AddChildView(std::make_unique<UrlLabel>(
      attempt.url, views::style::CONTEXT_DIALOG_TITLE,
      views::style::STYLE_HINT));
  url_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
}

PreviewView::~PreviewView() = default;

void PreviewView::TakeCallbackSubscription(
    base::CallbackListSubscription subscription) {
  subscription_ = std::move(subscription);
}

void PreviewView::OnImageChanged(ui::ImageModel model) {
  image_->SetImage(model);
  image_->SetImageSize(LayoutVariant::FromFeatureConfig().image_size);
}

}  // namespace sharing_hub
