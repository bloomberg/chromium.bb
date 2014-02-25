// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/speech_view.h"

#include "base/strings/utf_string_conversions.h"
#include "grit/ui_resources.h"
#include "grit/ui_strings.h"
#include "ui/app_list/app_list_model.h"
#include "ui/app_list/app_list_view_delegate.h"
#include "ui/app_list/speech_ui_model.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/views/animation/bounds_animator.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/shadow_border.h"

namespace app_list {

namespace {

const int kShadowOffset = 1;
const int kShadowBlur = 4;
const int kSpeechViewMaxHeight = 300;
const int kMicButtonMargin = 12;
const int kTextMargin = 32;
const int kLogoMarginLeft = 30;
const int kLogoMarginTop = 28;
const int kLogoWidth = 104;
const int kLogoHeight = 36;
const int kIndicatorCenterOffsetY = -1;
const int kIndicatorRadiusMinOffset = -3;
const int kIndicatorRadiusMax = 100;
const int kIndicatorAnimationDuration = 100;
const SkColor kShadowColor = SkColorSetARGB(0.3 * 255, 0, 0, 0);
const SkColor kHintTextColor = SkColorSetRGB(119, 119, 119);
const SkColor kResultTextColor = SkColorSetRGB(178, 178, 178);
const SkColor kSoundLevelIndicatorColor = SkColorSetRGB(219, 219, 219);

class SoundLevelIndicator : public views::View {
 public:
  SoundLevelIndicator();
  virtual ~SoundLevelIndicator();

 private:
  // Overridden from views::View:
  virtual void Paint(gfx::Canvas* canvas) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(SoundLevelIndicator);
};

SoundLevelIndicator::SoundLevelIndicator() {}

SoundLevelIndicator::~SoundLevelIndicator() {}

void SoundLevelIndicator::Paint(gfx::Canvas* canvas) {
  SkPaint paint;
  paint.setStyle(SkPaint::kFill_Style);
  paint.setColor(kSoundLevelIndicatorColor);
  paint.setAntiAlias(true);
  canvas->DrawCircle(bounds().CenterPoint(), width() / 2, paint);
}

// MicButton is an image button with circular hit area.
class MicButton : public views::ImageButton {
 public:
  explicit MicButton(views::ButtonListener* listener);
  virtual ~MicButton();

