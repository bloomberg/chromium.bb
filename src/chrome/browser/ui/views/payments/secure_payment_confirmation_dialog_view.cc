// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/secure_payment_confirmation_dialog_view.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/payments/payment_request_views_util.h"
#include "chrome/browser/ui/views/payments/secure_payment_confirmation_views_util.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/payments/content/payment_ui_observer.h"
#include "components/payments/content/secure_payment_confirmation_model.h"
#include "components/payments/core/sizes.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/layout/table_layout.h"

namespace payments {
namespace {

// Records UMA metric for the authentication dialog result.
void RecordAuthenticationDialogResult(
    const SecurePaymentConfirmationAuthenticationDialogResult result) {
  base::UmaHistogramEnumeration(
      "PaymentRequest.SecurePaymentConfirmation.Funnel."
      "AuthenticationDialogResult",
      result);
}

class BorderedRowView : public views::View {
 public:
  METADATA_HEADER(BorderedRowView);

  void OnThemeChanged() override {
    View::OnThemeChanged();
    SetBorder(views::CreateSolidSidedBorder(
        0, 0, 1, 0, GetColorProvider()->GetColor(ui::kColorSeparator)));
  }
};

BEGIN_METADATA(BorderedRowView, views::View)
END_METADATA

}  // namespace

// static
base::WeakPtr<SecurePaymentConfirmationView>
SecurePaymentConfirmationView::Create(
    const base::WeakPtr<PaymentUIObserver> payment_ui_observer) {
  // On desktop, the SecurePaymentConfirmationView object is memory managed by
  // the views:: machinery. It is deleted when the window is closed and
  // views::DialogDelegateView::DeleteDelegate() is called by its corresponding
  // views::Widget.
  return (new SecurePaymentConfirmationDialogView(
              /*observer_for_test=*/nullptr, payment_ui_observer))
      ->GetWeakPtr();
}

SecurePaymentConfirmationView::SecurePaymentConfirmationView() = default;
SecurePaymentConfirmationView::~SecurePaymentConfirmationView() = default;

SecurePaymentConfirmationDialogView::SecurePaymentConfirmationDialogView(
    base::WeakPtr<ObserverForTest> observer_for_test,
    const base::WeakPtr<PaymentUIObserver> ui_observer_for_test)
    : observer_for_test_(observer_for_test),
      ui_observer_for_test_(ui_observer_for_test) {}
SecurePaymentConfirmationDialogView::~SecurePaymentConfirmationDialogView() =
    default;

void SecurePaymentConfirmationDialogView::ShowDialog(
    content::WebContents* web_contents,
    base::WeakPtr<SecurePaymentConfirmationModel> model,
    VerifyCallback verify_callback,
    CancelCallback cancel_callback) {
  DCHECK(model);
  model_ = model;

  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));

  InitChildViews();

  OnModelUpdated();

  verify_callback_ = std::move(verify_callback);
  cancel_callback_ = std::move(cancel_callback);

  SetAcceptCallback(
      base::BindOnce(&SecurePaymentConfirmationDialogView::OnDialogAccepted,
                     weak_ptr_factory_.GetWeakPtr()));
  SetCancelCallback(
      base::BindOnce(&SecurePaymentConfirmationDialogView::OnDialogCancelled,
                     weak_ptr_factory_.GetWeakPtr()));
  SetCloseCallback(
      base::BindOnce(&SecurePaymentConfirmationDialogView::OnDialogClosed,
                     weak_ptr_factory_.GetWeakPtr()));

  SetModalType(ui::MODAL_TYPE_CHILD);

  constrained_window::ShowWebModalDialogViews(this, web_contents);

  // observer_for_test_ is used in views browsertests.
  if (observer_for_test_)
    observer_for_test_->OnDialogOpened();

  // ui_observer_for_test_ is used in platform browsertests.
  if (ui_observer_for_test_)
    ui_observer_for_test_->OnUIDisplayed();
}

void SecurePaymentConfirmationDialogView::OnDialogAccepted() {
  std::move(verify_callback_).Run();
  RecordAuthenticationDialogResult(
      SecurePaymentConfirmationAuthenticationDialogResult::kAccepted);

  if (observer_for_test_) {
    observer_for_test_->OnConfirmButtonPressed();
    observer_for_test_->OnDialogClosed();
  }
}

void SecurePaymentConfirmationDialogView::OnDialogCancelled() {
  std::move(cancel_callback_).Run();
  RecordAuthenticationDialogResult(
      SecurePaymentConfirmationAuthenticationDialogResult::kCanceled);

  if (observer_for_test_) {
    observer_for_test_->OnCancelButtonPressed();
    observer_for_test_->OnDialogClosed();
  }
}

