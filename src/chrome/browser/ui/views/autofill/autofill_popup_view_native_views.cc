// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/autofill_popup_view_native_views.h"

#include <algorithm>
#include <string>
#include <type_traits>
#include <utility>

#include "base/containers/contains.h"
#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller_utils.h"
#include "chrome/browser/ui/views/autofill/autofill_popup_view_utils.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/chrome_typography_provider.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/font.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/shadow_value.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace {

// By spec, dropdowns should always have a width which is a multiple of 12.
constexpr int kAutofillPopupWidthMultiple = 12;
constexpr int kAutofillPopupMinWidth = kAutofillPopupWidthMultiple * 16;
// TODO(crbug.com/831603): move handling the max width to the base class.
constexpr int kAutofillPopupMaxWidth = kAutofillPopupWidthMultiple * 38;

// Max width for the username and masked password.
constexpr int kAutofillPopupUsernameMaxWidth = 272;
constexpr int kAutofillPopupPasswordMaxWidth = 108;

// The additional height of the row in case it has two lines of text.
constexpr int kAutofillPopupAdditionalDoubleRowHeight = 22;

// The additional padding of the row in case it has three lines of text.
constexpr int kAutofillPopupAdditionalPadding = 16;

// Vertical spacing between labels in one row.
constexpr int kAdjacentLabelsVerticalSpacing = 2;

// Popup footer items that use a leading icon instead of a trailing one.
constexpr autofill::PopupItemId kItemTypesUsingLeadingIcons[] = {
    autofill::PopupItemId::POPUP_ITEM_ID_SHOW_ACCOUNT_CARDS,
    autofill::PopupItemId::POPUP_ITEM_ID_ALL_SAVED_PASSWORDS_ENTRY,
    autofill::PopupItemId::POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_EMPTY,
    autofill::PopupItemId::POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_OPT_IN,
    autofill::PopupItemId::POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_RE_SIGNIN,
    autofill::PopupItemId::
        POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_OPT_IN_AND_GENERATE};

int GetContentsVerticalPadding() {
  return ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_CONTENT_LIST_VERTICAL_MULTI);
}

int GetHorizontalMargin() {
  return views::MenuConfig::instance().item_horizontal_padding +
         autofill::AutofillPopupBaseView::GetCornerRadius();
}

// Builds a column set for |layout| used in the autofill dropdown.
void BuildColumnSet(views::GridLayout* layout) {
  views::ColumnSet* column_set = layout->AddColumnSet(0);
  const int column_divider = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_RELATED_LABEL_HORIZONTAL_LIST);

  column_set->AddColumn(views::GridLayout::LEADING, views::GridLayout::FILL,
                        views::GridLayout::kFixedSize,
                        views::GridLayout::ColumnSize::kUsePreferred, 0, 0);
  column_set->AddPaddingColumn(views::GridLayout::kFixedSize, column_divider);
  column_set->AddColumn(views::GridLayout::LEADING, views::GridLayout::FILL,
                        views::GridLayout::kFixedSize,
                        views::GridLayout::ColumnSize::kUsePreferred, 0, 0);
}

std::unique_ptr<views::ImageView> ImageViewFromImageSkia(
    const gfx::ImageSkia& image_skia) {
  if (image_skia.isNull())
    return nullptr;
  auto image_view = std::make_unique<views::ImageView>();
  image_view->SetImage(image_skia);
  return image_view;
}

std::unique_ptr<views::ImageView> ImageViewFromVectorIcon(
    const gfx::VectorIcon& vector_icon) {
  return std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
      vector_icon, ui::NativeTheme::kColorId_DefaultIconColor,
      gfx::kFaviconSize));
}

std::unique_ptr<views::ImageView> GetIconImageViewByName(
    const std::string& icon_str) {
  if (icon_str.empty())
    return nullptr;

  // For http warning message, get icon images from VectorIcon, which is the
  // same as security indicator icons in location bar.
  if (icon_str == "httpWarning")
    return ImageViewFromVectorIcon(omnibox::kHttpIcon);

  if (icon_str == "httpsInvalid") {
    return ImageViewFromImageSkia(
        gfx::CreateVectorIcon(vector_icons::kNotSecureWarningIcon,
                              gfx::kFaviconSize, gfx::kGoogleRed700));
  }

  if (icon_str == "keyIcon")
    return ImageViewFromVectorIcon(kKeyIcon);

  if (icon_str == "globeIcon")
    return ImageViewFromVectorIcon(kGlobeIcon);

  if (icon_str == "settingsIcon")
    return ImageViewFromVectorIcon(vector_icons::kSettingsIcon);

  if (icon_str == "empty")
    return ImageViewFromVectorIcon(omnibox::kHttpIcon);

  if (icon_str == "google") {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    return ImageViewFromImageSkia(gfx::CreateVectorIcon(
        kGoogleGLogoIcon, gfx::kFaviconSize, gfx::kPlaceholderColor));
#else
    return nullptr;
#endif
  }

#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (icon_str == "googlePay" || icon_str == "googlePayDark") {
    return nullptr;
  }
#endif
  // For other suggestion entries, get icon from PNG files.
  int icon_id = autofill::GetIconResourceID(icon_str);
  DCHECK_NE(icon_id, 0);
  return ImageViewFromImageSkia(
      *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(icon_id));
}

std::unique_ptr<views::ImageView> GetIconImageView(
    const autofill::Suggestion& suggestion) {
  if (!suggestion.custom_icon.IsEmpty()) {
    return ImageViewFromImageSkia(suggestion.custom_icon.AsImageSkia());
  }

  return GetIconImageViewByName(suggestion.icon);
}

std::unique_ptr<views::ImageView> GetStoreIndicatorIconImageView(
    const autofill::Suggestion& suggestion) {
  return GetIconImageViewByName(suggestion.store_indicator_icon);
}

// Creates a label with a specific context and style.
std::unique_ptr<views::Label> CreateLabelWithStyleAndContext(
    const std::u16string& text,
    int text_context,
    int text_style) {
  auto label = std::make_unique<views::Label>(text, text_context, text_style);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  return label;
}

}  // namespace

namespace autofill {

namespace {

// Container view that holds one child view and limits its width to the
// specified maximum.
class ConstrainedWidthView : public views::View {
 public:
  METADATA_HEADER(ConstrainedWidthView);
  ConstrainedWidthView(std::unique_ptr<views::View> child, int max_width);
  ConstrainedWidthView(const ConstrainedWidthView&) = delete;
  ConstrainedWidthView& operator=(const ConstrainedWidthView&) = delete;
  ~ConstrainedWidthView() override = default;

