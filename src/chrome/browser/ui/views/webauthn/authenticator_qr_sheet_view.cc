// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/authenticator_qr_sheet_view.h"

#include "base/base64url.h"
#include "base/rand_util.h"
#include "base/strings/string_piece.h"
#include "chrome/common/qr_code_generator/dino_image.h"
#include "chrome/common/qr_code_generator/qr_code_generator.h"
#include "device/fido/cable/cable_discovery_data.h"
#include "ui/gfx/canvas.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"

using QRCode = QRCodeGenerator;

namespace {

// QRView displays a QR code.
class QRView : public views::View {
 public:
  // kTilePixels is the height and width, in pixels, of a single tile from the
  // QR code.
  static constexpr int kTilePixels = 10;
  // kDinoTilePixels is the height and width, in pixels, of a single bit from
  // the dino image.
  static constexpr int kDinoTilePixels = 3;
  // kMid is the pixel offset from the top (or left) to the middle of the
  // displayed QR code.
  static constexpr int kMid = (kTilePixels * (2 + QRCode::kSize + 2)) / 2;
  // kDinoX is the x-coordinate of the dino image.
  static constexpr int kDinoX =
      kMid - (dino_image::kDinoWidth * kDinoTilePixels) / 2;
  // kDinoY is the y-coordinate of the dino image.
  static constexpr int kDinoY =
      kMid - (dino_image::kDinoHeight * kDinoTilePixels) / 2;

  explicit QRView(base::span<const uint8_t> qr_data)
      : qr_tiles_(qr_.Generate(qr_data)) {}
  ~QRView() override = default;

  void RefreshQRCode(base::span<const uint8_t> new_qr_data) {
    state_ = (state_ + 1) % 6;
    qr_tiles_ = qr_.Generate(new_qr_data);
    SchedulePaint();
  }

  // View:
  gfx::Size CalculatePreferredSize() const override {
    // A two-tile border is required around the QR code.
    return gfx::Size((2 + QRCode::kSize + 2) * kTilePixels,
                     (2 + QRCode::kSize + 2) * kTilePixels);
  }

  void OnPaint(gfx::Canvas* canvas) override {
    const SkColor off = SkColorSetARGB(0xff, 0xff, 0xff, 0xff);

    // kV is the intensity of the colors in the QR code.
    constexpr uint8_t kV = 0x70;
    SkColor on;
    switch (state_) {
      case 0:
        on = SkColorSetARGB(0xff, kV, 0, 0);
        break;
      case 1:
        on = SkColorSetARGB(0xff, 0, kV, 0);
        break;
      case 2:
        on = SkColorSetARGB(0xff, 0, 0, kV);
        break;
      case 3:
        on = SkColorSetARGB(0xff, kV, kV, 0);
        break;
      case 4:
        on = SkColorSetARGB(0xff, kV, 0, kV);
        break;
      case 5:
        on = SkColorSetARGB(0xff, 0, kV, kV);
        break;
    }

    // Draw the two-tile border around the edge.
    // Top.
    canvas->FillRect(
        gfx::Rect(0, 0, (2 + QRCode::kSize + 2) * kTilePixels, 2 * kTilePixels),
        off);
    // Bottom.
    canvas->FillRect(
        gfx::Rect(0, (2 + QRCode::kSize) * kTilePixels,
                  (2 + QRCode::kSize + 2) * kTilePixels, 2 * kTilePixels),
        off);
    // Left
    canvas->FillRect(gfx::Rect(0, 2 * kTilePixels, 2 * kTilePixels,
                               QRCode::kSize * kTilePixels),
                     off);
    // Right
    canvas->FillRect(
        gfx::Rect((2 + QRCode::kSize) * kTilePixels, 2 * kTilePixels,
                  2 * kTilePixels, QRCode::kSize * kTilePixels),
        off);

    // Paint the QR code.
    int index = 0;
    for (int y = 0; y < QRCode::kSize; y++) {
      for (int x = 0; x < QRCode::kSize; x++) {
        SkColor tile_color = qr_tiles_[index++] & 1 ? on : off;
        canvas->FillRect(gfx::Rect((x + 2) * kTilePixels, (y + 2) * kTilePixels,
                                   kTilePixels, kTilePixels),
                         tile_color);
      }
    }

    PaintDinoSegment(
        canvas,
        (state_ & 1) ? dino_image::kDinoHeadLeft : dino_image::kDinoHeadRight,
        dino_image::kDinoHeadHeight, 0);
    PaintDinoSegment(canvas, dino_image::kDinoBody,
                     dino_image::kDinoHeight - dino_image::kDinoHeadHeight,
                     dino_image::kDinoHeadHeight);
  }

