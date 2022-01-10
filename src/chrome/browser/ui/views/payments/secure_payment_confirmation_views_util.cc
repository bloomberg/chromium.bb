// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/secure_payment_confirmation_views_util.h"

#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/views/accessibility/non_accessible_image_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/theme_resources.h"
#include "components/payments/core/sizes.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view.h"

namespace payments {
namespace {

const gfx::VectorIcon& GetPlatformVectorIcon(bool dark_mode) {
#if defined(OS_WIN)
  return dark_mode ? kSecurePaymentConfirmationFaceDarkIcon
                   : kSecurePaymentConfirmationFaceIcon;
#else
  return dark_mode ? kSecurePaymentConfirmationFingerprintDarkIcon
                   : kSecurePaymentConfirmationFingerprintIcon;
#endif
}

int GetSecurePaymentConfirmationHeaderWidth() {
  return ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH);
}

const gfx::ImageSkia GetHeaderImageSkia(bool dark_mode) {
  return ui::ResourceBundle::GetSharedInstance()
      .GetImageNamed(dark_mode ? IDR_SAVE_CARD_DARK : IDR_SAVE_CARD)
      .AsImageSkia();
}

class SecurePaymentConfirmationIconView : public NonAccessibleImageView {
 public:
  METADATA_HEADER(SecurePaymentConfirmationIconView);

  explicit SecurePaymentConfirmationIconView(bool use_cart_image = false)
      : use_cart_image_{use_cart_image} {
    const gfx::Size header_size(
        GetSecurePaymentConfirmationHeaderWidth(),
        use_cart_image_ ? kShoppingCartHeaderIconHeight : kHeaderIconHeight);
    SetSize(header_size);
    SetPreferredSize(header_size);
    SetVerticalAlignment(views::ImageView::Alignment::kLeading);
  }
  ~SecurePaymentConfirmationIconView() override = default;

  // NonAccessibleImageView:
  void OnThemeChanged() override {
    NonAccessibleImageView::OnThemeChanged();
    SetImage(use_cart_image_
                 ? GetHeaderImageSkia(GetNativeTheme()->ShouldUseDarkColors())
                 : gfx::CreateVectorIcon(GetPlatformVectorIcon(
                       GetNativeTheme()->ShouldUseDarkColors())));
  }

 private:
  bool use_cart_image_;
};

BEGIN_METADATA(SecurePaymentConfirmationIconView, NonAccessibleImageView)
END_METADATA

}  // namespace

std::unique_ptr<views::ProgressBar>
CreateSecurePaymentConfirmationProgressBarView() {
  auto progress_bar = std::make_unique<views::ProgressBar>(
      kProgressBarHeight, /*allow_round_corner=*/false);
  progress_bar->SetValue(-1);  // infinite animation.
  progress_bar->SetBackgroundColor(SK_ColorTRANSPARENT);
  progress_bar->SetPreferredSize(
      gfx::Size(GetSecurePaymentConfirmationHeaderWidth(), kProgressBarHeight));
  progress_bar->SizeToPreferredSize();

  return progress_bar;
}

std::unique_ptr<views::View> CreateSecurePaymentConfirmationHeaderView(
    int progress_bar_id,
    int header_icon_id,
    bool use_cart_image) {
  auto header = std::make_unique<views::BoxLayoutView>();
  header->SetOrientation(views::BoxLayout::Orientation::kVertical);
  header->SetBetweenChildSpacing(kHeaderIconTopPadding);

  // Progress bar
  auto progress_bar = CreateSecurePaymentConfirmationProgressBarView();
  progress_bar->SetID(progress_bar_id);
  progress_bar->SetVisible(false);
  auto* container = header->AddChildView(std::make_unique<views::View>());
  container->SetPreferredSize(progress_bar->GetPreferredSize());
  container->AddChildView(std::move(progress_bar));

  // Header icon
  auto image_view =
      std::make_unique<SecurePaymentConfirmationIconView>(use_cart_image);
  image_view->SetID(header_icon_id);
  header->AddChildView(std::move(image_view));

  return header;
}

std::unique_ptr<views::Label> CreateSecurePaymentConfirmationTitleLabel(
    const std::u16string& title) {
  std::unique_ptr<views::Label> title_label = std::make_unique<views::Label>(
      title, views::style::CONTEXT_DIALOG_TITLE, views::style::STYLE_PRIMARY);
  title_label->SetHorizontalAlignment(gfx::ALIGN_TO_HEAD);
  title_label->SetLineHeight(kTitleLineHeight);
  title_label->SetBorder(views::CreateEmptyBorder(0, 0, kBodyInsets, 0));

  return title_label;
}

std::unique_ptr<views::ImageView>
CreateSecurePaymentConfirmationInstrumentIconView(const SkBitmap& bitmap) {
  gfx::ImageSkia image = gfx::ImageSkia::CreateFrom1xBitmap(bitmap).DeepCopy();

  std::unique_ptr<views::ImageView> icon_view =
      std::make_unique<views::ImageView>();
  icon_view->SetImage(image);
  icon_view->SetImageSize(
      gfx::Size(kSecurePaymentConfirmationInstrumentIconWidthPx,
                kSecurePaymentConfirmationInstrumentIconHeightPx));
  icon_view->SetPaintToLayer();
  icon_view->layer()->SetFillsBoundsOpaquely(false);

  return icon_view;
}

}  // namespace payments
