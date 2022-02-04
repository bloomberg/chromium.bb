// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/content_analysis_dialog.h"

#include <memory>

#include "base/bind.h"
#include "base/memory/raw_ptr.h"
#include "cc/paint/paint_flags.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_types.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/textarea/textarea.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/layout/table_layout_view.h"

// This should be after all other #includes.
#if defined(_WINDOWS_)  // Detect whether windows.h was included.
#include "base/win/windows_h_disallowed.h"
#endif  // defined(_WINDOWS_)

namespace enterprise_connectors {

namespace {

constexpr base::TimeDelta kResizeAnimationDuration = base::Milliseconds(100);

constexpr int kSideImageSize = 24;
constexpr int kLineHeight = 20;

constexpr gfx::Insets kSideImageInsets = gfx::Insets(8, 8, 8, 8);
constexpr int kMessageAndIconRowLeadingPadding = 32;
constexpr int kMessageAndIconRowTrailingPadding = 48;
constexpr int kSideIconBetweenChildSpacing = 16;
constexpr int kPaddingBeforeBypassJustification = 16;

constexpr size_t kMaxBypassJustificationLength = 200;

// These time values are non-const in order to be overridden in test so they
// complete faster.
base::TimeDelta minimum_pending_dialog_time_ = base::Seconds(2);
base::TimeDelta success_dialog_timeout_ = base::Seconds(1);

// A simple background class to show a colored circle behind the side icon once
// the scanning is done.
class CircleBackground : public views::Background {
 public:
  explicit CircleBackground(SkColor color) { SetNativeControlColor(color); }
  ~CircleBackground() override = default;

  void Paint(gfx::Canvas* canvas, views::View* view) const override {
    int radius = view->bounds().width() / 2;
    gfx::PointF center(radius, radius);
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setColor(get_color());
    canvas->DrawCircle(center, radius, flags);
  }
};

SkColor GetBackgroundColor(const views::View* view) {
  return view->GetColorProvider()->GetColor(ui::kColorDialogBackground);
}

ContentAnalysisDialog::TestObserver* observer_for_testing = nullptr;

}  // namespace

// View classes used to override OnThemeChanged and update the sub-views to the
// new theme.

class DeepScanningBaseView {
 public:
  explicit DeepScanningBaseView(ContentAnalysisDialog* dialog)
      : dialog_(dialog) {}
  ContentAnalysisDialog* dialog() { return dialog_; }

