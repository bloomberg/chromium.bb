// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/chosen_object_view.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/page_info/chosen_object_view_observer.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view.h"
#include "components/page_info/page_info_delegate.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_utils.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/grid_layout.h"

ChosenObjectView::ChosenObjectView(
    std::unique_ptr<PageInfoUI::ChosenObjectInfo> info,
    std::u16string display_name)
    : info_(std::move(info)) {
  // |ChosenObjectView| layout (fills parent):
  // *------------------------------------*
  // | Icon | Chosen Object Name      | X |
  // |------|-------------------------|---|
  // |      | USB device              |   |
  // *------------------------------------*
  //
  // Where the icon and close button columns are fixed widths.

  views::GridLayout* layout =
      SetLayoutManager(std::make_unique<views::GridLayout>());
  const int column_set_id = 0;

  const int related_label_padding =
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_LABEL_HORIZONTAL);
  views::ColumnSet* column_set = layout->AddColumnSet(column_set_id);
  column_set->AddColumn(views::GridLayout::CENTER, views::GridLayout::CENTER,
                        views::GridLayout::kFixedSize,
                        views::GridLayout::ColumnSize::kFixed,
                        PageInfoBubbleView::kIconColumnWidth, 0);
  column_set->AddPaddingColumn(views::GridLayout::kFixedSize,
                               related_label_padding);
  column_set->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                        1.0, views::GridLayout::ColumnSize::kUsePreferred, 0,
                        0);
  column_set->AddPaddingColumn(views::GridLayout::kFixedSize,
                               related_label_padding);
  column_set->AddColumn(views::GridLayout::TRAILING, views::GridLayout::CENTER,
                        views::GridLayout::kFixedSize,
                        views::GridLayout::ColumnSize::kUsePreferred,
                        PageInfoBubbleView::kIconColumnWidth, 0);

  layout->StartRow(1.0, column_set_id);
  // This padding is added to the top and bottom of each chosen object row, so
  // use half. This is also consistent with |PermissionSelectorRow|'s behavior.
  const int list_item_padding = ChromeLayoutProvider::Get()->GetDistanceMetric(
                                    DISTANCE_CONTROL_LIST_VERTICAL) /
                                2;
  layout->StartRowWithPadding(1.0, column_set_id, views::GridLayout::kFixedSize,
                              list_item_padding);
  // Create the chosen object icon.
  icon_ = layout->AddView(std::make_unique<views::ImageView>());

  // Create the label that displays the chosen object name.
  auto label = std::make_unique<views::Label>(
      display_name, views::style::CONTEXT_DIALOG_BODY_TEXT);
  layout->AddView(std::move(label));

  // Create the delete button. It is safe to use base::Unretained here
  // because the button is owned by this object.
  std::unique_ptr<views::ImageButton> delete_button =
      views::CreateVectorImageButton(base::BindRepeating(
          &ChosenObjectView::ExecuteDeleteCommand, base::Unretained(this)));

  views::SetImageFromVectorIcon(
      delete_button.get(), vector_icons::kCloseRoundedIcon,
      views::style::GetColor(*this, views::style::CONTEXT_DIALOG_BODY_TEXT,
                             views::style::STYLE_PRIMARY));
  delete_button->SetRequestFocusOnPress(true);
  delete_button->SetTooltipText(
      l10n_util::GetStringUTF16(info_->ui_info.delete_tooltip_string_id));
  delete_button_ = layout->AddView(std::move(delete_button));

  // Display secondary text underneath the name of the chosen object to describe
  // what the chosen object actually is.
  layout->StartRow(1.0, column_set_id);
  layout->SkipColumns(1);

  // Disable the delete button for policy controlled objects and display the
  // allowed by policy string below for |secondary_label|.
  std::unique_ptr<views::Label> secondary_label;
  if (info_->chooser_object->source ==
      content_settings::SettingSource::SETTING_SOURCE_POLICY) {
    delete_button_->SetEnabled(false);
    secondary_label = std::make_unique<views::Label>(l10n_util::GetStringUTF16(
        info_->ui_info.allowed_by_policy_description_string_id));
  } else {
    secondary_label = std::make_unique<views::Label>(
        l10n_util::GetStringUTF16(info_->ui_info.description_string_id));
  }

  secondary_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  secondary_label->SetEnabledColor(PageInfoUI::GetSecondaryTextColor());
  secondary_label->SetMultiLine(true);

  // Long labels that cannot fit in the existing space under the permission
  // label should be allowed to use up to |kMaxSecondaryLabelWidth| for display.
  int preferred_width = secondary_label->GetPreferredSize().width();
  constexpr int kMaxSecondaryLabelWidth = 140;
  if (preferred_width > kMaxSecondaryLabelWidth) {
    layout->AddView(std::move(secondary_label), /*col_span=*/1, /*row_span=*/1,
                    views::GridLayout::LEADING, views::GridLayout::CENTER,
                    kMaxSecondaryLabelWidth, /*pref_height=*/0);
  } else {
    layout->AddView(std::move(secondary_label), /*col_span=*/1, /*row_span=*/1,
                    views::GridLayout::FILL, views::GridLayout::CENTER);
  }

  layout->AddPaddingRow(column_set_id, list_item_padding);

  UpdateIconImage(/*is_deleted=*/false);
}

void ChosenObjectView::AddObserver(ChosenObjectViewObserver* observer) {
  observer_list_.AddObserver(observer);
}

ChosenObjectView::~ChosenObjectView() {}

void ChosenObjectView::ExecuteDeleteCommand() {
  // Policy-managed permissions cannot be deleted. This isn't normally
  // reachable but views::test::ButtonTestApi::NotifyClick doesn't check
  // before executing the PressedCallback.
  if (info_->chooser_object->source ==
      content_settings::SettingSource::SETTING_SOURCE_POLICY) {
    return;
  }

  // Change the icon to reflect the selected setting.
  UpdateIconImage(/*is_deleted=*/true);

  DCHECK(delete_button_->GetEnabled());
  DCHECK(delete_button_->GetVisible());
  delete_button_->SetVisible(false);

  for (ChosenObjectViewObserver& observer : observer_list_) {
    observer.OnChosenObjectDeleted(*info_);
  }
}

void ChosenObjectView::UpdateIconImage(bool is_deleted) const {
  icon_->SetImage(PageInfoUI::GetChosenObjectIcon(*info_, is_deleted));
}

BEGIN_METADATA(ChosenObjectView, views::View)
END_METADATA