void SecurePaymentConfirmationDialogView::OnDialogClosed() {
  std::move(cancel_callback_).Run();
  RecordAuthenticationDialogResult(
      SecurePaymentConfirmationAuthenticationDialogResult::kClosed);

  if (observer_for_test_) {
    observer_for_test_->OnDialogClosed();
  }
}

void SecurePaymentConfirmationDialogView::OnModelUpdated() {
  views::View* progress_bar =
      GetViewByID(static_cast<int>(DialogViewID::PROGRESS_BAR));
  if (progress_bar)
    progress_bar->SetVisible(model_->progress_bar_visible());

  SetButtonLabel(ui::DIALOG_BUTTON_OK, model_->verify_button_label());
  SetButtonEnabled(ui::DIALOG_BUTTON_OK, model_->verify_button_enabled());
  SetButtonLabel(ui::DIALOG_BUTTON_CANCEL, model_->cancel_button_label());
  SetButtonEnabled(ui::DIALOG_BUTTON_CANCEL, model_->cancel_button_enabled());

  SetAccessibleTitle(model_->title());
  UpdateLabelView(DialogViewID::TITLE, model_->title());
  UpdateLabelView(DialogViewID::MERCHANT_LABEL, model_->merchant_label());
  UpdateLabelView(DialogViewID::MERCHANT_VALUE, model_->merchant_value());
  UpdateLabelView(DialogViewID::INSTRUMENT_LABEL, model_->instrument_label());
  UpdateLabelView(DialogViewID::INSTRUMENT_VALUE, model_->instrument_value());

  // Update the instrument icon only if it's changed
  if (model_->instrument_icon() &&
      (model_->instrument_icon() != instrument_icon_ ||
       model_->instrument_icon()->getGenerationID() !=
           instrument_icon_generation_id_)) {
    instrument_icon_generation_id_ =
        model_->instrument_icon()->getGenerationID();
    gfx::ImageSkia image =
        gfx::ImageSkia::CreateFrom1xBitmap(*model_->instrument_icon())
            .DeepCopy();

    static_cast<views::ImageView*>(
        GetViewByID(static_cast<int>(DialogViewID::INSTRUMENT_ICON)))
        ->SetImage(image);
  }
  instrument_icon_ = model_->instrument_icon();

  UpdateLabelView(DialogViewID::TOTAL_LABEL, model_->total_label());
  UpdateLabelView(DialogViewID::TOTAL_VALUE, model_->total_value());
}

void SecurePaymentConfirmationDialogView::UpdateLabelView(
    DialogViewID id,
    const std::u16string& text) {
  static_cast<views::Label*>(GetViewByID(static_cast<int>(id)))->SetText(text);
}

void SecurePaymentConfirmationDialogView::HideDialog() {
  if (GetWidget())
    GetWidget()->Close();
}

bool SecurePaymentConfirmationDialogView::ShouldShowCloseButton() const {
  return false;
}

bool SecurePaymentConfirmationDialogView::Accept() {
  views::DialogDelegateView::Accept();
  // Returning "false" to keep the dialog open after "Confirm" button is
  // pressed, so the dialog can show a progress bar and wait for the user to use
  // their authenticator device.
  return false;
}

base::WeakPtr<SecurePaymentConfirmationDialogView>
SecurePaymentConfirmationDialogView::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void SecurePaymentConfirmationDialogView::InitChildViews() {
  RemoveAllChildViews();

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(), 0));

  AddChildView(CreateSecurePaymentConfirmationHeaderView(
      static_cast<int>(DialogViewID::PROGRESS_BAR),
      static_cast<int>(DialogViewID::HEADER_ICON)));

  AddChildView(CreateBodyView());

  InvalidateLayout();
}