 protected:
  raw_ptr<ContentAnalysisDialog> dialog_;
};

class DeepScanningTopImageView : public DeepScanningBaseView,
                                 public views::ImageView {
 public:
  METADATA_HEADER(DeepScanningTopImageView);

  using DeepScanningBaseView::DeepScanningBaseView;

  void Update() {
    if (!GetWidget())
      return;
    SetImage(dialog()->GetTopImage());
  }

 protected:
  void OnThemeChanged() override {
    views::ImageView::OnThemeChanged();
    Update();
  }
};

BEGIN_METADATA(DeepScanningTopImageView, views::ImageView)
END_METADATA

class DeepScanningSideIconImageView : public DeepScanningBaseView,
                                      public views::ImageView {
 public:
  METADATA_HEADER(DeepScanningSideIconImageView);

  using DeepScanningBaseView::DeepScanningBaseView;

  void Update() {
    if (!GetWidget())
      return;
    SetImage(gfx::CreateVectorIcon(vector_icons::kBusinessIcon, kSideImageSize,
                                   dialog()->GetSideImageLogoColor()));
    if (dialog()->is_result()) {
      SetBackground(std::make_unique<CircleBackground>(
          dialog()->GetSideImageBackgroundColor()));
    }
  }

 protected:
  void OnThemeChanged() override {
    views::ImageView::OnThemeChanged();
    Update();
  }
};

BEGIN_METADATA(DeepScanningSideIconImageView, views::ImageView)
END_METADATA

class DeepScanningSideIconSpinnerView : public DeepScanningBaseView,
                                        public views::Throbber {
 public:
  METADATA_HEADER(DeepScanningSideIconSpinnerView);

  using DeepScanningBaseView::DeepScanningBaseView;

  void Update() {
    if (dialog()->is_result()) {
      parent()->RemoveChildView(this);
      delete this;
    }
  }

 protected:
  void OnThemeChanged() override {
    views::Throbber::OnThemeChanged();
    Update();
  }
};

BEGIN_METADATA(DeepScanningSideIconSpinnerView, views::Throbber)
END_METADATA

// static
base::TimeDelta ContentAnalysisDialog::GetMinimumPendingDialogTime() {
  return minimum_pending_dialog_time_;
}

// static
base::TimeDelta ContentAnalysisDialog::GetSuccessDialogTimeout() {
  return success_dialog_timeout_;
}

ContentAnalysisDialog::ContentAnalysisDialog(
    std::unique_ptr<ContentAnalysisDelegateBase> delegate,
    content::WebContents* web_contents,
    safe_browsing::DeepScanAccessPoint access_point,
    int files_count,
    ContentAnalysisDelegateBase::FinalResult final_result)
    : content::WebContentsObserver(web_contents),
      delegate_(std::move(delegate)),
      web_contents_(web_contents),
      final_result_(final_result),
      access_point_(std::move(access_point)),
      files_count_(files_count) {
  DCHECK(delegate_);
  SetOwnedByWidget(true);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));

  if (observer_for_testing)
    observer_for_testing->ConstructorCalled(this, base::TimeTicks::Now());

  if (final_result_ != ContentAnalysisDelegateBase::FinalResult::SUCCESS)
    UpdateStateFromFinalResult(final_result_);

  SetupButtons();

  first_shown_timestamp_ = base::TimeTicks::Now();

  constrained_window::ShowWebModalDialogViews(this, web_contents_);

  if (observer_for_testing)
    observer_for_testing->ViewsFirstShown(this, first_shown_timestamp_);
}

std::u16string ContentAnalysisDialog::GetWindowTitle() const {
  return std::u16string();
}

void ContentAnalysisDialog::AcceptButtonCallback() {
  DCHECK(delegate_);
  DCHECK(is_warning());
  absl::optional<std::u16string> justification = absl::nullopt;
  if (delegate_->BypassRequiresJustification())
    justification = bypass_justification_->GetText();
  delegate_->BypassWarnings(justification);
}

void ContentAnalysisDialog::CancelButtonCallback() {
  if (delegate_)
    delegate_->Cancel(is_warning());
}

void ContentAnalysisDialog::LearnMoreLinkClickedCallback(
    const ui::Event& event) {
  DCHECK(has_learn_more_url());
  web_contents_->OpenURL(content::OpenURLParams(
      (*delegate_->GetCustomLearnMoreUrl()), content::Referrer(),
      WindowOpenDisposition::NEW_FOREGROUND_TAB, ui::PAGE_TRANSITION_LINK,
      false));
}

void ContentAnalysisDialog::SuccessCallback() {
#if defined(USE_AURA)
  if (web_contents_) {
    // It's possible focus has been lost and gained back incorrectly if the user
    // clicked on the page between the time the scan started and the time the
    // dialog closes. This results in the behaviour detailed in
    // crbug.com/1139050. The fix is to preemptively take back focus when this
    // dialog closes on its own.
    web_contents_->SetIgnoreInputEvents(false);
    web_contents_->Focus();
  }
#endif
}

void ContentAnalysisDialog::ContentsChanged(
    views::Textfield* sender,
    const std::u16string& new_contents) {
  if (new_contents.size() == 0 ||
      new_contents.size() > kMaxBypassJustificationLength)
    DialogDelegate::SetButtonEnabled(ui::DIALOG_BUTTON_OK, false);
  else
    DialogDelegate::SetButtonEnabled(ui::DIALOG_BUTTON_OK, true);
}

bool ContentAnalysisDialog::ShouldShowCloseButton() const {
  return false;
}