 private:
  // views::View:
  gfx::Size CalculatePreferredSize() const override;

  int max_width_;
};

ConstrainedWidthView::ConstrainedWidthView(std::unique_ptr<views::View> child,
                                           int max_width)
    : max_width_(max_width) {
  SetLayoutManager(std::make_unique<views::FillLayout>());
  AddChildView(std::move(child));
}

gfx::Size ConstrainedWidthView::CalculatePreferredSize() const {
  gfx::Size size = View::CalculatePreferredSize();
  if (size.width() <= max_width_)
    return size;
  return gfx::Size(max_width_, GetHeightForWidth(max_width_));
}

BEGIN_METADATA(ConstrainedWidthView, views::View)
END_METADATA

class PopupSeparator : public views::Separator {
 public:
  METADATA_HEADER(PopupSeparator);
  explicit PopupSeparator(AutofillPopupBaseView* popup);

  // views::Separator:
  void OnThemeChanged() override;

 private:
  AutofillPopupBaseView* popup_;
};

PopupSeparator::PopupSeparator(AutofillPopupBaseView* popup) : popup_(popup) {
  // Add some spacing between the the previous item and the separator.
  SetPreferredHeight(views::MenuConfig::instance().separator_thickness);
  SetBorder(views::CreateEmptyBorder(GetContentsVerticalPadding(), 0, 0, 0));
}

void PopupSeparator::OnThemeChanged() {
  views::Separator::OnThemeChanged();
  SetColor(popup_->GetSeparatorColor());
}

BEGIN_METADATA(PopupSeparator, views::Separator)
END_METADATA

class SuggestionLabel : public views::Label {
 public:
  METADATA_HEADER(SuggestionLabel);
  SuggestionLabel(const std::u16string& text, AutofillPopupBaseView* popup);

  // views::Label:
  void OnThemeChanged() override;

 private:
  AutofillPopupBaseView* popup_;
};

SuggestionLabel::SuggestionLabel(const std::u16string& text,
                                 AutofillPopupBaseView* popup)
    : Label(text,
            views::style::CONTEXT_DIALOG_BODY_TEXT,
            ChromeTextStyle::STYLE_RED),
      popup_(popup) {
  SetMultiLine(true);
  SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
}

void SuggestionLabel::OnThemeChanged() {
  views::Label::OnThemeChanged();
  SetEnabledColor(popup_->GetWarningColor());
}

BEGIN_METADATA(SuggestionLabel, views::Label)
END_METADATA

// This represents a single selectable item. Subclasses distinguish between
// footer and suggestion rows, which are structurally similar but have
// distinct styling.
class AutofillPopupItemView : public AutofillPopupRowView {
 public:
  METADATA_HEADER(AutofillPopupItemView);
  ~AutofillPopupItemView() override = default;

  // views::View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

 protected:
  AutofillPopupItemView(AutofillPopupViewNativeViews* popup_view,
                        int line_number,
                        int frontend_id)
      : AutofillPopupRowView(popup_view, line_number),
        frontend_id_(frontend_id) {}

  // AutofillPopupRowView:
  void CreateContent() override;
  void RefreshStyle() override;
  std::unique_ptr<views::Background> CreateBackground() final;

  int GetFrontendId() const;

  virtual int GetPrimaryTextStyle() = 0;
  // Returns a main text label view. The label part is optional but allow caller
  // to keep track of all the labels for background color update.
  virtual std::unique_ptr<views::View> CreateMainTextView();
  // Returns a minor text label view. The label is shown side by side with the
  // main text view, but in a secondary style. Can be nullptr.
  virtual std::unique_ptr<views::View> CreateMinorTextView();
  // The description view can be nullptr.
  virtual std::unique_ptr<views::View> CreateDescriptionView();

  // Returns the font weight to be applied to primary info.
  virtual gfx::Font::Weight GetPrimaryTextWeight() const = 0;

  void AddSpacerWithSize(int spacer_width,
                         bool resize,
                         views::BoxLayout* layout);

  void KeepLabel(views::Label* label) {
    if (label)
      inner_labels_.push_back(label);
  }

 private:
  // Returns a vector of optional labels to be displayed beneath value.
  virtual std::vector<std::unique_ptr<views::View>> CreateSubtextViews();

  // Returns the minimum cross axis size depending on the length of
  // GetSubtexts();
  void UpdateLayoutSize(views::BoxLayout* layout_manager, int64_t num_subtexts);

  const int frontend_id_;

  // All the labels inside this view.
  std::vector<views::Label*> inner_labels_;
};

int AutofillPopupItemView::GetFrontendId() const {
  return frontend_id_;
}

BEGIN_METADATA(AutofillPopupItemView, AutofillPopupRowView)
ADD_READONLY_PROPERTY_METADATA(int, FrontendId)
END_METADATA

// This represents a suggestion, i.e., a row containing data that will be filled
// into the page if selected.
class AutofillPopupSuggestionView : public AutofillPopupItemView {
 public:
  METADATA_HEADER(AutofillPopupSuggestionView);
  AutofillPopupSuggestionView(const AutofillPopupSuggestionView&) = delete;
  AutofillPopupSuggestionView& operator=(const AutofillPopupSuggestionView&) =
      delete;
  ~AutofillPopupSuggestionView() override = default;

  static AutofillPopupSuggestionView* Create(
      AutofillPopupViewNativeViews* popup_view,
      int line_number,
      int frontend_id);

 protected:
  // AutofillPopupItemView:
  int GetPrimaryTextStyle() override;
  gfx::Font::Weight GetPrimaryTextWeight() const override;
  std::vector<std::unique_ptr<views::View>> CreateSubtextViews() override;
  AutofillPopupSuggestionView(AutofillPopupViewNativeViews* popup_view,
                              int line_number,
                              int frontend_id);
};

BEGIN_METADATA(AutofillPopupSuggestionView, AutofillPopupItemView)
END_METADATA

// This represents a password suggestion row, i.e., a username and password.
class PasswordPopupSuggestionView : public AutofillPopupSuggestionView {
 public:
  METADATA_HEADER(PasswordPopupSuggestionView);
  PasswordPopupSuggestionView(const PasswordPopupSuggestionView&) = delete;
  PasswordPopupSuggestionView& operator=(const PasswordPopupSuggestionView&) =
      delete;
  ~PasswordPopupSuggestionView() override = default;

  static PasswordPopupSuggestionView* Create(
      AutofillPopupViewNativeViews* popup_view,
      int line_number,
      int frontend_id);

