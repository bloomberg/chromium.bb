// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/clipboard_bubble.h"

#include "base/bind.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_clipboard_bubble_constants.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_utils.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/styled_label.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/style/color_provider.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/lacros/browser_service_lacros.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace policy {

namespace {

// The corner radius of the bubble.
constexpr int kBubbleCornerRadius = 8;
constexpr gfx::RoundedCornersF kCornerRadii(kBubbleCornerRadius);

// TODO(crbug.com/1261496) Enable dynamic UI color & theme in lacros
#if BUILDFLAG(IS_CHROMEOS_ASH)
// The blur radius for the bubble background.
constexpr int kBubbleBlurRadius = 80;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// The size of the managed icon.
constexpr int kManagedIconSize = 20;

// The maximum width of the bubble.
constexpr int kBubbleWidth = 360;

// The spacing between the icon and label in the bubble.
constexpr int kIconLabelSpacing = 16;

// The padding which separates the bubble border with its inner contents.
constexpr int kBubblePadding = 16;

// The line height of the bubble text.
constexpr int kLineHeight = 20;

// The insets of the bubble borders.
constexpr gfx::Insets kBubbleBorderInsets(1);

// The font name of the text used in the bubble.
constexpr char kTextFontName[] = "Roboto";

// The font size of the text used in the bubble.
constexpr int kTextFontSize = 13;

// The height of the dismiss button.
constexpr int kButtonHeight = 32;

// The padding which separates the button border with its inner contents.
constexpr int kButtonPadding = 16;

// The spacing between the button border and label.
constexpr int kButtonLabelSpacing = 8;

// The spacing between the buttons.
constexpr int kButtonsSpacing = 8;

class Button : public views::LabelButton {
 public:
  METADATA_HEADER(Button);
  explicit Button(const std::u16string& button_label) {
    SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_CENTER);

    SetText(button_label);

    const gfx::FontList font_list = GetFontList();
    label()->SetFontList(font_list);

#if BUILDFLAG(IS_CHROMEOS_ASH)
    const SkColor text_color = ash::ColorProvider::Get()->GetContentLayerColor(
        ash::ColorProvider::ContentLayerType::kButtonLabelColorBlue);
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
    // TODO(crbug.com/1261496) Enable dynamic UI color & theme in lacros
    const SkColor text_color = SK_ColorBLUE;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    SetTextColor(ButtonState::STATE_NORMAL, text_color);
    SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_CENTER);
    SetSize({gfx::GetStringWidth(button_label, font_list) + 2 * kButtonPadding,
             kButtonHeight});
  }

  Button(const Button&) = delete;
  Button& operator=(const Button&) = delete;
  ~Button() override = default;

  int GetLabelWidth() const { return label()->bounds().width(); }

  static gfx::FontList GetFontList() {
    return gfx::FontList({kTextFontName}, gfx::Font::NORMAL, kTextFontSize,
                         gfx::Font::Weight::MEDIUM);
  }
};

void OnLearnMoreLinkClicked() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::NewWindowDelegate::GetInstance()->OpenUrl(
      GURL(kDlpLearnMoreUrl), /*from_user_interaction=*/true);
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  auto browser_service = std::make_unique<BrowserServiceLacros>();
  browser_service->OpenUrl(GURL(kDlpLearnMoreUrl), base::DoNothing());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

}  // namespace

BEGIN_METADATA(Button, views::LabelButton)
ADD_READONLY_PROPERTY_METADATA(int, LabelWidth)
END_METADATA