// Creates the body including the title, the set of merchant, instrument, and
// total rows.
// +------------------------------------------+
// | Title                                    |
// |                                          |
// | merchant label      value                |
// +~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~+
// | instrument label    [icon] value         |
// +~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~+
// | total label         value                |
// +~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~+
std::unique_ptr<views::View>
SecurePaymentConfirmationDialogView::CreateBodyView() {
  auto body_view = std::make_unique<views::BoxLayoutView>();
  body_view->SetOrientation(views::BoxLayout::Orientation::kVertical);
  body_view->SetInsideBorderInsets(
      ChromeLayoutProvider::Get()->GetInsetsMetric(views::INSETS_DIALOG));
  body_view->SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kCenter);
  body_view->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);

  std::unique_ptr<views::Label> title_text =
      CreateSecurePaymentConfirmationTitleLabel(model_->title());
  title_text->SetID(static_cast<int>(DialogViewID::TITLE));
  body_view->AddChildView(std::move(title_text));

  body_view->AddChildView(
      CreateRowView(model_->merchant_label(), DialogViewID::MERCHANT_LABEL,
                    model_->merchant_value(), DialogViewID::MERCHANT_VALUE));

  body_view->AddChildView(
      CreateRowView(model_->instrument_label(), DialogViewID::INSTRUMENT_LABEL,
                    model_->instrument_value(), DialogViewID::INSTRUMENT_VALUE,
                    model_->instrument_icon(), DialogViewID::INSTRUMENT_ICON));

  body_view->AddChildView(
      CreateRowView(model_->total_label(), DialogViewID::TOTAL_LABEL,
                    model_->total_value(), DialogViewID::TOTAL_VALUE));

  return body_view;
}

// Creates a row of data with |label|, |value|, and optionally |icon|.
// +------------------------------------------+
// | label      [icon] value                  |
// +~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~+ <-- border
std::unique_ptr<views::View> SecurePaymentConfirmationDialogView::CreateRowView(
    const std::u16string& label,
    DialogViewID label_id,
    const std::u16string& value,
    DialogViewID value_id,
    const SkBitmap* icon,
    DialogViewID icon_id) {
  auto row = std::make_unique<BorderedRowView>();

  views::TableLayout* layout =
      row->SetLayoutManager(std::make_unique<views::TableLayout>());

  // Label column
  constexpr int kLabelColumnWidth = 80;
  layout->AddColumn(
      views::LayoutAlignment::kStart, views::LayoutAlignment::kCenter,
      views::TableLayout::kFixedSize, views::TableLayout::ColumnSize::kFixed,
      kLabelColumnWidth, 0);

  constexpr int kPaddingAfterLabel = 24;
  layout->AddPaddingColumn(views::TableLayout::kFixedSize, kPaddingAfterLabel);

  // Icon column
  if (icon) {
    layout->AddColumn(
        views::LayoutAlignment::kStart, views::LayoutAlignment::kCenter,
        views::TableLayout::kFixedSize, views::TableLayout::ColumnSize::kFixed,
        kSecurePaymentConfirmationInstrumentIconWidthPx,
        kSecurePaymentConfirmationInstrumentIconWidthPx);
    layout->AddPaddingColumn(views::TableLayout::kFixedSize,
                             ChromeLayoutProvider::Get()->GetDistanceMetric(
                                 views::DISTANCE_RELATED_LABEL_HORIZONTAL));
  }

  // Value column
  layout->AddColumn(views::LayoutAlignment::kStretch,
                    views::LayoutAlignment::kCenter, 1.0f,
                    views::TableLayout::ColumnSize::kUsePreferred, 0, 0);

  layout->AddRows(1, views::TableLayout::kFixedSize, kPaymentInfoRowHeight);

  std::unique_ptr<views::Label> label_text = std::make_unique<views::Label>(
      label, views::style::CONTEXT_DIALOG_BODY_TEXT,
      ChromeTextStyle::STYLE_EMPHASIZED_SECONDARY);
  label_text->SetHorizontalAlignment(gfx::ALIGN_TO_HEAD);
  label_text->SetLineHeight(kDescriptionLineHeight);
  label_text->SetID(static_cast<int>(label_id));
  row->AddChildView(std::move(label_text));

  if (icon) {
    instrument_icon_ = model_->instrument_icon();
    instrument_icon_generation_id_ =
        model_->instrument_icon()->getGenerationID();

    std::unique_ptr<views::ImageView> icon_view =
        CreateSecurePaymentConfirmationInstrumentIconView(
            *model_->instrument_icon());
    icon_view->SetID(static_cast<int>(icon_id));
    row->AddChildView(std::move(icon_view));
  }

  std::unique_ptr<views::Label> value_text = std::make_unique<views::Label>(
      value, views::style::CONTEXT_DIALOG_BODY_TEXT,
      views::style::STYLE_PRIMARY);
  value_text->SetHorizontalAlignment(gfx::ALIGN_TO_HEAD);
  value_text->SetLineHeight(kDescriptionLineHeight);
  value_text->SetID(static_cast<int>(value_id));
  row->AddChildView(std::move(value_text));

  return row;
}

BEGIN_METADATA(SecurePaymentConfirmationDialogView, views::DialogDelegateView)
END_METADATA

}  // namespace payments