 protected:
  // AutofillPopupItemView:
  std::unique_ptr<views::View> CreateMainTextView() override;
  std::vector<std::unique_ptr<views::View>> CreateSubtextViews() override;
  std::unique_ptr<views::View> CreateDescriptionView() override;
  gfx::Font::Weight GetPrimaryTextWeight() const override;

 private:
  PasswordPopupSuggestionView(AutofillPopupViewNativeViews* popup_view,
                              int line_number,
                              int frontend_id);
  std::u16string origin_;
  std::u16string masked_password_;
};

BEGIN_METADATA(PasswordPopupSuggestionView, AutofillPopupSuggestionView)
END_METADATA

// This represents an option which appears in the footer of the dropdown, such
// as a row which will open the Autofill settings page when selected.
class AutofillPopupFooterView : public AutofillPopupItemView {
 public:
  METADATA_HEADER(AutofillPopupFooterView);
  ~AutofillPopupFooterView() override = default;

  static AutofillPopupFooterView* Create(
      AutofillPopupViewNativeViews* popup_view,
      int line_number,
      int frontend_id);

 protected:
  // AutofillPopupItemView:
  void CreateContent() override;
  void RefreshStyle() override;
  int GetPrimaryTextStyle() override;
  gfx::Font::Weight GetPrimaryTextWeight() const override;

 private:
  AutofillPopupFooterView(AutofillPopupViewNativeViews* popup_view,
                          int line_number,
                          int frontend_id);
};

BEGIN_METADATA(AutofillPopupFooterView, AutofillPopupItemView)
END_METADATA

// Draws a separator between sections of the dropdown, namely between datalist
// and Autofill suggestions. Note that this is NOT the same as the border on top
// of the footer section or the border between footer items.
class AutofillPopupSeparatorView : public AutofillPopupRowView {
 public:
  METADATA_HEADER(AutofillPopupSeparatorView);
  AutofillPopupSeparatorView(const AutofillPopupSeparatorView&) = delete;
  AutofillPopupSeparatorView& operator=(const AutofillPopupSeparatorView&) =
      delete;
  ~AutofillPopupSeparatorView() override = default;

  static AutofillPopupSeparatorView* Create(
      AutofillPopupViewNativeViews* popup_view,
      int line_number);

  // views::View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void OnMouseEntered(const ui::MouseEvent& event) override {}
  void OnMouseExited(const ui::MouseEvent& event) override {}
  void OnMouseReleased(const ui::MouseEvent& event) override {}

 protected:
  // AutofillPopupRowView:
  void CreateContent() override;
  void RefreshStyle() override;
  std::unique_ptr<views::Background> CreateBackground() override;

 private:
  AutofillPopupSeparatorView(AutofillPopupViewNativeViews* popup_view,
                             int line_number);
};

BEGIN_METADATA(AutofillPopupSeparatorView, AutofillPopupRowView)
END_METADATA

// Draws a row which contains a warning message.
class AutofillPopupWarningView : public AutofillPopupRowView {
 public:
  METADATA_HEADER(AutofillPopupWarningView);
  AutofillPopupWarningView(const AutofillPopupWarningView&) = delete;
  AutofillPopupWarningView& operator=(const AutofillPopupWarningView&) = delete;
  ~AutofillPopupWarningView() override = default;

  static AutofillPopupWarningView* Create(
      AutofillPopupViewNativeViews* popup_view,
      int line_number);

  // views::View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void OnMouseEntered(const ui::MouseEvent& event) override {}
  void OnMouseReleased(const ui::MouseEvent& event) override {}

 protected:
  // AutofillPopupRowView:
  void CreateContent() override;
  void RefreshStyle() override {}
  std::unique_ptr<views::Background> CreateBackground() override;

 private:
  AutofillPopupWarningView(AutofillPopupViewNativeViews* popup_view,
                           int line_number)
      : AutofillPopupRowView(popup_view, line_number) {}
};

BEGIN_METADATA(AutofillPopupWarningView, AutofillPopupRowView)
END_METADATA

/************** AutofillPopupItemView **************/

void AutofillPopupItemView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  AutofillPopupController* controller = popup_view()->controller();
  std::vector<std::u16string> text;

  auto main_text = controller->GetSuggestionMainTextAt(GetLineNumber());
  text.push_back(main_text);

  auto minor_text = controller->GetSuggestionMinorTextAt(GetLineNumber());
  if (!minor_text.empty())
    text.push_back(minor_text);

  auto label_text = controller->GetSuggestionLabelAt(GetLineNumber());
  if (!label_text.empty()) {
    // |label| is not populated for footers or autocomplete entries.
    text.push_back(label_text);
  }

  // TODO(siyua): GetSuggestionLabelAt should return a vector of strings.
  auto suggestion = controller->GetSuggestionAt(GetLineNumber());
  if (!suggestion.offer_label.empty()) {
    // |offer_label| is only populated for credit card suggestions.
    text.push_back(suggestion.offer_label);
  }

  if (!suggestion.additional_label.empty()) {
    // |additional_label| is only populated in a passwords context.
    text.push_back(suggestion.additional_label);
  }

  node_data->SetName(base::JoinString(text, u" "));

  // Options are selectable.
  node_data->role = ax::mojom::Role::kListBoxOption;
  node_data->AddBoolAttribute(ax::mojom::BoolAttribute::kSelected,
                              GetSelected());