 private:
  void PaintDinoSegment(gfx::Canvas* canvas,
                        const uint8_t* data,
                        const int rows,
                        const int y_offset) {
    const SkColor color = SkColorSetARGB(0xff, 0x00, 0x00, 0x00);

    for (int y = 0; y < rows; y++) {
      uint8_t current_byte;
      int bits = 0;

      for (int x = 0; x < dino_image::kDinoWidth; x++) {
        if (bits == 0) {
          current_byte = *data++;
          bits = 8;
        }
        const bool is_set = (current_byte & 128) != 0;
        current_byte <<= 1;
        bits--;

        if (is_set) {
          canvas->FillRect(gfx::Rect(kDinoX + x * kDinoTilePixels,
                                     kDinoY + (y + y_offset) * kDinoTilePixels,
                                     kDinoTilePixels, kDinoTilePixels),
                           color);
        }
      }
    }
  }

  QRCode qr_;
  base::span<const uint8_t, QRCode::kTotalSize> qr_tiles_;
  int state_ = 0;

  DISALLOW_COPY_AND_ASSIGN(QRView);
};

// Base64EncodedSize returns the number of bytes required to base64 encode an
// input of |input_length| bytes, without padding.
constexpr size_t Base64EncodedSize(size_t input_length) {
  return ((input_length * 4) + 2) / 3;
}

// QRDataForCurrentTime writes a URL suitable for encoding as a QR to |out_buf|
// and returns a span pointing into that buffer. The URL is generated based on
// |qr_generator_key| and the current time such that the caBLE discovery code
// can recognise the URL as valid.
base::span<uint8_t> QRDataForCurrentTime(
    uint8_t out_buf[QRCode::kInputBytes],
    base::span<const uint8_t, 32> qr_generator_key) {
  const int64_t current_tick = device::CableDiscoveryData::CurrentTimeTick();
  const device::CableQRData qr_data =
      device::CableDiscoveryData::DeriveQRData(qr_generator_key, current_tick);

  std::string base64_qr_data;
  base::Base64UrlEncode(
      base::StringPiece(reinterpret_cast<const char*>(qr_data.data()),
                        sizeof(qr_data)),
      base::Base64UrlEncodePolicy::OMIT_PADDING, &base64_qr_data);
  static constexpr size_t kEncodedDataLength =
      Base64EncodedSize(sizeof(qr_data));
  DCHECK_EQ(kEncodedDataLength, base64_qr_data.size());

  static constexpr char kPrefix[] = "fido://c1/";
  static constexpr size_t kPrefixLength = sizeof(kPrefix) - 1;

  static_assert(QRCode::kInputBytes >= kPrefixLength + kEncodedDataLength,
                "unexpected QR input length");
  memcpy(out_buf, kPrefix, kPrefixLength);
  memcpy(&out_buf[kPrefixLength], base64_qr_data.data(), kEncodedDataLength);
  return base::span<uint8_t>(out_buf, kPrefixLength + kEncodedDataLength);
}

}  // anonymous namespace

class AuthenticatorQRViewCentered : public views::View {
 public:
  explicit AuthenticatorQRViewCentered(base::span<const uint8_t> qr_data) {
    views::BoxLayout* layout =
        SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kHorizontal));
    layout->set_main_axis_alignment(
        views::BoxLayout::MainAxisAlignment::kCenter);
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);
    qr_view_ = new QRView(qr_data);
    AddChildView(qr_view_);
  }

  void RefreshQRCode(base::span<const uint8_t> new_qr_data) {
    qr_view_->RefreshQRCode(new_qr_data);
  }

  QRView* qr_view_;
};

AuthenticatorQRSheetView::AuthenticatorQRSheetView(
    std::unique_ptr<AuthenticatorQRSheetModel> sheet_model)
    : AuthenticatorRequestSheetView(std::move(sheet_model)),
      qr_generator_key_(reinterpret_cast<AuthenticatorQRSheetModel*>(model())
                            ->dialog_model()
                            ->qr_generator_key()) {}

AuthenticatorQRSheetView::~AuthenticatorQRSheetView() = default;

std::unique_ptr<views::View>
AuthenticatorQRSheetView::BuildStepSpecificContent() {
  uint8_t qr_data_buf[QRCode::kInputBytes];
  auto qr_view = std::make_unique<AuthenticatorQRViewCentered>(
      QRDataForCurrentTime(qr_data_buf, qr_generator_key_));
  qr_view_ = qr_view.get();

  timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(600), this,
               &AuthenticatorQRSheetView::Update);
  return qr_view;
}

void AuthenticatorQRSheetView::Update() {
  uint8_t qr_data_buf[QRCode::kInputBytes];
  qr_view_->RefreshQRCode(QRDataForCurrentTime(qr_data_buf, qr_generator_key_));
}