views::View* ContentAnalysisDialog::GetContentsView() {
  if (!contents_view_) {
    contents_view_ = new views::BoxLayoutView();  // Owned by caller.
    contents_view_->SetOrientation(views::BoxLayout::Orientation::kVertical);
    // Padding to distance the top image from the icon and message.
    contents_view_->SetBetweenChildSpacing(16);
    // padding to distance the message from the button(s).
    contents_view_->SetInsideBorderInsets(gfx::Insets(0, 0, 10, 0));

    // Add the top image.
    image_ = contents_view_->AddChildView(
        std::make_unique<DeepScanningTopImageView>(this));

    // Create message area layout.
    auto* message_container = contents_view_->AddChildView(
        std::make_unique<views::TableLayoutView>());
    message_container
        ->AddPaddingColumn(views::TableLayout::kFixedSize,
                           kMessageAndIconRowLeadingPadding)
        .AddColumn(views::LayoutAlignment::kStart,
                   views::LayoutAlignment::kStart,
                   views::TableLayout::kFixedSize,
                   views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
        .AddPaddingColumn(views::TableLayout::kFixedSize,
                          kSideIconBetweenChildSpacing)
        .AddColumn(views::LayoutAlignment::kStretch,
                   views::LayoutAlignment::kStretch, 1.0f,
                   views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
        .AddPaddingColumn(views::TableLayout::kFixedSize,
                          kMessageAndIconRowTrailingPadding)
        .AddRows(4, views::TableLayout::kFixedSize);

    // Add the side icon.
    message_container->AddChildView(CreateSideIcon());

    // Add the message.
    message_ =
        message_container->AddChildView(std::make_unique<views::Label>());
    message_->SetText(GetDialogMessage());
    message_->SetLineHeight(kLineHeight);
    message_->SetMultiLine(true);
    message_->SetVerticalAlignment(gfx::ALIGN_MIDDLE);
    message_->SetHorizontalAlignment(gfx::ALIGN_LEFT);

    // Add the Learn More link but hide it so it can only be displayed when
    // required.
    message_container->AddChildView(
        std::make_unique<views::View>());  // Skip a column
    learn_more_link_ = message_container->AddChildView(
        std::make_unique<views::Link>(l10n_util::GetStringUTF16(
            IDS_DEEP_SCANNING_DIALOG_CUSTOM_MESSAGE_LEARN_MORE_LINK)));
    learn_more_link_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    learn_more_link_->SetCallback(base::BindRepeating(
        &ContentAnalysisDialog::LearnMoreLinkClickedCallback,
        base::Unretained(this)));
    learn_more_link_->SetVisible(false);

    message_container->AddChildView(
        std::make_unique<views::View>());  // Skip a column
    justification_text_label_ =
        message_container->AddChildView(std::make_unique<views::Label>());
    justification_text_label_->SetText(
        delegate_->GetBypassJustificationLabel());
    justification_text_label_->SetBorder(
        views::CreateEmptyBorder(kPaddingBeforeBypassJustification, 0, 0, 0));
    justification_text_label_->SetLineHeight(kLineHeight);
    justification_text_label_->SetMultiLine(true);
    justification_text_label_->SetVerticalAlignment(gfx::ALIGN_MIDDLE);
    justification_text_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    justification_text_label_->SetVisible(false);

    message_container->AddChildView(
        std::make_unique<views::View>());  // Skip a column
    bypass_justification_ =
        message_container->AddChildView(std::make_unique<views::Textarea>());
    bypass_justification_->SetAssociatedLabel(justification_text_label_);
    bypass_justification_->SetController(this);
    bypass_justification_->SetVisible(false);

    // If the dialog was started in a state other than pending, setup the views
    // accordingly.
    if (!is_pending())
      UpdateViews();
  }

  return contents_view_;
}

views::Widget* ContentAnalysisDialog::GetWidget() {
  return contents_view_->GetWidget();
}

const views::Widget* ContentAnalysisDialog::GetWidget() const {
  return contents_view_->GetWidget();
}

ui::ModalType ContentAnalysisDialog::GetModalType() const {
  return ui::MODAL_TYPE_CHILD;
}

void ContentAnalysisDialog::WebContentsDestroyed() {
  // If |web_contents_| is destroyed, then the scan results don't matter so the
  // delegate can be destroyed as well.
  delegate_.reset(nullptr);
  CancelDialog();
}

void ContentAnalysisDialog::ShowResult(
    ContentAnalysisDelegateBase::FinalResult result) {
  DCHECK(is_pending());

  UpdateStateFromFinalResult(result);

  // Update the pending dialog only after it has been shown for a minimum amount
  // of time.
  base::TimeDelta time_shown = base::TimeTicks::Now() - first_shown_timestamp_;
  if (time_shown >= GetMinimumPendingDialogTime()) {
    UpdateDialog();
  } else {
    content::GetUIThreadTaskRunner({})->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ContentAnalysisDialog::UpdateDialog,
                       weak_ptr_factory_.GetWeakPtr()),
        GetMinimumPendingDialogTime() - time_shown);
  }
}