  // Compute set size and position in set, by checking the frontend_id of each
  // row, summing the number of interactive rows, and subtracting the number
  // of separators found before this row from its |pos_in_set|.
  int set_size = 0;
  int pos_in_set = GetLineNumber() + 1;
  for (int i = 0; i < controller->GetLineCount(); ++i) {
    if (controller->GetSuggestionAt(i).frontend_id ==
        autofill::POPUP_ITEM_ID_SEPARATOR) {
      if (i < GetLineNumber())
        --pos_in_set;
    } else {
      ++set_size;
    }
  }
  node_data->AddIntAttribute(ax::mojom::IntAttribute::kSetSize, set_size);
  node_data->AddIntAttribute(ax::mojom::IntAttribute::kPosInSet, pos_in_set);
}

void AutofillPopupItemView::OnMouseEntered(const ui::MouseEvent& event) {
  AutofillPopupController* controller = popup_view()->controller();
  if (controller)
    controller->SetSelectedLine(GetLineNumber());
}

void AutofillPopupItemView::OnMouseExited(const ui::MouseEvent& event) {
  AutofillPopupController* controller = popup_view()->controller();
  if (controller)
    controller->SelectionCleared();
}

void AutofillPopupItemView::OnMouseReleased(const ui::MouseEvent& event) {
  AutofillPopupController* controller = popup_view()->controller();
  if (controller && event.IsOnlyLeftMouseButton() &&
      HitTestPoint(event.location())) {
    controller->AcceptSuggestion(GetLineNumber());
  }
}

void AutofillPopupItemView::OnGestureEvent(ui::GestureEvent* event) {
  AutofillPopupController* controller = popup_view()->controller();
  if (!controller)
    return;
  switch (event->type()) {
    case ui::ET_GESTURE_TAP_DOWN:
      controller->SetSelectedLine(GetLineNumber());
      break;
    case ui::ET_GESTURE_TAP:
      controller->AcceptSuggestion(GetLineNumber());
      break;
    case ui::ET_GESTURE_TAP_CANCEL:
    case ui::ET_GESTURE_END:
      controller->SelectionCleared();
      break;
    default:
      return;
  }
}

void AutofillPopupItemView::CreateContent() {
  AutofillPopupController* controller = popup_view()->controller();

  auto* layout_manager = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets(0, GetHorizontalMargin())));

  layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  std::vector<Suggestion> suggestions = controller->GetSuggestions();

  std::unique_ptr<views::ImageView> icon =
      GetIconImageView(suggestions[GetLineNumber()]);

  if (icon) {
    AddChildView(std::move(icon));
    AddSpacerWithSize(GetHorizontalMargin(),
                      /*resize=*/false, layout_manager);
  }

  std::unique_ptr<views::View> main_text_label = CreateMainTextView();
  std::unique_ptr<views::View> minor_text_label = CreateMinorTextView();
  std::vector<std::unique_ptr<views::View>> subtext_labels =
      CreateSubtextViews();
  std::unique_ptr<views::View> description_label = CreateDescriptionView();

  std::unique_ptr<views::View> all_labels = std::make_unique<views::View>();
  views::GridLayout* grid_layout =
      all_labels->SetLayoutManager(std::make_unique<views::GridLayout>());
  BuildColumnSet(grid_layout);
  grid_layout->StartRow(0, 0);

  // Create the first line text view.
  if (minor_text_label) {
    auto first_line_container = std::make_unique<views::View>();
    first_line_container
        ->SetLayoutManager(std::make_unique<views::FlexLayout>())
        ->SetOrientation(views::LayoutOrientation::kHorizontal)
        .SetMainAxisAlignment(views::LayoutAlignment::kStart)
        .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
        .SetIgnoreDefaultMainAxisMargins(true)
        .SetCollapseMargins(true)
        .SetDefault(views::kMarginsKey,
                    gfx::Insets(
                        /*vertical=*/0,
                        /*horizontal=*/
                        ChromeLayoutProvider::Get()->GetDistanceMetric(
                            DISTANCE_RELATED_LABEL_HORIZONTAL_LIST)));

    first_line_container->AddChildView(std::move(main_text_label));
    first_line_container->AddChildView(std::move(minor_text_label));
    grid_layout->AddView(std::move(first_line_container));
  } else {
    grid_layout->AddView(std::move(main_text_label));
  }

  if (description_label) {
    grid_layout->AddView(std::move(description_label));
  } else {
    grid_layout->SkipColumns(1);
  }

  UpdateLayoutSize(layout_manager, subtext_labels.size());
  for (std::unique_ptr<views::View>& subtext_label : subtext_labels) {
    grid_layout->StartRowWithPadding(0, 0, 0, kAdjacentLabelsVerticalSpacing);
    grid_layout->AddView(std::move(subtext_label));
    grid_layout->SkipColumns(1);
  }

  AddChildView(std::move(all_labels));
  std::unique_ptr<views::ImageView> store_indicator_icon =
      GetStoreIndicatorIconImageView(suggestions[GetLineNumber()]);
  if (store_indicator_icon) {
    AddSpacerWithSize(GetHorizontalMargin(),
                      /*resize=*/true, layout_manager);
    AddChildView(std::move(store_indicator_icon));
  }
}

void AutofillPopupItemView::RefreshStyle() {
  SetBackground(CreateBackground());
  SkColor bk_color = GetSelected() ? popup_view()->GetSelectedBackgroundColor()
                                   : popup_view()->GetBackgroundColor();
  SkColor fg_color = GetSelected() ? popup_view()->GetSelectedForegroundColor()
                                   : popup_view()->GetForegroundColor();
  for (views::Label* label : inner_labels_) {
    label->SetAutoColorReadabilityEnabled(false);
    label->SetBackgroundColor(bk_color);
    // Set style depending on current state since the style isn't automatically
    // adjusted after creation of the label.
    label->SetEnabledColor(
        label->GetEnabled()
            ? fg_color
            : views::style::GetColor(*this, label->GetTextContext(),
                                     views::style::STYLE_DISABLED));
  }
  SchedulePaint();
}

std::unique_ptr<views::Background> AutofillPopupItemView::CreateBackground() {
  return views::CreateSolidBackground(
      GetSelected() ? popup_view()->GetSelectedBackgroundColor()
                    : popup_view()->GetBackgroundColor());
}

std::unique_ptr<views::View> AutofillPopupItemView::CreateMainTextView() {
  // TODO(crbug.com/831603): Remove elision responsibilities from controller.
  std::u16string text =
      popup_view()->controller()->GetSuggestionMainTextAt(GetLineNumber());
  if (popup_view()
          ->controller()
          ->GetSuggestionAt(GetLineNumber())
          .is_value_secondary) {
    std::unique_ptr<views::Label> label = CreateLabelWithStyleAndContext(
        text, views::style::CONTEXT_DIALOG_BODY_TEXT,
        views::style::STYLE_SECONDARY);
    KeepLabel(label.get());
    return label;
  }

  std::unique_ptr<views::Label> label = CreateLabelWithStyleAndContext(
      popup_view()->controller()->GetSuggestionMainTextAt(GetLineNumber()),
      views::style::CONTEXT_DIALOG_BODY_TEXT, GetPrimaryTextStyle());

  const gfx::Font::Weight font_weight = GetPrimaryTextWeight();
  if (font_weight != label->font_list().GetFontWeight()) {
    label->SetFontList(label->font_list().DeriveWithWeight(font_weight));
  }

  KeepLabel(label.get());
  return label;
}

std::unique_ptr<views::View> AutofillPopupItemView::CreateMinorTextView() {
  std::u16string text =
      popup_view()->controller()->GetSuggestionMinorTextAt(GetLineNumber());
  if (text.empty())
    return nullptr;

  std::unique_ptr<views::Label> label = CreateLabelWithStyleAndContext(
      text, views::style::CONTEXT_DIALOG_BODY_TEXT,
      views::style::STYLE_SECONDARY);
  KeepLabel(label.get());
  return label;
}

