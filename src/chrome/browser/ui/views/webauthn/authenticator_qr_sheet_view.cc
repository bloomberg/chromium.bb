// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/authenticator_qr_sheet_view.h"

#include "base/bind.h"
#include "base/callback.h"
#include "chrome/services/qrcode_generator/public/cpp/qrcode_generator_service.h"
#include "chrome/services/qrcode_generator/public/mojom/qrcode_generator.mojom.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/background.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view.h"

namespace {

constexpr int kQrCodeMargin = 40;
constexpr int kQrCodeImageSize = 240;

}  // namespace

class AuthenticatorQRViewCentered : public views::View {
 public:
  METADATA_HEADER(AuthenticatorQRViewCentered);
  explicit AuthenticatorQRViewCentered(const std::string& qr_string) {
    views::BoxLayout* layout =
        SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kHorizontal));
    layout->set_main_axis_alignment(
        views::BoxLayout::MainAxisAlignment::kCenter);
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);
    const int border_radius =
        views::LayoutProvider::Get()->GetCornerRadiusMetric(
            views::Emphasis::kHigh);
    qr_code_image_ = AddChildViewAt(std::make_unique<views::ImageView>(), 0);
    qr_code_image_->SetBorder(views::CreateRoundedRectBorder(
        /*thickness=*/2, border_radius, gfx::kGoogleGrey200));
    qr_code_image_->SetHorizontalAlignment(
        views::ImageView::Alignment::kCenter);
    qr_code_image_->SetVerticalAlignment(views::ImageView::Alignment::kCenter);
    qr_code_image_->SetImageSize(qrCodeImageSize());
    qr_code_image_->SetPreferredSize(qrCodeImageSize() +
                                     gfx::Size(kQrCodeMargin, kQrCodeMargin));
    qr_code_image_->SetBackground(
        views::CreateRoundedRectBackground(SK_ColorWHITE, border_radius));

    qrcode_generator::mojom::GenerateQRCodeRequestPtr request =
        qrcode_generator::mojom::GenerateQRCodeRequest::New();
    request->data = qr_string;
    request->should_render = true;
    request->render_dino = true;
    request->render_module_style =
        qrcode_generator::mojom::ModuleStyle::CIRCLES;
    request->render_locator_style =
        qrcode_generator::mojom::LocatorStyle::DEFAULT_SQUARE;

    // Deleting the view will close the channel so base::Unretained is safe
    // here.
    auto callback =
        base::BindOnce(&AuthenticatorQRViewCentered::OnQrCodeGenerated,
                       base::Unretained(this));
    qr_code_service()->GenerateQRCode(std::move(request), std::move(callback));
  }

  ~AuthenticatorQRViewCentered() override = default;

  AuthenticatorQRViewCentered(const AuthenticatorQRViewCentered&) = delete;
  AuthenticatorQRViewCentered& operator=(const AuthenticatorQRViewCentered&) =
      delete;

 private:
  qrcode_generator::mojom::QRCodeGeneratorService* qr_code_service() {
    if (!qr_code_service_remote_)
      qr_code_service_remote_ =
          qrcode_generator::LaunchQRCodeGeneratorService();
    return qr_code_service_remote_.get();
  }

  gfx::Size qrCodeImageSize() const {
    return gfx::Size(kQrCodeImageSize, kQrCodeImageSize);
  }

  void OnQrCodeGenerated(
      const qrcode_generator::mojom::GenerateQRCodeResponsePtr response) {
    DCHECK(response->error_code ==
           qrcode_generator::mojom::QRCodeGeneratorError::NONE);
    qr_code_image_->SetImage(
        gfx::ImageSkia::CreateFrom1xBitmap(response->bitmap));
    qr_code_image_->SetVisible(true);
  }

  std::string qr_string_;
  views::ImageView* qr_code_image_;

  // Service instance for QR code image generation.
  mojo::Remote<qrcode_generator::mojom::QRCodeGeneratorService>
      qr_code_service_remote_;
};

BEGIN_METADATA(AuthenticatorQRViewCentered, views::View)
END_METADATA

AuthenticatorQRSheetView::AuthenticatorQRSheetView(
    std::unique_ptr<AuthenticatorQRSheetModel> sheet_model)
    : AuthenticatorRequestSheetView(std::move(sheet_model)),
      qr_string_(static_cast<AuthenticatorQRSheetModel*>(model())
                     ->dialog_model()
                     ->cable_qr_string()) {}

AuthenticatorQRSheetView::~AuthenticatorQRSheetView() = default;

std::pair<std::unique_ptr<views::View>,
          AuthenticatorRequestSheetView::AutoFocus>
AuthenticatorQRSheetView::BuildStepSpecificContent() {
  auto qr_view = std::make_unique<AuthenticatorQRViewCentered>(qr_string_);
  qr_view_ = qr_view.get();

  return std::make_pair(std::move(qr_view), AutoFocus::kYes);
}