ContentAnalysisDialog::~ContentAnalysisDialog() {
  if (observer_for_testing)
    observer_for_testing->DestructorCalled(this);
}

void ContentAnalysisDialog::UpdateStateFromFinalResult(
    ContentAnalysisDelegateBase::FinalResult final_result) {
  final_result_ = final_result;
  switch (final_result_) {
    case ContentAnalysisDelegateBase::FinalResult::ENCRYPTED_FILES:
    case ContentAnalysisDelegateBase::FinalResult::LARGE_FILES:
    case ContentAnalysisDelegateBase::FinalResult::FAILURE:
      dialog_state_ = State::FAILURE;
      break;
    case ContentAnalysisDelegateBase::FinalResult::SUCCESS:
      dialog_state_ = State::SUCCESS;
      break;
    case ContentAnalysisDelegateBase::FinalResult::WARNING:
      dialog_state_ = State::WARNING;
      break;
  }
}

void ContentAnalysisDialog::UpdateViews() {
  DCHECK(contents_view_);

  // Update the style of the dialog to reflect the new state.
  image_->Update();
  side_icon_image_->Update();
  // There isn't always a spinner, for instance when the dialog is started in a
  // state other than the "pending" state.
  if (side_icon_spinner_) {
    side_icon_spinner_->Update();
    side_icon_spinner_ = nullptr;
  }

  // Update the buttons.
  SetupButtons();

  // Update the message's text, and send an alert for screen readers since the
  // text changed.
  std::u16string new_message = GetDialogMessage();
  message_->SetText(new_message);
  message_->GetViewAccessibility().AnnounceText(std::move(new_message));

  // Update the visibility of the Learn More link, which should only be visible
  // if the dialog is in the warning or failure state, and there's a link to
  // display.
  learn_more_link_->SetVisible((is_failure() || is_warning()) &&
                               has_learn_more_url());
  justification_text_label_->SetVisible(is_warning() &&
                                        bypass_requires_justification());
  bypass_justification_->SetVisible(is_warning() &&
                                    bypass_requires_justification());
}

void ContentAnalysisDialog::UpdateDialog() {
  DCHECK(contents_view_);
  DCHECK(is_result());

  auto height_before = contents_view_->GetPreferredSize().height();

  UpdateViews();

  // Resize the dialog's height. This is needed since the text might take more
  // lines after changing.
  auto height_after = contents_view_->GetPreferredSize().height();
  int height_to_add = std::max(height_after - height_before, 0);
  if (height_to_add > 0)
    Resize(height_to_add);

  // Update the dialog.
  DialogDelegate::DialogModelChanged();
  contents_view_->InvalidateLayout();

  // Schedule the dialog to close itself in the success case.
  if (is_success()) {
    content::GetUIThreadTaskRunner({})->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&DialogDelegate::CancelDialog,
                       weak_ptr_factory_.GetWeakPtr()),
        GetSuccessDialogTimeout());
  }

  if (observer_for_testing)
    observer_for_testing->DialogUpdated(this, final_result_);

  // Cancel the dialog as it is updated in tests in the failure dialog case.
  // This is necessary to terminate tests that end when the dialog is closed.
  if (observer_for_testing && is_failure())
    CancelDialog();
}

