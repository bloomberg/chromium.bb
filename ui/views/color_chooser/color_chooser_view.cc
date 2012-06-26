// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/color_chooser/color_chooser_view.h"

#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "ui/gfx/canvas.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/views/color_chooser/color_chooser_listener.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/events/event.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace {

const int kHueBarWidth = 20;
const int kSaturationValueSize = 200;
const int kMarginWidth = 5;
const int kSaturationValueIndicatorSize = 3;
const int kHueIndicatorSize = 3;

string16 GetColorText(SkColor color) {
  return ASCIIToUTF16(base::StringPrintf("#%02x%02x%02x",
                                         SkColorGetR(color),
                                         SkColorGetG(color),
                                         SkColorGetB(color)));
}

bool GetColorFromText(const string16& text, SkColor* result) {
  if (text.size() != 6 && !(text.size() == 7 && text[0] == '#'))
    return false;

  std::string input = UTF16ToUTF8((text.size() == 6) ? text : text.substr(1));
  std::vector<uint8> hex;
  if (!base::HexStringToBytes(input, &hex))
    return false;

  *result = SkColorSetRGB(hex[0], hex[1], hex[2]);
  return true;
}

}  // namespace

namespace views {

// The class to choose the hue of the color.  It draws a vertical bar and
// the indicator for the currently selected hue.
class ColorChooserView::HueView : public View {
 public:
  explicit HueView(ColorChooserView* chooser_view);

  void OnHueChanged(SkScalar hue);

 private:
  // View overrides:
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;
  virtual bool OnMousePressed(const MouseEvent& event) OVERRIDE;
  virtual bool OnMouseDragged(const MouseEvent& event) OVERRIDE;

  ColorChooserView* chooser_view_;
  int level_;

  DISALLOW_COPY_AND_ASSIGN(HueView);
};

ColorChooserView::HueView::HueView(ColorChooserView* chooser_view)
    : chooser_view_(chooser_view),
      level_(0) {
  set_focusable(false);
}

void ColorChooserView::HueView::OnHueChanged(SkScalar hue) {
  SkScalar height = SkIntToScalar(GetPreferredSize().height() - 1);
  SkScalar hue_max = SkIntToScalar(360);
  int level = SkScalarDiv(SkScalarMul(hue_max - hue, height), hue_max);
  if (level_ != level) {
    level_ = level;
    SchedulePaint();
  }
}

gfx::Size ColorChooserView::HueView::GetPreferredSize() {
  // We put indicators on the both sides of the hue bar.
  return gfx::Size(kHueBarWidth + kHueIndicatorSize * 2, kSaturationValueSize);
}

void ColorChooserView::HueView::OnPaint(gfx::Canvas* canvas) {
  SkScalar hsv[3];
  // In the hue bar, saturation and value for the color should be always 100%.
  hsv[1] = SK_Scalar1;
  hsv[2] = SK_Scalar1;

  for (int y = 0; y < height(); ++y) {
    hsv[0] = SkScalarDiv(SkScalarMul(SkIntToScalar(360),
                                     SkIntToScalar(height() - 1 - y)),
                    SkIntToScalar(height() - 1));
    canvas->DrawLine(gfx::Point(kHueIndicatorSize, y),
                     gfx::Point(width() - kHueIndicatorSize, y),
                     SkHSVToColor(hsv));
  }

  // Put the triangular indicators besides.
  SkPath left_indicator_path;
  SkPath right_indicator_path;
  left_indicator_path.moveTo(
      SK_ScalarHalf, SkIntToScalar(level_ - kHueIndicatorSize));
  left_indicator_path.lineTo(
      kHueIndicatorSize, SkIntToScalar(level_));
  left_indicator_path.lineTo(
      SK_ScalarHalf, SkIntToScalar(level_ + kHueIndicatorSize));
  left_indicator_path.lineTo(
      SK_ScalarHalf, SkIntToScalar(level_ - kHueIndicatorSize));
  right_indicator_path.moveTo(
      SkIntToScalar(width()) - SK_ScalarHalf,
      SkIntToScalar(level_ - kHueIndicatorSize));
  right_indicator_path.lineTo(
      SkIntToScalar(width() - kHueIndicatorSize) - SK_ScalarHalf,
      SkIntToScalar(level_));
  right_indicator_path.lineTo(
      SkIntToScalar(width()) - SK_ScalarHalf,
      SkIntToScalar(level_ + kHueIndicatorSize));
  right_indicator_path.lineTo(
      SkIntToScalar(width()) - SK_ScalarHalf,
      SkIntToScalar(level_ - kHueIndicatorSize));

  SkPaint indicator_paint;
  indicator_paint.setColor(SK_ColorBLACK);
  indicator_paint.setStyle(SkPaint::kStroke_Style);
  canvas->DrawPath(left_indicator_path, indicator_paint);
  canvas->DrawPath(right_indicator_path, indicator_paint);
}

bool ColorChooserView::HueView::OnMousePressed(const MouseEvent& event) {
  level_ = std::max(0, std::min(height() - 1, event.y()));
  chooser_view_->OnHueChosen(SkScalarDiv(
      SkScalarMul(SkIntToScalar(360), SkIntToScalar(height() - 1 - level_)),
      SkIntToScalar(height() - 1)));
  return true;
}

bool ColorChooserView::HueView::OnMouseDragged(const MouseEvent& event) {
  return OnMousePressed(event);
}

// The class to choose the saturation and the value of the color.  It draws
// a square area and the indicator for the currently selected saturation and
// value.
class ColorChooserView::SaturationValueView : public View {
 public:
  explicit SaturationValueView(ColorChooserView* chooser_view);