std::unique_ptr<views::View> AutofillPopupItemView::CreateDescriptionView() {
  return nullptr;
}

std::vector<std::unique_ptr<views::View>>
AutofillPopupItemView::CreateSubtextViews() {
  return {};
}

void AutofillPopupItemView::UpdateLayoutSize(views::BoxLayout* layout_manager,
                                             int64_t num_subtexts) {
  const int kStandardRowHeight =
      views::MenuConfig::instance().touchable_menu_height;
  if (num_subtexts == 0) {
    layout_manager->set_minimum_cross_axis_size(kStandardRowHeight);
  } else {
    layout_manager->set_minimum_cross_axis_size(
        kStandardRowHeight + kAutofillPopupAdditionalDoubleRowHeight);
  }

  // In the case that there are three rows in total, adding extra padding to
  // avoid cramming.
  if (num_subtexts == 2) {
    layout_manager->set_inside_border_insets(
        gfx::Insets(kAutofillPopupAdditionalPadding, GetHorizontalMargin(),
                    kAutofillPopupAdditionalPadding, GetHorizontalMargin()));
  }
}

void AutofillPopupItemView::AddSpacerWithSize(int spacer_width,
                                              bool resize,
                                              views::BoxLayout* layout) {
  auto spacer = std::make_unique<views::View>();
  spacer->SetPreferredSize(gfx::Size(spacer_width, 1));
  layout->SetFlexForView(AddChildView(std::move(spacer)),
                         /*flex=*/resize ? 1 : 0,
                         /*use_min_size=*/true);
}

/************** AutofillPopupSuggestionView **************/

// static
AutofillPopupSuggestionView* AutofillPopupSuggestionView::Create(
    AutofillPopupViewNativeViews* popup_view,
    int line_number,
    int frontend_id) {
  AutofillPopupSuggestionView* result =
      new AutofillPopupSuggestionView(popup_view, line_number, frontend_id);
  result->Init();
  return result;
}

int AutofillPopupSuggestionView::GetPrimaryTextStyle() {
  return views::style::TextStyle::STYLE_PRIMARY;
}

gfx::Font::Weight AutofillPopupSuggestionView::GetPrimaryTextWeight() const {
  return views::TypographyProvider::MediumWeightForUI();
}

AutofillPopupSuggestionView::AutofillPopupSuggestionView(
    AutofillPopupViewNativeViews* popup_view,
    int line_number,
    int frontend_id)
    : AutofillPopupItemView(popup_view, line_number, frontend_id) {
  SetFocusBehavior(FocusBehavior::ALWAYS);
}

std::vector<std::unique_ptr<views::View>>
AutofillPopupSuggestionView::CreateSubtextViews() {
  const std::u16string& second_row_label =
      popup_view()->controller()->GetSuggestionLabelAt(GetLineNumber());
  const std::u16string& third_row_label =
      popup_view()->controller()->GetSuggestionAt(GetLineNumber()).offer_label;

  std::vector<std::unique_ptr<views::View>> labels;
  for (const std::u16string& text : {second_row_label, third_row_label}) {
    // If a row is missing, do not include any further rows.
    if (text.empty())
      return labels;

    auto label = CreateLabelWithStyleAndContext(
        text, ChromeTextContext::CONTEXT_DIALOG_BODY_TEXT_SMALL,
        views::style::STYLE_SECONDARY);
    KeepLabel(label.get());
    labels.emplace_back(std::move(label));
  }

  return labels;
}

/************** PasswordPopupSuggestionView **************/

PasswordPopupSuggestionView* PasswordPopupSuggestionView::Create(
    AutofillPopupViewNativeViews* popup_view,
    int line_number,
    int frontend_id) {
  PasswordPopupSuggestionView* result =
      new PasswordPopupSuggestionView(popup_view, line_number, frontend_id);
  result->Init();
  return result;
}

std::unique_ptr<views::View> PasswordPopupSuggestionView::CreateMainTextView() {
  std::unique_ptr<views::View> label =
      AutofillPopupSuggestionView::CreateMainTextView();
  label = std::make_unique<ConstrainedWidthView>(
      std::move(label), kAutofillPopupUsernameMaxWidth);
  return label;
}

std::vector<std::unique_ptr<views::View>>
PasswordPopupSuggestionView::CreateSubtextViews() {
  std::unique_ptr<views::Label> label = CreateLabelWithStyleAndContext(
      masked_password_, views::style::CONTEXT_DIALOG_BODY_TEXT,
      views::style::STYLE_SECONDARY);
  label->SetElideBehavior(gfx::TRUNCATE);
  KeepLabel(label.get());

  std::unique_ptr<views::View> result = std::make_unique<ConstrainedWidthView>(
      std::move(label), kAutofillPopupPasswordMaxWidth);
  std::vector<std::unique_ptr<views::View>> labels;
  labels.emplace_back(std::move(result));
  return labels;
}

std::unique_ptr<views::View>
PasswordPopupSuggestionView::CreateDescriptionView() {
  if (origin_.empty())
    return nullptr;

  std::unique_ptr<views::Label> label = CreateLabelWithStyleAndContext(
      origin_, views::style::CONTEXT_DIALOG_BODY_TEXT,
      views::style::STYLE_SECONDARY);
  label->SetElideBehavior(gfx::ELIDE_HEAD);
  KeepLabel(label.get());

  std::unique_ptr<views::View> result = std::make_unique<ConstrainedWidthView>(
      std::move(label), kAutofillPopupUsernameMaxWidth);
  return result;
}

gfx::Font::Weight PasswordPopupSuggestionView::GetPrimaryTextWeight() const {
  return gfx::Font::Weight::NORMAL;
}

PasswordPopupSuggestionView::PasswordPopupSuggestionView(
    AutofillPopupViewNativeViews* popup_view,
    int line_number,
    int frontend_id)
    : AutofillPopupSuggestionView(popup_view, line_number, frontend_id) {
  origin_ = popup_view->controller()->GetSuggestionLabelAt(line_number);
  masked_password_ =
      popup_view->controller()->GetSuggestionAt(line_number).additional_label;
}

/************** AutofillPopupFooterView **************/

// static
AutofillPopupFooterView* AutofillPopupFooterView::Create(
    AutofillPopupViewNativeViews* popup_view,
    int line_number,
    int frontend_id) {
  AutofillPopupFooterView* result =
      new AutofillPopupFooterView(popup_view, line_number, frontend_id);
  result->Init();
  return result;
}