void ContentAnalysisDialog::Resize(int height_to_add) {
  // Only resize if the dialog is updated to show a result.
  DCHECK(is_result());
  views::Widget* widget = GetWidget();
  DCHECK(widget);

  gfx::Rect dialog_rect = widget->GetContentsView()->GetContentsBounds();
  int new_height = dialog_rect.height();

  // Remove the button row's height if it's removed in the success case.
  if (is_success()) {
    DCHECK(contents_view_->parent());
    DCHECK_EQ(contents_view_->parent()->children().size(), 2ul);
    DCHECK_EQ(contents_view_->parent()->children()[0], contents_view_);

    views::View* button_row_view = contents_view_->parent()->children()[1];
    new_height -= button_row_view->GetContentsBounds().height();
  }

  // Apply the message lines delta.
  new_height += height_to_add;
  dialog_rect.set_height(new_height);

  // Setup the animation.
  bounds_animator_ =
      std::make_unique<views::BoundsAnimator>(widget->GetRootView());
  bounds_animator_->SetAnimationDuration(kResizeAnimationDuration);

  DCHECK(widget->GetRootView());
  views::View* view_to_resize = widget->GetRootView()->children()[0];

  // Start the animation.
  bounds_animator_->AnimateViewTo(view_to_resize, dialog_rect);

  // Change the widget's size.
  gfx::Size new_size = view_to_resize->size();
  new_size.set_height(new_height);
  widget->SetSize(new_size);
}

