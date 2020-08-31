// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_group_editor_bubble_view.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/no_destructor.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/bubble_menu_item_factory.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/tabs/color_picker_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "chrome/grit/generated_resources.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view_class_properties.h"

// static
views::Widget* TabGroupEditorBubbleView::Show(
    const Browser* browser,
    const tab_groups::TabGroupId& group,
    TabGroupHeader* header_view,
    base::Optional<gfx::Rect> anchor_rect,
    views::View* anchor_view,
    bool stop_context_menu_propagation) {
  // If |header_view| is not null, use |header_view| as the |anchor_view|.
  views::Widget* const widget =
      BubbleDialogDelegateView::CreateBubble(new TabGroupEditorBubbleView(
          browser, group, header_view ? header_view : anchor_view, anchor_rect,
          header_view, stop_context_menu_propagation));
  widget->Show();
  return widget;
}

ui::ModalType TabGroupEditorBubbleView::GetModalType() const {
  return ui::MODAL_TYPE_NONE;
}

views::View* TabGroupEditorBubbleView::GetInitiallyFocusedView() {
  return title_field_;
}

gfx::Rect TabGroupEditorBubbleView::GetAnchorRect() const {
  // We want to avoid calling BubbleDialogDelegateView::GetAnchorRect() if
  // |anchor_rect_| has been set. This is because the default behavior uses the
  // anchor view's bounds and also updates |anchor_rect_| to the views bounds.
  // It does this so that the bubble does not jump when the anchoring view is
  // deleted.
  if (use_set_anchor_rect_)
    return anchor_rect().value();
  return BubbleDialogDelegateView::GetAnchorRect();
}