ClipboardBubbleView::ClipboardBubbleView(const std::u16string& text) {
  SetPaintToLayer(ui::LAYER_SOLID_COLOR);
// TODO(crbug.com/1261496) Enable dynamic UI color & theme in lacros
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::ColorProvider* color_provider = ash::ColorProvider::Get();
  SkColor background_color = color_provider->GetBaseLayerColor(
      ash::ColorProvider::BaseLayerType::kTransparent80);
  layer()->SetColor(background_color);
  if (ash::features::IsBackgroundBlurEnabled())
    layer()->SetBackgroundBlur(kBubbleBlurRadius);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  layer()->SetRoundedCornerRadius(kCornerRadii);

  // Add the managed icon.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  const SkColor icon_color = color_provider->GetContentLayerColor(
      ash::ColorProvider::ContentLayerType::kIconColorPrimary);
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  // TODO(crbug.com/1261496) Enable dynamic UI color & theme in lacros
  const SkColor icon_color = SK_ColorGRAY;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  managed_icon_ = AddChildView(std::make_unique<views::ImageView>());
  managed_icon_->SetPaintToLayer();
  managed_icon_->layer()->SetFillsBoundsOpaquely(false);
  managed_icon_->SetBounds(kBubblePadding, kBubblePadding, kManagedIconSize,
                           kManagedIconSize);
  managed_icon_->SetImage(gfx::CreateVectorIcon(vector_icons::kBusinessIcon,
                                                kManagedIconSize, icon_color));

  // Add the bubble text.
  label_ = AddChildView(std::make_unique<views::StyledLabel>());
  label_->SetPaintToLayer();
  label_->layer()->SetFillsBoundsOpaquely(false);
  label_->SetPosition(gfx::Point(
      kBubblePadding + kManagedIconSize + kIconLabelSpacing, kBubblePadding));
  label_->SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT);

  std::u16string learn_more_link_text =
      l10n_util::GetStringUTF16(IDS_LEARN_MORE);
  std::u16string full_text = l10n_util::GetStringFUTF16(
      IDS_POLICY_DLP_CLIPBOARD_BUBBLE_MESSAGE, text, learn_more_link_text);
  const int main_message_length =
      full_text.size() - learn_more_link_text.size();

  // Set the styling of the main text.
  // TODO(crbug.com/1150741): Handle RTL.
  views::StyledLabel::RangeStyleInfo message_style;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // TODO(crbug.com/1261496) Enable dynamic UI color & theme in lacros
  message_style.override_color = color_provider->GetContentLayerColor(
      ash::ColorProvider::ContentLayerType::kTextColorPrimary);
  label_->SetDisplayedOnBackgroundColor(background_color);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  label_->SetText(full_text);
  label_->AddStyleRange(gfx::Range(0, main_message_length), message_style);

  // Add "Learn more" link.
  views::StyledLabel::RangeStyleInfo link_style =
      views::StyledLabel::RangeStyleInfo::CreateForLink(
          base::BindRepeating(&OnLearnMoreLinkClicked));
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // TODO(crbug.com/1261496) Enable dynamic UI color & theme in lacros
  link_style.override_color = color_provider->GetContentLayerColor(
      ash::ColorProvider::ContentLayerType::kTextColorURL);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  label_->AddStyleRange(gfx::Range(main_message_length, full_text.size()),
                        link_style);
  label_->SetLineHeight(kLineHeight);
  label_->SizeToFit(kBubbleWidth - 2 * kBubblePadding - kManagedIconSize -
                    kIconLabelSpacing);
  label_->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);

  // Bubble borders
  border_ = AddChildView(std::make_unique<views::ImageView>());
  border_->SetPaintToLayer();
  border_->layer()->SetFillsBoundsOpaquely(false);
  auto shadow_border = std::make_unique<views::BubbleBorder>(
      views::BubbleBorder::FLOAT, views::BubbleBorder::STANDARD_SHADOW,
      SK_ColorTRANSPARENT);
  shadow_border->SetCornerRadius(kBubbleCornerRadius);
  shadow_border->set_background_color(SK_ColorTRANSPARENT);
  shadow_border->set_insets(kBubbleBorderInsets);
  border_->SetSize({kBubbleWidth, INT_MAX});
  border_->SetBorder(std::move(shadow_border));
  border_->SetCanProcessEventsWithinSubtree(false);
}

ClipboardBubbleView::~ClipboardBubbleView() = default;

void ClipboardBubbleView::UpdateBorderSize(const gfx::Size& size) {
  border_->SetSize(size);
}