 private:
  // Overridden from views::View:
  virtual bool HitTestRect(const gfx::Rect& rect) const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(MicButton);
};

MicButton::MicButton(views::ButtonListener* listener)
    : views::ImageButton(listener) {}

MicButton::~MicButton() {}

bool MicButton::HitTestRect(const gfx::Rect& rect) const {
  if (!views::ImageButton::HitTestRect(rect))
    return false;

  gfx::Rect local_bounds = GetLocalBounds();
  int radius = local_bounds.width() / 2;
  return (rect.origin() - local_bounds.CenterPoint()).LengthSquared() <
      radius * radius;
}

}  // namespace

// static

SpeechView::SpeechView(AppListViewDelegate* delegate)
    : delegate_(delegate),
      logo_(NULL) {
  SetBorder(scoped_ptr<views::Border>(
      new views::ShadowBorder(kShadowBlur,
                              kShadowColor,
                              kShadowOffset,  // Vertical offset.
                              0)));

  // To keep the painting order of the border and the background, this class
  // actually has a single child of 'container' which has white background and
  // contains all components.
  views::View* container = new views::View();
  container->set_background(
      views::Background::CreateSolidBackground(SK_ColorWHITE));

  const gfx::ImageSkia& logo_image = delegate_->GetSpeechUI()->logo();
  if (!logo_image.isNull()) {
    logo_ = new views::ImageView();
    logo_->SetImage(&logo_image);
    container->AddChildView(logo_);
  }

  indicator_ = new SoundLevelIndicator();
  indicator_->SetVisible(false);
  container->AddChildView(indicator_);

  mic_button_ = new MicButton(this);
  container->AddChildView(mic_button_);

  // TODO(mukai): use BoundedLabel to cap 2 lines.
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  speech_result_ = new views::Label(
      base::string16(), bundle.GetFontList(ui::ResourceBundle::LargeFont));
  speech_result_->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  speech_result_->SetMultiLine(true);
  container->AddChildView(speech_result_);

  AddChildView(container);

  delegate_->GetSpeechUI()->AddObserver(this);
  indicator_animator_.reset(new views::BoundsAnimator(container));
  indicator_animator_->SetAnimationDuration(kIndicatorAnimationDuration);
  indicator_animator_->set_tween_type(gfx::Tween::LINEAR);

  Reset();
}

SpeechView::~SpeechView() {
  delegate_->GetSpeechUI()->RemoveObserver(this);
}

void SpeechView::Reset() {
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  speech_result_->SetText(l10n_util::GetStringUTF16(
      IDS_APP_LIST_SPEECH_HINT_TEXT));
  speech_result_->SetEnabledColor(kHintTextColor);
  mic_button_->SetImage(views::Button::STATE_NORMAL,
                        bundle.GetImageSkiaNamed(IDR_APP_LIST_SPEECH_MIC_ON));
}

int SpeechView::GetIndicatorRadius(uint8 level) {
  int radius_min = mic_button_->width() / 2 + kIndicatorRadiusMinOffset;
  int range = kIndicatorRadiusMax - radius_min;
  return level * range / kuint8max + radius_min;
}

void SpeechView::Layout() {
  views::View* container = child_at(0);
  container->SetBoundsRect(GetContentsBounds());

  // Because container is a pure View, this class should layout its children.
  const gfx::Rect contents_bounds = container->GetContentsBounds();
  if (logo_)
    logo_->SetBounds(kLogoMarginLeft, kLogoMarginTop, kLogoWidth, kLogoHeight);
  gfx::Size mic_size = mic_button_->GetPreferredSize();
  gfx::Point mic_origin(
      contents_bounds.right() - kMicButtonMargin - mic_size.width(),
      contents_bounds.y() + kMicButtonMargin);
  mic_button_->SetBoundsRect(gfx::Rect(mic_origin, mic_size));

  int speech_width = contents_bounds.width() - kTextMargin * 2;
  speech_result_->SizeToFit(speech_width);
  int speech_height = speech_result_->GetHeightForWidth(speech_width);
  speech_result_->SetBounds(
      contents_bounds.x() + kTextMargin,
      contents_bounds.bottom() - kTextMargin - speech_height,
      speech_width,
      speech_height);
}

gfx::Size SpeechView::GetPreferredSize() {
  return gfx::Size(0, kSpeechViewMaxHeight);
}

void SpeechView::ButtonPressed(views::Button* sender, const ui::Event& event) {
  delegate_->ToggleSpeechRecognition();
}

void SpeechView::OnSpeechSoundLevelChanged(uint8 level) {
  if (!visible())
    return;

  gfx::Point origin = mic_button_->bounds().CenterPoint();
  int radius = GetIndicatorRadius(level);
  origin.Offset(-radius, -radius + kIndicatorCenterOffsetY);
  gfx::Rect indicator_bounds =
      gfx::Rect(origin, gfx::Size(radius * 2, radius * 2));
  if (indicator_->visible()) {
    indicator_animator_->AnimateViewTo(indicator_, indicator_bounds);
  } else {
    indicator_->SetVisible(true);
    indicator_->SetBoundsRect(indicator_bounds);
  }
}

void SpeechView::OnSpeechResult(const base::string16& result,
                                bool is_final) {
  speech_result_->SetText(result);
  speech_result_->SetEnabledColor(kResultTextColor);
}

void SpeechView::OnSpeechRecognitionStateChanged(
    SpeechRecognitionState new_state) {
  int resource_id = IDR_APP_LIST_SPEECH_MIC_OFF;
  if (new_state == SPEECH_RECOGNITION_RECOGNIZING)
    resource_id = IDR_APP_LIST_SPEECH_MIC_ON;
  else if (new_state == SPEECH_RECOGNITION_IN_SPEECH)
    resource_id = IDR_APP_LIST_SPEECH_MIC_RECORDING;

  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  mic_button_->SetImage(views::Button::STATE_NORMAL,
                        bundle.GetImageSkiaNamed(resource_id));
}

}  // namespace app_list