TabGroupEditorBubbleView::TabGroupEditorBubbleView(
    const Browser* browser,
    const tab_groups::TabGroupId& group,
    views::View* anchor_view,
    base::Optional<gfx::Rect> anchor_rect,
    TabGroupHeader* header_view,
    bool stop_context_menu_propagation)
    : browser_(browser),
      group_(group),
      title_field_controller_(this),
      button_listener_(browser, group, header_view),
      use_set_anchor_rect_(anchor_rect) {
  // |anchor_view| should always be defined as it will be used to source the
  // |anchor_widget_|.
  DCHECK(anchor_view);
  SetAnchorView(anchor_view);
  if (anchor_rect)
    SetAnchorRect(anchor_rect.value());

  set_margins(gfx::Insets());

  SetButtons(ui::DIALOG_BUTTON_NONE);

  const base::string16 title = browser_->tab_strip_model()
                                   ->group_model()
                                   ->GetTabGroup(group_)
                                   ->visual_data()
                                   ->title();
  title_at_opening_ = title;
  SetCloseCallback(base::BindOnce(&TabGroupEditorBubbleView::OnBubbleClose,
                                  base::Unretained(this)));

  const auto* layout_provider = ChromeLayoutProvider::Get();
  const int horizontal_spacing = layout_provider->GetDistanceMetric(
      views::DISTANCE_RELATED_CONTROL_HORIZONTAL);
  const int vertical_spacing = layout_provider->GetDistanceMetric(
      views::DISTANCE_RELATED_CONTROL_VERTICAL);

  // The padding of the editing controls is adaptive, to improve the hit target
  // size and screen real estate usage on touch devices.
  const int group_modifier_vertical_spacing =
      ui::TouchUiController::Get()->touch_ui() ? vertical_spacing / 2
                                               : vertical_spacing;
  const gfx::Insets control_insets =
      ui::TouchUiController::Get()->touch_ui()
          ? gfx::Insets(5 * vertical_spacing / 4, horizontal_spacing)
          : gfx::Insets(vertical_spacing, horizontal_spacing);

  views::View* group_modifier_container =
      AddChildView(std::make_unique<views::View>());
  group_modifier_container->SetBorder(views::CreateEmptyBorder(
      gfx::Insets(group_modifier_vertical_spacing, 0)));

  views::FlexLayout* group_modifier_container_layout =
      group_modifier_container->SetLayoutManager(
          std::make_unique<views::FlexLayout>());
  group_modifier_container_layout
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetIgnoreDefaultMainAxisMargins(true);

  // Add the text field for editing the title.
  views::View* title_field_container =
      group_modifier_container->AddChildView(std::make_unique<views::View>());
  title_field_container->SetBorder(views::CreateEmptyBorder(
      control_insets.top(), control_insets.left(),
      group_modifier_vertical_spacing, control_insets.right()));

  title_field_ = title_field_container->AddChildView(
      std::make_unique<TitleField>(stop_context_menu_propagation));
  title_field_->SetText(title);
  title_field_->SetAccessibleName(base::ASCIIToUTF16("Group title"));
  title_field_->SetPlaceholderText(
      l10n_util::GetStringUTF16(IDS_TAB_GROUP_HEADER_BUBBLE_TITLE_PLACEHOLDER));
  title_field_->set_controller(&title_field_controller_);

  views::FlexLayout* title_field_container_layout =
      title_field_container->SetLayoutManager(
          std::make_unique<views::FlexLayout>());
  title_field_container_layout
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetIgnoreDefaultMainAxisMargins(true);

  const tab_groups::TabGroupColorId initial_color_id = InitColorSet();
  color_selector_ =
      group_modifier_container->AddChildView(std::make_unique<ColorPickerView>(
          this, colors_, initial_color_id,
          base::Bind(&TabGroupEditorBubbleView::UpdateGroup,
                     base::Unretained(this))));
  color_selector_->SetProperty(
      views::kMarginsKey,
      gfx::Insets(0, control_insets.left(), 0, control_insets.right()));

  AddChildView(std::make_unique<views::Separator>());

  views::View* menu_items_container =
      AddChildView(std::make_unique<views::View>());
  menu_items_container->SetBorder(
      views::CreateEmptyBorder(gfx::Insets(control_insets.top(), 0)));
  views::FlexLayout* layout_manager_ = menu_items_container->SetLayoutManager(
      std::make_unique<views::FlexLayout>());
  layout_manager_->SetOrientation(views::LayoutOrientation::kVertical)
      .SetIgnoreDefaultMainAxisMargins(true);

  std::unique_ptr<views::LabelButton> new_tab_menu_item = CreateBubbleMenuItem(
      TAB_GROUP_HEADER_CXMENU_NEW_TAB_IN_GROUP,
      l10n_util::GetStringUTF16(IDS_TAB_GROUP_HEADER_CXMENU_NEW_TAB_IN_GROUP),
      &button_listener_);
  new_tab_menu_item->SetBorder(views::CreateEmptyBorder(control_insets));
  menu_items_container->AddChildView(std::move(new_tab_menu_item));

  std::unique_ptr<views::LabelButton> ungroup_menu_item = CreateBubbleMenuItem(
      TAB_GROUP_HEADER_CXMENU_UNGROUP,
      l10n_util::GetStringUTF16(IDS_TAB_GROUP_HEADER_CXMENU_UNGROUP),
      &button_listener_);
  ungroup_menu_item->SetBorder(views::CreateEmptyBorder(control_insets));
  menu_items_container->AddChildView(std::move(ungroup_menu_item));

  std::unique_ptr<views::LabelButton> close_menu_item = CreateBubbleMenuItem(
      TAB_GROUP_HEADER_CXMENU_CLOSE_GROUP,
      l10n_util::GetStringUTF16(IDS_TAB_GROUP_HEADER_CXMENU_CLOSE_GROUP),
      &button_listener_);
  close_menu_item->SetBorder(views::CreateEmptyBorder(control_insets));
  menu_items_container->AddChildView(std::move(close_menu_item));

  std::unique_ptr<views::LabelButton> move_to_new_window_menu_item =
      CreateBubbleMenuItem(
          TAB_GROUP_HEADER_CXMENU_MOVE_GROUP_TO_NEW_WINDOW,
          l10n_util::GetStringUTF16(
              IDS_TAB_GROUP_HEADER_CXMENU_MOVE_GROUP_TO_NEW_WINDOW),
          &button_listener_);
  move_to_new_window_menu_item->SetBorder(
      views::CreateEmptyBorder(control_insets));
  menu_items_container->AddChildView(std::move(move_to_new_window_menu_item));

  if (base::FeatureList::IsEnabled(features::kTabGroupsFeedback)) {
    std::unique_ptr<views::LabelButton> feedback_menu_item =
        CreateBubbleMenuItem(TAB_GROUP_HEADER_CXMENU_FEEDBACK,
                             l10n_util::GetStringUTF16(
                                 IDS_TAB_GROUP_HEADER_CXMENU_SEND_FEEDBACK),
                             &button_listener_);
    feedback_menu_item->SetBorder(views::CreateEmptyBorder(control_insets));
    menu_items_container->AddChildView(std::move(feedback_menu_item));
  }

  views::FlexLayout* menu_layout_manager_ =
      SetLayoutManager(std::make_unique<views::FlexLayout>());
  menu_layout_manager_->SetOrientation(views::LayoutOrientation::kVertical);
}