  void OnHueChanged(SkScalar hue);
  void OnSaturationValueChanged(SkScalar saturation, SkScalar value);

 private:
  // View overrides:
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;
  virtual bool OnMousePressed(const MouseEvent& event) OVERRIDE;
  virtual bool OnMouseDragged(const MouseEvent& event) OVERRIDE;

  ColorChooserView* chooser_view_;
  SkScalar hue_;
  gfx::Point marker_position_;

  DISALLOW_COPY_AND_ASSIGN(SaturationValueView);
};

ColorChooserView::SaturationValueView::SaturationValueView(
    ColorChooserView* chooser_view)
    : chooser_view_(chooser_view), hue_(0) {
  set_focusable(false);
}

void ColorChooserView::SaturationValueView::OnHueChanged(SkScalar hue) {
  if (hue_ != hue) {
    hue_ = hue;
    SchedulePaint();
  }
}

void ColorChooserView::SaturationValueView::OnSaturationValueChanged(
    SkScalar saturation,
    SkScalar value) {
  SkScalar scalar_size = SkIntToScalar(kSaturationValueSize - 1);
  int x = SkScalarFloorToInt(SkScalarMul(saturation, scalar_size));
  int y = SkScalarFloorToInt(SkScalarMul(SK_Scalar1 - value, scalar_size));
  if (gfx::Point(x, y) == marker_position_)
    return;

  marker_position_.set_x(x);
  marker_position_.set_y(y);
  SchedulePaint();
  chooser_view_->OnSaturationValueChosen(saturation, value);
}

gfx::Size ColorChooserView::SaturationValueView::GetPreferredSize() {
  return gfx::Size(kSaturationValueSize, kSaturationValueSize);
}

void ColorChooserView::SaturationValueView::OnPaint(gfx::Canvas* canvas) {
  SkScalar hsv[3];
  hsv[0] = hue_;
  SkScalar scalar_size = SkIntToScalar(kSaturationValueSize - 1);
  for (int x = 0; x < width(); ++x) {
    hsv[1] = SkScalarDiv(SkIntToScalar(x), scalar_size);
    for (int y = 0; y < height(); ++y) {
      hsv[2] = SK_Scalar1 - SkScalarDiv(SkIntToScalar(y), scalar_size);
      SkPaint paint;
      paint.setColor(SkHSVToColor(255, hsv));
      canvas->DrawPoint(gfx::Point(x, y), paint);
    }
  }

  // The background is very dark at the bottom of the view.  Use a white
  // marker in that case.
  SkColor indicator_color =
      (marker_position_.y() > width() * 3 / 4) ? SK_ColorWHITE : SK_ColorBLACK;
  // Draw a crosshair indicator but do not draw its center to see the selected
  // saturation/value.  Note that the DrawLine() doesn't draw the right-bottom
  // pixel.
  canvas->DrawLine(
      gfx::Point(marker_position_.x(),
                 marker_position_.y() - kSaturationValueIndicatorSize),
      gfx::Point(marker_position_.x(),
                 marker_position_.y()),
      indicator_color);
  canvas->DrawLine(
      gfx::Point(marker_position_.x(),
                 marker_position_.y() + kSaturationValueIndicatorSize + 1),
      gfx::Point(marker_position_.x(),
                 marker_position_.y() + 1),
      indicator_color);
  canvas->DrawLine(
      gfx::Point(marker_position_.x() - kSaturationValueIndicatorSize,
                 marker_position_.y()),
      gfx::Point(marker_position_.x(),
                 marker_position_.y()),
      indicator_color);
  canvas->DrawLine(
      gfx::Point(marker_position_.x() + kSaturationValueIndicatorSize + 1,
                 marker_position_.y()),
      gfx::Point(marker_position_.x() + 1,
                 marker_position_.y()),
      indicator_color);
}

bool ColorChooserView::SaturationValueView::OnMousePressed(
    const MouseEvent& event) {
  SkScalar scalar_size = SkIntToScalar(kSaturationValueSize);
  SkScalar saturation = SkScalarDiv(SkIntToScalar(event.x()), scalar_size);
  SkScalar value = SK_Scalar1 - SkScalarDiv(
      SkIntToScalar(event.y()), scalar_size);
  saturation = SkScalarPin(saturation, 0, SK_Scalar1);
  value = SkScalarPin(value, 0, SK_Scalar1);
  OnSaturationValueChanged(saturation, value);
  return true;
}

bool ColorChooserView::SaturationValueView::OnMouseDragged(
    const MouseEvent& event) {
  return OnMousePressed(event);
}


ColorChooserView::ColorChooserView(ColorChooserListener* listener,
                                   SkColor initial_color)
    : listener_(listener) {
  DCHECK(listener_);

  set_focusable(false);
  set_background(Background::CreateSolidBackground(SK_ColorLTGRAY));
  SetLayoutManager(new BoxLayout(BoxLayout::kVertical, kMarginWidth,
                                 kMarginWidth, kMarginWidth));

  View* container = new View();
  container->SetLayoutManager(new BoxLayout(BoxLayout::kHorizontal, 0, 0,
                                            kMarginWidth));
  saturation_value_ = new SaturationValueView(this);
  container->AddChildView(saturation_value_);
  hue_ = new HueView(this);
  container->AddChildView(hue_);
  AddChildView(container);

  textfield_ = new Textfield();
  textfield_->SetController(this);
  AddChildView(textfield_);

  OnColorChanged(initial_color);
}

ColorChooserView::~ColorChooserView() {
}

void ColorChooserView::OnColorChanged(SkColor color) {
  SkColorToHSV(color, hsv_);
  hue_->OnHueChanged(hsv_[0]);
  saturation_value_->OnHueChanged(hsv_[0]);
  saturation_value_->OnSaturationValueChanged(hsv_[1], hsv_[2]);
  textfield_->SetText(GetColorText(color));
}

void ColorChooserView::OnHueChosen(SkScalar hue) {
  hsv_[0] = hue;
  SkColor color = SkHSVToColor(255, hsv_);
  listener_->OnColorChosen(color);
  saturation_value_->OnHueChanged(hue);
  textfield_->SetText(GetColorText(color));
}

void ColorChooserView::OnSaturationValueChosen(SkScalar saturation,
                                               SkScalar value) {
  hsv_[1] = saturation;
  hsv_[2] = value;
  SkColor color = SkHSVToColor(255, hsv_);
  listener_->OnColorChosen(color);
  textfield_->SetText(GetColorText(color));
}

View* ColorChooserView::GetInitiallyFocusedView() {
  return textfield_;
}

ui::ModalType ColorChooserView::GetModalType() const {
  return ui::MODAL_TYPE_WINDOW;
}

void ColorChooserView::WindowClosing() {
  if (listener_)
    listener_->OnColorChooserDialogClosed();
}

View* ColorChooserView::GetContentsView() {
  return this;
}

void ColorChooserView::ContentsChanged(Textfield* sender,
                                       const string16& new_contents) {
  SkColor color = SK_ColorBLACK;
  if (GetColorFromText(new_contents, &color)) {
    SkColorToHSV(color, hsv_);
    listener_->OnColorChosen(color);
    hue_->OnHueChanged(hsv_[0]);
    saturation_value_->OnHueChanged(hsv_[0]);
    saturation_value_->OnSaturationValueChanged(hsv_[1], hsv_[2]);
  }
}

bool ColorChooserView::HandleKeyEvent(Textfield* sender,
                                      const KeyEvent& key_event) {
  if (key_event.key_code() != ui::VKEY_RETURN &&
      key_event.key_code() != ui::VKEY_ESCAPE)
    return false;

  GetWidget()->Close();
  return true;
}

}  // namespace views