void AutofillPopupFooterView::CreateContent() {
  AutofillPopupController* controller = popup_view()->controller();

  views::BoxLayout* layout_manager =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal,
          gfx::Insets(0, GetHorizontalMargin())));

  layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  const Suggestion suggestion = controller->GetSuggestions()[GetLineNumber()];
  std::unique_ptr<views::ImageView> icon = GetIconImageView(suggestion);

  const bool use_leading_icon =
      base::Contains(kItemTypesUsingLeadingIcons, GetFrontendId());

  if (suggestion.is_loading) {
    SetEnabled(false);
    AddChildView(std::make_unique<views::Throbber>())->Start();
    AddSpacerWithSize(GetHorizontalMargin(), /*resize=*/false, layout_manager);
  } else if (icon && use_leading_icon) {
    AddChildView(std::move(icon));
    AddSpacerWithSize(GetHorizontalMargin(), /*resize=*/false, layout_manager);
  }

  // GetCornerRadius adds extra height to the footer to account for rounded
  // corners.
  layout_manager->set_minimum_cross_axis_size(
      views::MenuConfig::instance().touchable_menu_height +
      AutofillPopupBaseView::GetCornerRadius());

  auto main_text_label = CreateMainTextView();
  main_text_label->SetEnabled(!suggestion.is_loading);
  AddChildView(std::move(main_text_label));
  AddSpacerWithSize(
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          DISTANCE_BETWEEN_PRIMARY_AND_SECONDARY_LABELS_HORIZONTAL),
      /*resize=*/true, layout_manager);

  if (icon && !use_leading_icon) {
    AddSpacerWithSize(GetHorizontalMargin(), /*resize=*/false, layout_manager);
    AddChildView(std::move(icon));
  }
}

void AutofillPopupFooterView::RefreshStyle() {
  AutofillPopupItemView::RefreshStyle();
  SetBorder(views::CreateSolidSidedBorder(
      /*top=*/views::MenuConfig::instance().separator_thickness,
      /*left=*/0,
      /*bottom=*/0,
      /*right=*/0,
      /*color=*/popup_view()->GetSeparatorColor()));
}

int AutofillPopupFooterView::GetPrimaryTextStyle() {
  return views::style::STYLE_SECONDARY;
}

gfx::Font::Weight AutofillPopupFooterView::GetPrimaryTextWeight() const {
  return gfx::Font::Weight::NORMAL;
}

AutofillPopupFooterView::AutofillPopupFooterView(
    AutofillPopupViewNativeViews* popup_view,
    int line_number,
    int frontend_id)
    : AutofillPopupItemView(popup_view, line_number, frontend_id) {
  SetFocusBehavior(FocusBehavior::ALWAYS);
}

/************** AutofillPopupSeparatorView **************/

// static
AutofillPopupSeparatorView* AutofillPopupSeparatorView::Create(
    AutofillPopupViewNativeViews* popup_view,
    int line_number) {
  AutofillPopupSeparatorView* result =
      new AutofillPopupSeparatorView(popup_view, line_number);
  result->Init();
  return result;
}

void AutofillPopupSeparatorView::GetAccessibleNodeData(
    ui::AXNodeData* node_data) {
  // Separators are not selectable.
  node_data->role = ax::mojom::Role::kSplitter;
}

void AutofillPopupSeparatorView::CreateContent() {
  SetLayoutManager(std::make_unique<views::FillLayout>());
  AddChildView(std::make_unique<PopupSeparator>(popup_view()));
}

void AutofillPopupSeparatorView::RefreshStyle() {
  SchedulePaint();
}

std::unique_ptr<views::Background>
AutofillPopupSeparatorView::CreateBackground() {
  return nullptr;
}

AutofillPopupSeparatorView::AutofillPopupSeparatorView(
    AutofillPopupViewNativeViews* popup_view,
    int line_number)
    : AutofillPopupRowView(popup_view, line_number) {
  SetFocusBehavior(FocusBehavior::NEVER);
}

/************** AutofillPopupWarningView **************/

// static
AutofillPopupWarningView* AutofillPopupWarningView::Create(
    AutofillPopupViewNativeViews* popup_view,
    int line_number) {
  AutofillPopupWarningView* result =
      new AutofillPopupWarningView(popup_view, line_number);
  result->Init();
  return result;
}

void AutofillPopupWarningView::GetAccessibleNodeData(
    ui::AXNodeData* node_data) {
  AutofillPopupController* controller = popup_view()->controller();
  if (!controller)
    return;

  node_data->SetName(controller->GetSuggestionAt(GetLineNumber()).value);
  node_data->role = ax::mojom::Role::kStaticText;
}

void AutofillPopupWarningView::CreateContent() {
  AutofillPopupController* controller = popup_view()->controller();

  int horizontal_margin = GetHorizontalMargin();
  int vertical_margin = AutofillPopupBaseView::GetCornerRadius();

  SetLayoutManager(std::make_unique<views::FillLayout>());
  SetBorder(views::CreateEmptyBorder(
      gfx::Insets(vertical_margin, horizontal_margin)));

  AddChildView(std::make_unique<SuggestionLabel>(
      controller->GetSuggestionMainTextAt(GetLineNumber()), popup_view()));
}

std::unique_ptr<views::Background>
AutofillPopupWarningView::CreateBackground() {
  return nullptr;
}

}  // namespace

/************** AutofillPopupRowView **************/

void AutofillPopupRowView::SetSelected(bool selected) {
  if (selected == selected_)
    return;

  selected_ = selected;
  if (selected)
    popup_view_->NotifyAXSelection(this);
  RefreshStyle();
  OnPropertyChanged(&selected_, views::kPropertyEffectsNone);
}

void AutofillPopupRowView::OnThemeChanged() {
  views::View::OnThemeChanged();
  RefreshStyle();
}

bool AutofillPopupRowView::OnMouseDragged(const ui::MouseEvent& event) {
  return true;
}

bool AutofillPopupRowView::OnMousePressed(const ui::MouseEvent& event) {
  return true;
}

AutofillPopupRowView::AutofillPopupRowView(
    AutofillPopupViewNativeViews* popup_view,
    int line_number)
    : popup_view_(popup_view), line_number_(line_number) {
  SetNotifyEnterExitOnChild(true);
}

void AutofillPopupRowView::Init() {
  CreateContent();
}

int AutofillPopupRowView::GetLineNumber() const {
  return line_number_;
}

bool AutofillPopupRowView::GetSelected() const {
  return selected_;
}