TabGroupEditorBubbleView::~TabGroupEditorBubbleView() = default;

tab_groups::TabGroupColorId TabGroupEditorBubbleView::InitColorSet() {
  const tab_groups::ColorLabelMap& color_map =
      tab_groups::GetTabGroupColorLabelMap();

  // TODO(tluk) remove the reliance on the ordering of the color pairs in the
  // vector and use the ColorLabelMap structure instead.
  std::copy(color_map.begin(), color_map.end(), std::back_inserter(colors_));

  // Keep track of the current group's color, to be returned as the initial
  // selected value.
  auto* const group_model = browser_->tab_strip_model()->group_model();
  return group_model->GetTabGroup(group_)->visual_data()->color();
}

void TabGroupEditorBubbleView::UpdateGroup() {
  base::Optional<int> selected_element = color_selector_->GetSelectedElement();
  TabGroup* tab_group =
      browser_->tab_strip_model()->group_model()->GetTabGroup(group_);

  const tab_groups::TabGroupColorId current_color =
      tab_group->visual_data()->color();
  const tab_groups::TabGroupColorId updated_color =
      selected_element.has_value() ? colors_[selected_element.value()].first
                                   : current_color;

  if (current_color != updated_color) {
    base::RecordAction(
        base::UserMetricsAction("TabGroups_TabGroupBubble_ColorChanged"));
  }

  tab_groups::TabGroupVisualData new_data(title_field_->GetText(),
                                          updated_color);
  tab_group->SetVisualData(new_data, true);
}

void TabGroupEditorBubbleView::OnBubbleClose() {
  if (title_at_opening_ != title_field_->GetText()) {
    base::RecordAction(
        base::UserMetricsAction("TabGroups_TabGroupBubble_NameChanged"));
  }
}

void TabGroupEditorBubbleView::TitleFieldController::ContentsChanged(
    views::Textfield* sender,
    const base::string16& new_contents) {
  DCHECK_EQ(sender, parent_->title_field_);
  parent_->UpdateGroup();
}

bool TabGroupEditorBubbleView::TitleFieldController::HandleKeyEvent(
    views::Textfield* sender,
    const ui::KeyEvent& key_event) {
  DCHECK_EQ(sender, parent_->title_field_);

  // For special actions, only respond to key pressed events, to be consistent
  // with other views like buttons and dialogs.
  if (key_event.type() == ui::EventType::ET_KEY_PRESSED) {
    const ui::KeyboardCode key_code = key_event.key_code();
    if (key_code == ui::VKEY_ESCAPE) {
      parent_->GetWidget()->CloseWithReason(
          views::Widget::ClosedReason::kEscKeyPressed);
      return true;
    }
    if (key_code == ui::VKEY_RETURN) {
      parent_->GetWidget()->CloseWithReason(
          views::Widget::ClosedReason::kUnspecified);
      return true;
    }
  }

  return false;
}