void ContentAnalysisDialog::SetupButtons() {
  // TODO(domfc): Add "Learn more" button on scan failure.
  if (is_warning()) {
    // Include the Ok and Cancel buttons if there is a bypassable warning.
    DialogDelegate::SetButtons(ui::DIALOG_BUTTON_CANCEL | ui::DIALOG_BUTTON_OK);
    DialogDelegate::SetDefaultButton(ui::DIALOG_BUTTON_CANCEL);

    DialogDelegate::SetButtonLabel(ui::DIALOG_BUTTON_CANCEL,
                                   GetCancelButtonText());
    DialogDelegate::SetCancelCallback(
        base::BindOnce(&ContentAnalysisDialog::CancelButtonCallback,
                       weak_ptr_factory_.GetWeakPtr()));

    DialogDelegate::SetButtonLabel(ui::DIALOG_BUTTON_OK,
                                   GetBypassWarningButtonText());
    DialogDelegate::SetAcceptCallback(
        base::BindOnce(&ContentAnalysisDialog::AcceptButtonCallback,
                       weak_ptr_factory_.GetWeakPtr()));

    if (delegate_->BypassRequiresJustification())
      DialogDelegate::SetButtonEnabled(ui::DIALOG_BUTTON_OK, false);
  } else if (is_failure() || is_pending()) {
    // Include the Cancel button when the scan is pending or failing.
    DialogDelegate::SetButtons(ui::DIALOG_BUTTON_CANCEL);
    DialogDelegate::SetDefaultButton(ui::DIALOG_BUTTON_NONE);

    DialogDelegate::SetButtonLabel(ui::DIALOG_BUTTON_CANCEL,
                                   GetCancelButtonText());
    DialogDelegate::SetCancelCallback(
        base::BindOnce(&ContentAnalysisDialog::CancelButtonCallback,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    // Include no buttons otherwise.
    DialogDelegate::SetButtons(ui::DIALOG_BUTTON_NONE);
    DialogDelegate::SetCancelCallback(
        base::BindOnce(&ContentAnalysisDialog::SuccessCallback,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

std::u16string ContentAnalysisDialog::GetDialogMessage() const {
  switch (dialog_state_) {
    case State::PENDING:
      return GetPendingMessage();
    case State::FAILURE:
      return GetFailureMessage();
    case State::SUCCESS:
      return GetSuccessMessage();
    case State::WARNING:
      return GetWarningMessage();
  }
}

std::u16string ContentAnalysisDialog::GetCancelButtonText() const {
  int text_id;
  auto overriden_text = delegate_->OverrideCancelButtonText();
  if (overriden_text) {
    return overriden_text.value();
  }

  switch (dialog_state_) {
    case State::SUCCESS:
      NOTREACHED();
      [[fallthrough]];
    case State::PENDING:
      text_id = IDS_DEEP_SCANNING_DIALOG_CANCEL_UPLOAD_BUTTON;
      break;
    case State::FAILURE:
      text_id = IDS_CLOSE;
      break;
    case State::WARNING:
      text_id = IDS_DEEP_SCANNING_DIALOG_CANCEL_WARNING_BUTTON;
      break;
  }
  return l10n_util::GetStringUTF16(text_id);
}

std::u16string ContentAnalysisDialog::GetBypassWarningButtonText() const {
  DCHECK(is_warning());
  return l10n_util::GetStringUTF16(IDS_DEEP_SCANNING_DIALOG_PROCEED_BUTTON);
}

std::unique_ptr<views::View> ContentAnalysisDialog::CreateSideIcon() {
  // The side icon is created either:
  // - When the pending dialog is shown
  // - When the response was fast enough that the failure dialog is shown first
  DCHECK(!is_success());

  // The icon left of the text has the appearance of a blue "Enterprise" logo
  // with a spinner when the scan is pending.
  auto icon = std::make_unique<views::View>();
  icon->SetLayoutManager(std::make_unique<views::FillLayout>());

  auto side_image = std::make_unique<DeepScanningSideIconImageView>(this);
  side_image->SetImage(gfx::CreateVectorIcon(
      gfx::IconDescription(vector_icons::kBusinessIcon, kSideImageSize)));
  side_image->SetBorder(views::CreateEmptyBorder(kSideImageInsets));
  side_icon_image_ = icon->AddChildView(std::move(side_image));

  // Add a spinner if the scan result is pending.
  if (is_pending()) {
    auto spinner = std::make_unique<DeepScanningSideIconSpinnerView>(this);
    spinner->Start();
    side_icon_spinner_ = icon->AddChildView(std::move(spinner));
  }

  return icon;
}

SkColor ContentAnalysisDialog::GetSideImageBackgroundColor() const {
  DCHECK(is_result());
  DCHECK(contents_view_);

  switch (dialog_state_) {
    case State::PENDING:
      NOTREACHED();
      return gfx::kGoogleBlue500;
    case State::SUCCESS:
      return gfx::kGoogleBlue500;
    case State::FAILURE:
      return gfx::kGoogleRed500;
    case State::WARNING:
      return gfx::kGoogleYellow500;
  }
}

int ContentAnalysisDialog::GetTopImageId(bool use_dark) const {
  if (use_dark) {
    switch (dialog_state_) {
      case State::PENDING:
        return IDR_UPLOAD_SCANNING_DARK;
      case State::SUCCESS:
        return IDR_UPLOAD_SUCCESS_DARK;
      case State::FAILURE:
        return IDR_UPLOAD_VIOLATION_DARK;
      case State::WARNING:
        return IDR_UPLOAD_WARNING_DARK;
    }
  } else {
    switch (dialog_state_) {
      case State::PENDING:
        return IDR_UPLOAD_SCANNING;
      case State::SUCCESS:
        return IDR_UPLOAD_SUCCESS;
      case State::FAILURE:
        return IDR_UPLOAD_VIOLATION;
      case State::WARNING:
        return IDR_UPLOAD_WARNING;
    }
  }
}

std::u16string ContentAnalysisDialog::GetPendingMessage() const {
  DCHECK(is_pending());
  return l10n_util::GetPluralStringFUTF16(
      IDS_DEEP_SCANNING_DIALOG_UPLOAD_PENDING_MESSAGE, files_count_);
}

std::u16string ContentAnalysisDialog::GetFailureMessage() const {
  DCHECK(is_failure());

  // If the admin has specified a custom message for this failure, it takes
  // precedence over the generic ones.
  if (has_custom_message())
    return GetCustomMessage();

  if (final_result_ == ContentAnalysisDelegateBase::FinalResult::LARGE_FILES) {
    return l10n_util::GetPluralStringFUTF16(
        IDS_DEEP_SCANNING_DIALOG_LARGE_FILE_FAILURE_MESSAGE, files_count_);
  }

  if (final_result_ ==
      ContentAnalysisDelegateBase::FinalResult::ENCRYPTED_FILES) {
    return l10n_util::GetPluralStringFUTF16(
        IDS_DEEP_SCANNING_DIALOG_ENCRYPTED_FILE_FAILURE_MESSAGE, files_count_);
  }

  return l10n_util::GetPluralStringFUTF16(
      IDS_DEEP_SCANNING_DIALOG_UPLOAD_FAILURE_MESSAGE, files_count_);
}

std::u16string ContentAnalysisDialog::GetWarningMessage() const {
  DCHECK(is_warning());

  // If the admin has specified a custom message for this warning, it takes
  // precedence over the generic one.
  if (has_custom_message())
    return GetCustomMessage();

  return l10n_util::GetPluralStringFUTF16(
      IDS_DEEP_SCANNING_DIALOG_UPLOAD_WARNING_MESSAGE, files_count_);
}

std::u16string ContentAnalysisDialog::GetSuccessMessage() const {
  DCHECK(is_success());
  return l10n_util::GetPluralStringFUTF16(
      IDS_DEEP_SCANNING_DIALOG_SUCCESS_MESSAGE, files_count_);
}

std::u16string ContentAnalysisDialog::GetCustomMessage() const {
  DCHECK(is_warning() || is_failure());
  DCHECK(has_custom_message());
  return *(delegate_->GetCustomMessage());
}

const gfx::ImageSkia* ContentAnalysisDialog::GetTopImage() const {
  const bool use_dark = color_utils::IsDark(GetBackgroundColor(contents_view_));
  return ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
      GetTopImageId(use_dark));
}

SkColor ContentAnalysisDialog::GetSideImageLogoColor() const {
  DCHECK(contents_view_);

  switch (dialog_state_) {
    case State::PENDING:
      // Match the spinner in the pending state.
      return gfx::kGoogleBlue500;
    case State::SUCCESS:
    case State::FAILURE:
    case State::WARNING:
      // In a result state the background will have the result's color, so the
      // logo should have the same color as the background.
      return GetBackgroundColor(contents_view_);
  }
}

// static
void ContentAnalysisDialog::SetMinimumPendingDialogTimeForTesting(
    base::TimeDelta delta) {
  minimum_pending_dialog_time_ = delta;
}

// static
void ContentAnalysisDialog::SetSuccessDialogTimeoutForTesting(
    base::TimeDelta delta) {
  success_dialog_timeout_ = delta;
}

// static
void ContentAnalysisDialog::SetObserverForTesting(TestObserver* observer) {
  observer_for_testing = observer;
}

views::ImageView* ContentAnalysisDialog::GetTopImageForTesting() const {
  return image_;
}

views::Throbber* ContentAnalysisDialog::GetSideIconSpinnerForTesting() const {
  return side_icon_spinner_;
}

views::Label* ContentAnalysisDialog::GetMessageForTesting() const {
  return message_;
}

views::Label* ContentAnalysisDialog::GetBypassJustificationLabelForTesting()
    const {
  return justification_text_label_;
}

views::Textarea*
ContentAnalysisDialog::GetBypassJustificationTextareaForTesting() const {
  return bypass_justification_;
}

}  // namespace enterprise_connectors