BEGIN_METADATA(ClipboardBubbleView, views::View)
ADD_READONLY_PROPERTY_METADATA(gfx::Size, BubbleSize)
END_METADATA

ClipboardBlockBubble::ClipboardBlockBubble(const std::u16string& text)
    : ClipboardBubbleView(text) {
  // Add "Got it" button.
  std::u16string button_label =
      l10n_util::GetStringUTF16(IDS_POLICY_DLP_CLIPBOARD_BLOCK_DISMISS_BUTTON);
  button_ = AddChildView(std::make_unique<Button>(button_label));
  button_->SetPaintToLayer();
  button_->layer()->SetFillsBoundsOpaquely(false);
  button_->SetPosition(
      gfx::Point(kBubbleWidth - kBubblePadding - button_->width(),
                 kBubblePadding + label_->height() + kButtonLabelSpacing));

  UpdateBorderSize(GetBubbleSize());
}

ClipboardBlockBubble::~ClipboardBlockBubble() = default;

gfx::Size ClipboardBlockBubble::GetBubbleSize() const {
  DCHECK(label_);
  DCHECK(button_);
  return {kBubbleWidth, 2 * kBubblePadding + label_->bounds().height() +
                            kButtonLabelSpacing + button_->height()};
}

void ClipboardBlockBubble::SetDismissCallback(
    base::RepeatingCallback<void()> cb) {
  DCHECK(button_);
  button_->SetCallback(std::move(cb));
}

BEGIN_METADATA(ClipboardBlockBubble, ClipboardBubbleView)
END_METADATA

ClipboardWarnBubble::ClipboardWarnBubble(const std::u16string& text)
    : ClipboardBubbleView(text) {
  // Add paste button.
  std::u16string paste_label =
      l10n_util::GetStringUTF16(IDS_POLICY_DLP_CLIPBOARD_WARN_PROCEED_BUTTON);
  paste_button_ = AddChildView(std::make_unique<Button>(paste_label));
  paste_button_->SetPaintToLayer();
  paste_button_->layer()->SetFillsBoundsOpaquely(false);
  paste_button_->SetPosition(
      gfx::Point(kBubbleWidth - kBubblePadding - paste_button_->width(),
                 kBubblePadding + label_->height() + kButtonLabelSpacing));

  // Add cancel button.
  std::u16string cancel_label =
      l10n_util::GetStringUTF16(IDS_POLICY_DLP_CLIPBOARD_WARN_DISMISS_BUTTON);
  cancel_button_ = AddChildView(std::make_unique<Button>(cancel_label));
  cancel_button_->SetPaintToLayer();
  cancel_button_->layer()->SetFillsBoundsOpaquely(false);
  cancel_button_->SetPosition(
      gfx::Point(kBubbleWidth - kBubblePadding - paste_button_->width() -
                     kButtonsSpacing - cancel_button_->width(),
                 kBubblePadding + label_->height() + kButtonLabelSpacing));

  UpdateBorderSize(GetBubbleSize());
}

ClipboardWarnBubble::~ClipboardWarnBubble() = default;

gfx::Size ClipboardWarnBubble::GetBubbleSize() const {
  DCHECK(label_);
  DCHECK(cancel_button_);
  DCHECK(paste_button_);
  return {kBubbleWidth, 2 * kBubblePadding + label_->bounds().height() +
                            kButtonLabelSpacing + paste_button_->height()};
}

void ClipboardWarnBubble::SetDismissCallback(
    base::RepeatingCallback<void()> cb) {
  DCHECK(cancel_button_);
  cancel_button_->SetCallback(std::move(cb));
}

void ClipboardWarnBubble::SetProceedCallback(
    base::RepeatingCallback<void()> cb) {
  DCHECK(paste_button_);
  paste_button_->SetCallback(std::move(cb));
}

BEGIN_METADATA(ClipboardWarnBubble, ClipboardBubbleView)
END_METADATA

}  // namespace policy