void TabGroupEditorBubbleView::TitleField::ShowContextMenu(
    const gfx::Point& p,
    ui::MenuSourceType source_type) {
  // There is no easy way to stop the propagation of a ShowContextMenu event,
  // which is sometimes used to open the bubble itself. So when the bubble is
  // opened this way, we manually hide the textfield's context menu the first
  // time. Otherwise, the textfield, which is automatically focused, would show
  // an extra context menu when the bubble first opens.
  if (stop_context_menu_propagation_) {
    stop_context_menu_propagation_ = false;
    return;
  }
  views::Textfield::ShowContextMenu(p, source_type);
}

TabGroupEditorBubbleView::ButtonListener::ButtonListener(
    const Browser* browser,
    tab_groups::TabGroupId group,
    TabGroupHeader* header_view)
    : browser_(browser), group_(group), header_view_(header_view) {}

void TabGroupEditorBubbleView::ButtonListener::ButtonPressed(
    views::Button* sender,
    const ui::Event& event) {
  TabStripModel* model = browser_->tab_strip_model();
  const std::vector<int> tabs_in_group =
      model->group_model()->GetTabGroup(group_)->ListTabs();
  switch (sender->GetID()) {
    case TAB_GROUP_HEADER_CXMENU_NEW_TAB_IN_GROUP:
      base::RecordAction(
          base::UserMetricsAction("TabGroups_TabGroupBubble_NewTabInGroup"));
      model->delegate()->AddTabAt(GURL(), tabs_in_group.back() + 1, true,
                                  group_);
      break;
    case TAB_GROUP_HEADER_CXMENU_UNGROUP:
      base::RecordAction(
          base::UserMetricsAction("TabGroups_TabGroupBubble_Ungroup"));
      if (header_view_)
        header_view_->RemoveObserverFromWidget(sender->GetWidget());
      model->RemoveFromGroup(tabs_in_group);
      break;
    case TAB_GROUP_HEADER_CXMENU_CLOSE_GROUP:
      base::RecordAction(
          base::UserMetricsAction("TabGroups_TabGroupBubble_CloseGroup"));
      for (int i = tabs_in_group.size() - 1; i >= 0; --i) {
        model->CloseWebContentsAt(
            tabs_in_group[i], TabStripModel::CLOSE_USER_GESTURE |
                                  TabStripModel::CLOSE_CREATE_HISTORICAL_TAB);
      }
      break;
    case TAB_GROUP_HEADER_CXMENU_MOVE_GROUP_TO_NEW_WINDOW:
      model->delegate()->MoveGroupToNewWindow(group_);
      break;
    case TAB_GROUP_HEADER_CXMENU_FEEDBACK: {
      base::RecordAction(
          base::UserMetricsAction("TabGroups_TabGroupBubble_SendFeedback"));
      chrome::ShowFeedbackPage(
          browser_, chrome::FeedbackSource::kFeedbackSourceDesktopTabGroups,
          std::string() /* description_template */,
          std::string() /* description_placeholder_text */,
          std::string("DESKTOP_TAB_GROUPS") /* category_tag */,
          std::string() /* extra_diagnostics */);
      break;
    }
    default:
      NOTREACHED();
  }

  // In the case of closing the tabs in a group or ungrouping the tabs, the
  // widget should be closed because it is no longer applicable. In the case of
  // opening a new tab in the group, the widget is closed to allow users to
  // continue their work in their newly created tab.
  sender->GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kUnspecified);
}