bool AutofillPopupRowView::HandleAccessibleAction(
    const ui::AXActionData& action_data) {
  if (action_data.action == ax::mojom::Action::kFocus)
    popup_view_->controller()->SetSelectedLine(line_number_);
  return View::HandleAccessibleAction(action_data);
}

BEGIN_METADATA(AutofillPopupRowView, views::View)
ADD_PROPERTY_METADATA(bool, Selected)
ADD_READONLY_PROPERTY_METADATA(int, LineNumber)
END_METADATA

/************** AutofillPopupViewNativeViews **************/

AutofillPopupViewNativeViews::AutofillPopupViewNativeViews(
    AutofillPopupController* controller,
    views::Widget* parent_widget)
    : AutofillPopupBaseView(controller, parent_widget),
      controller_(controller) {
  layout_ = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  layout_->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kStart);

  CreateChildViews();
}

AutofillPopupViewNativeViews::~AutofillPopupViewNativeViews() = default;

void AutofillPopupViewNativeViews::GetAccessibleNodeData(
    ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kListBox;
  // If controller_ is valid, then the view is expanded.
  if (controller_) {
    node_data->AddState(ax::mojom::State::kExpanded);
  } else {
    node_data->AddState(ax::mojom::State::kCollapsed);
    node_data->AddState(ax::mojom::State::kInvisible);
  }
  node_data->SetName(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_POPUP_ACCESSIBLE_NODE_DATA));
}

void AutofillPopupViewNativeViews::OnThemeChanged() {
  AutofillPopupBaseView::OnThemeChanged();
  SetBackground(views::CreateSolidBackground(GetBackgroundColor()));
  // |scroll_view_| and |footer_container_| will be null if there is no body
  // or footer content, respectively.
  if (scroll_view_) {
    scroll_view_->SetBackgroundColor(GetBackgroundColor());
  }
  if (footer_container_) {
    footer_container_->SetBackground(
        views::CreateSolidBackground(GetFooterBackgroundColor()));
  }
}

void AutofillPopupViewNativeViews::Show() {
  NotifyAccessibilityEvent(ax::mojom::Event::kExpandedChanged, true);
  DoShow();
}

void AutofillPopupViewNativeViews::Hide() {
  NotifyAccessibilityEvent(ax::mojom::Event::kExpandedChanged, true);
  // The controller is no longer valid after it hides us.
  controller_ = nullptr;
  DoHide();
}

void AutofillPopupViewNativeViews::OnSelectedRowChanged(
    absl::optional<int> previous_row_selection,
    absl::optional<int> current_row_selection) {
  if (previous_row_selection) {
    rows_[*previous_row_selection]->SetSelected(false);
  }

  if (current_row_selection)
    rows_[*current_row_selection]->SetSelected(true);
}

void AutofillPopupViewNativeViews::OnSuggestionsChanged() {
  CreateChildViews();
  DoUpdateBoundsAndRedrawPopup();
}

absl::optional<int32_t> AutofillPopupViewNativeViews::GetAxUniqueId() {
  return absl::optional<int32_t>(
      AutofillPopupBaseView::GetViewAccessibility().GetUniqueId());
}

void AutofillPopupViewNativeViews::CreateChildViews() {
  RemoveAllChildViews(true /* delete_children */);
  rows_.clear();
  scroll_view_ = nullptr;
  body_container_ = nullptr;
  footer_container_ = nullptr;

  int line_number = 0;
  bool has_footer = false;

  // Process and add all the suggestions which are in the primary container.
  // Stop once the first footer item is found, or there are no more items.
  while (line_number < controller_->GetLineCount()) {
    int frontend_id = controller_->GetSuggestionAt(line_number).frontend_id;
    switch (frontend_id) {
      case autofill::PopupItemId::POPUP_ITEM_ID_CLEAR_FORM:
      case autofill::PopupItemId::POPUP_ITEM_ID_AUTOFILL_OPTIONS:
      case autofill::PopupItemId::POPUP_ITEM_ID_SCAN_CREDIT_CARD:
      case autofill::PopupItemId::POPUP_ITEM_ID_CREDIT_CARD_SIGNIN_PROMO:
      case autofill::PopupItemId::POPUP_ITEM_ID_ALL_SAVED_PASSWORDS_ENTRY:
      case autofill::PopupItemId::POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_EMPTY:
      case autofill::PopupItemId::POPUP_ITEM_ID_HIDE_AUTOFILL_SUGGESTIONS:
      case autofill::PopupItemId::POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_OPT_IN:
      case autofill::PopupItemId::
          POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_RE_SIGNIN:
      case autofill::PopupItemId::
          POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_OPT_IN_AND_GENERATE:
      case autofill::PopupItemId::POPUP_ITEM_ID_SHOW_ACCOUNT_CARDS:
      case autofill::PopupItemId::POPUP_ITEM_ID_USE_VIRTUAL_CARD:
        // This is a footer, so this suggestion will be processed later. Don't
        // increment |line_number|, or else it will be skipped when adding
        // footer rows below.
        has_footer = true;
        break;

      case autofill::PopupItemId::POPUP_ITEM_ID_SEPARATOR:
        rows_.push_back(AutofillPopupSeparatorView::Create(this, line_number));
        break;

      case autofill::PopupItemId::POPUP_ITEM_ID_MIXED_FORM_MESSAGE:
      case autofill::PopupItemId::
          POPUP_ITEM_ID_INSECURE_CONTEXT_PAYMENT_DISABLED_MESSAGE:
        rows_.push_back(AutofillPopupWarningView::Create(this, line_number));
        break;

      case autofill::PopupItemId::POPUP_ITEM_ID_USERNAME_ENTRY:
      case autofill::PopupItemId::POPUP_ITEM_ID_PASSWORD_ENTRY:
      case autofill::PopupItemId::POPUP_ITEM_ID_ACCOUNT_STORAGE_USERNAME_ENTRY:
      case autofill::PopupItemId::POPUP_ITEM_ID_ACCOUNT_STORAGE_PASSWORD_ENTRY:
        rows_.push_back(PasswordPopupSuggestionView::Create(this, line_number,
                                                            frontend_id));
        break;

      default:
        rows_.push_back(AutofillPopupSuggestionView::Create(this, line_number,
                                                            frontend_id));
    }

    if (has_footer)
      break;
    line_number++;
  }

  if (!rows_.empty()) {
    // Create a container to wrap the "regular" (non-footer) rows.
    std::unique_ptr<views::View> body_container =
        std::make_unique<views::View>();
    views::BoxLayout* body_layout =
        body_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kVertical));
    body_layout->set_main_axis_alignment(
        views::BoxLayout::MainAxisAlignment::kStart);
    for (auto* row : rows_) {
      body_container->AddChildView(row);
    }

    scroll_view_ = new views::ScrollView();
    scroll_view_->SetHorizontalScrollBarMode(
        views::ScrollView::ScrollBarMode::kDisabled);
    body_container_ = scroll_view_->SetContents(std::move(body_container));
    scroll_view_->SetDrawOverflowIndicator(false);
    scroll_view_->ClipHeightTo(0, body_container_->GetPreferredSize().height());

    // Use an additional container to apply padding outside the scroll view, so
    // that the padding area is stationary. This ensures that the rounded
    // corners appear properly; on Mac, the clipping path will not apply
    // properly to a scrollable area. NOTE: GetContentsVerticalPadding is
    // guaranteed to return a size which accommodates the rounded corners.
    views::View* padding_wrapper = new views::View();
    padding_wrapper->SetBorder(
        views::CreateEmptyBorder(gfx::Insets(GetContentsVerticalPadding(), 0)));
    padding_wrapper->SetLayoutManager(std::make_unique<views::FillLayout>());
    padding_wrapper->AddChildView(scroll_view_);
    AddChildView(padding_wrapper);
    layout_->SetFlexForView(padding_wrapper, 1);
  }

  // All the remaining rows (where index >= |line_number|) are part of the
  // footer. This needs to be in its own container because it should not be
  // affected by scrolling behavior (it's "sticky") and because it has a
  // special background color.
  if (has_footer) {
    auto* footer_container = new views::View();

    views::BoxLayout* footer_layout =
        footer_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kVertical));
    footer_layout->set_main_axis_alignment(
        views::BoxLayout::MainAxisAlignment::kStart);

    while (line_number < controller_->GetLineCount()) {
      rows_.push_back(AutofillPopupFooterView::Create(
          this, line_number,
          controller_->GetSuggestionAt(line_number).frontend_id));
      footer_container->AddChildView(rows_.back());
      line_number++;
    }

    footer_container_ = AddChildView(footer_container);
    layout_->SetFlexForView(footer_container_, 0);
  }
}

