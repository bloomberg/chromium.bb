// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/colored_dialog_example.h"

#include <memory>
#include <utility>

#include "base/containers/adapters.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/native_theme/native_theme_color_id.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/examples/grit/views_examples_resources.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/vector_icons.h"
#include "ui/views/widget/widget.h"

namespace views {
namespace examples {

class ThemeTrackingCheckbox : public views::Checkbox,
                              public views::ButtonListener {
 public:
  explicit ThemeTrackingCheckbox(const base::string16& label)
      : Checkbox(label, this) {}
  ThemeTrackingCheckbox(const ThemeTrackingCheckbox&) = delete;
  ThemeTrackingCheckbox& operator=(const ThemeTrackingCheckbox&) = delete;
  ~ThemeTrackingCheckbox() override = default;

  // views::Checkbox
  void OnThemeChanged() override {
    views::Checkbox::OnThemeChanged();

    SetChecked(GetNativeTheme()->ShouldUseDarkColors());
  }

  // ButtonListener
  void ButtonPressed(views::Button* sender, const ui::Event& event) override {
    GetNativeTheme()->set_use_dark_colors(GetChecked());
    GetWidget()->ThemeChanged();
  }
};

class TextVectorImageButton : public views::MdTextButton {
 public:
  TextVectorImageButton(ButtonListener* listener,
                        const base::string16& text,
                        const gfx::VectorIcon& icon)
      : MdTextButton(listener, style::CONTEXT_BUTTON_MD), icon_(icon) {
    SetText(text);
  }
  TextVectorImageButton(const TextVectorImageButton&) = delete;
  TextVectorImageButton& operator=(const TextVectorImageButton&) = delete;
  ~TextVectorImageButton() override = default;

  void OnThemeChanged() override {
    views::MdTextButton::OnThemeChanged();

    // Use the text color for the associated vector image.
    SetImage(views::Button::ButtonState::STATE_NORMAL,
             gfx::CreateVectorIcon(icon_, label()->GetEnabledColor()));
  }

 private:
  const gfx::VectorIcon& icon_;
};

ColoredDialog::ColoredDialog(AcceptCallback accept_callback) {
  SetAcceptCallback(base::BindOnce(
      [](ColoredDialog* dialog, AcceptCallback callback) {
        std::move(callback).Run(dialog->textfield_->GetText());
      },
      base::Unretained(this), std::move(accept_callback)));

  SetTitle(l10n_util::GetStringUTF16(IDS_COLORED_DIALOG_TITLE));

  SetLayoutManager(std::make_unique<views::FillLayout>());
  set_margins(views::LayoutProvider::Get()->GetDialogInsetsForContentType(
      views::CONTROL, views::CONTROL));

  textfield_ = AddChildView(std::make_unique<views::Textfield>());
  textfield_->SetPlaceholderText(
      l10n_util::GetStringUTF16(IDS_COLORED_DIALOG_TEXTFIELD_PLACEHOLDER));
  textfield_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_COLORED_DIALOG_TEXTFIELD_AX_LABEL));
  textfield_->set_controller(this);

  SetButtonLabel(ui::DIALOG_BUTTON_OK,
                 l10n_util::GetStringUTF16(IDS_COLORED_DIALOG_SUBMIT_BUTTON));
  SetButtonEnabled(ui::DIALOG_BUTTON_OK, false);
}

ColoredDialog::~ColoredDialog() = default;

ui::ModalType ColoredDialog::GetModalType() const {
  return ui::MODAL_TYPE_WINDOW;
}

bool ColoredDialog::ShouldShowCloseButton() const {
  return false;
}

void ColoredDialog::ContentsChanged(Textfield* sender,
                                    const base::string16& new_contents) {
  SetButtonEnabled(ui::DIALOG_BUTTON_OK, !textfield_->GetText().empty());
  DialogModelChanged();
}

ColoredDialogChooser::ColoredDialogChooser() {
  views::LayoutProvider* provider = views::LayoutProvider::Get();
  const int vertical_spacing =
      provider->GetDistanceMetric(views::DISTANCE_UNRELATED_CONTROL_VERTICAL);
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      vertical_spacing));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);

  AddChildView(std::make_unique<ThemeTrackingCheckbox>(
      l10n_util::GetStringUTF16(IDS_COLORED_DIALOG_CHOOSER_CHECKBOX)));

  AddChildView(std::make_unique<TextVectorImageButton>(
      this, l10n_util::GetStringUTF16(IDS_COLORED_DIALOG_CHOOSER_BUTTON),
      views::kInfoIcon));

  confirmation_label_ = AddChildView(
      std::make_unique<views::Label>(base::string16(), style::CONTEXT_LABEL));
  confirmation_label_->SetVisible(false);
}

ColoredDialogChooser::~ColoredDialogChooser() = default;

void ColoredDialogChooser::ButtonPressed(Button* sender,
                                         const ui::Event& event) {
  // Create the colored dialog.
  views::Widget* widget = DialogDelegate::CreateDialogWidget(
      new ColoredDialog(base::BindOnce(&ColoredDialogChooser::OnFeedbackSubmit,
                                       base::Unretained(this))),
      nullptr, GetWidget()->GetNativeView());
  widget->Show();
}

void ColoredDialogChooser::OnFeedbackSubmit(base::string16 text) {
  constexpr base::TimeDelta kConfirmationDuration =
      base::TimeDelta::FromSeconds(3);

  confirmation_label_->SetText(l10n_util::GetStringFUTF16(
      IDS_COLORED_DIALOG_CHOOSER_CONFIRM_LABEL, text));
  confirmation_label_->SetVisible(true);

  confirmation_timer_.Start(
      FROM_HERE, kConfirmationDuration,
      base::BindOnce([](views::View* view) { view->SetVisible(false); },
                     confirmation_label_));
}

ColoredDialogExample::ColoredDialogExample() : ExampleBase("Colored Dialog") {}

ColoredDialogExample::~ColoredDialogExample() = default;

void ColoredDialogExample::CreateExampleView(views::View* container) {
  container->SetLayoutManager(std::make_unique<views::FillLayout>());
  container->AddChildView(std::make_unique<ColoredDialogChooser>());
}

}  // namespace examples
}  // namespace views