int AutofillPopupViewNativeViews::AdjustWidth(int width) const {
  if (width >= kAutofillPopupMaxWidth)
    return kAutofillPopupMaxWidth;

  int elem_width = gfx::ToEnclosingRect(controller_->element_bounds()).width();

  // If the element width is within the range of legal sizes for the popup, use
  // it as the min width, so that the popup will align with its edges when
  // possible.
  int min_width = (kAutofillPopupMinWidth <= elem_width &&
                   elem_width < kAutofillPopupMaxWidth)
                      ? elem_width
                      : kAutofillPopupMinWidth;

  if (width <= min_width)
    return min_width;

  // The popup size is being determined by the contents, rather than the min/max
  // or the element bounds. Round up to a multiple of
  // |kAutofillPopupWidthMultiple|.
  if (width % kAutofillPopupWidthMultiple) {
    width +=
        (kAutofillPopupWidthMultiple - (width % kAutofillPopupWidthMultiple));
  }

  return width;
}

bool AutofillPopupViewNativeViews::DoUpdateBoundsAndRedrawPopup() {
  gfx::Size preferred_size = CalculatePreferredSize();
  gfx::Rect popup_bounds;


  // When a bubble border is shown, the contents area (inside the shadow) is
  // supposed to be aligned with input element boundaries.
  gfx::Rect element_bounds =
      gfx::ToEnclosingRect(controller_->element_bounds());
  // Consider the element is |kElementBorderPadding| pixels larger at the top
  // and at the bottom in order to reposition the dropdown, so that it doesn't
  // look too close to the element.
  element_bounds.Inset(/*horizontal=*/0, /*vertical=*/-kElementBorderPadding);

  int item_height =
      body_container_ && body_container_->children().size() > 0
          ? body_container_->children()[0]->GetPreferredSize().height()
          : 0;

  const gfx::Rect content_area_bounds = GetContentAreaBounds();
  if (!CanShowDropdownHere(item_height, content_area_bounds, element_bounds)) {
    controller_->Hide(PopupHidingReason::kInsufficientSpace);
    return false;
  }

  CalculatePopupYAndHeight(preferred_size.height(), content_area_bounds,
                           element_bounds, &popup_bounds);

  // Adjust the width to compensate for a scroll bar, if necessary, and for
  // other rules.
  int scroll_width = 0;
  if (scroll_view_ && preferred_size.height() > popup_bounds.height()) {
    preferred_size.set_height(popup_bounds.height());

    // Because the preferred size is greater than the bounds available, the
    // contents will have to scroll. The scroll bar will steal width from the
    // content and smoosh everything together. Instead, add to the width to
    // compensate.
    scroll_width = scroll_view_->GetScrollBarLayoutWidth();
  }
  preferred_size.set_width(AdjustWidth(preferred_size.width() + scroll_width));

  CalculatePopupXAndWidth(preferred_size.width(), content_area_bounds,
                          element_bounds, controller_->IsRTL(), &popup_bounds);

  if (BoundsOverlapWithAnyOpenPrompt(popup_bounds,
                                     controller_->GetWebContents())) {
    controller_->Hide(PopupHidingReason::kInsufficientSpace);
    return false;
  }

  SetSize(preferred_size);

  popup_bounds.Inset(-GetWidget()->GetRootView()->border()->GetInsets());
  GetWidget()->SetBounds(popup_bounds);
  UpdateClipPath();

  SchedulePaint();
  return true;
}

BEGIN_METADATA(AutofillPopupViewNativeViews, AutofillPopupBaseView)
END_METADATA

// static
AutofillPopupView* AutofillPopupView::Create(
    base::WeakPtr<AutofillPopupController> controller) {
#if defined(OS_MAC)
  // It's possible for the container_view to not be in a window. In that case,
  // cancel the popup since we can't fully set it up.
  if (!platform_util::GetTopLevel(controller->container_view()))
    return nullptr;
#endif

  views::Widget* observing_widget =
      views::Widget::GetTopLevelWidgetForNativeView(
          controller->container_view());

#if !defined(OS_MAC)
  // If the top level widget can't be found, cancel the popup since we can't
  // fully set it up. On Mac Cocoa browser, |observing_widget| is null
  // because the parent is not a views::Widget.
  if (!observing_widget)
    return nullptr;
#endif

  return new AutofillPopupViewNativeViews(controller.get(), observing_widget);
}

}  // namespace autofill
