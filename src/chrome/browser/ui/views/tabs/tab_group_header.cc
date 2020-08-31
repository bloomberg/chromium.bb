// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_group_header.h"

#include <memory>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/views/tabs/tab_controller.h"
#include "chrome/browser/ui/views/tabs/tab_group_editor_bubble_view.h"
#include "chrome/browser/ui/views/tabs/tab_group_underline.h"
#include "chrome/browser/ui/views/tabs/tab_slot_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/tab_strip_layout.h"
#include "chrome/browser/ui/views/tabs/tab_strip_types.h"
#include "chrome/grit/generated_resources.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace {

constexpr int kEmptyChipSize = 14;

int GetChipCornerRadius() {
  return TabStyle::GetCornerRadius() - TabGroupUnderline::kStrokeThickness;
}

class TabGroupHighlightPathGenerator : public views::HighlightPathGenerator {
 public:
  TabGroupHighlightPathGenerator(const views::View* chip,
                                 const views::View* title)
      : chip_(chip), title_(title) {}

  // views::HighlightPathGenerator:
  SkPath GetHighlightPath(const views::View* view) override {
    SkScalar corner_radius =
        title_->GetVisible() ? GetChipCornerRadius() : kEmptyChipSize / 2;
    return SkPath().addRoundRect(gfx::RectToSkRect(chip_->bounds()),
                                 corner_radius, corner_radius);
  }

 private:
  const views::View* const chip_;
  const views::View* const title_;

  DISALLOW_COPY_AND_ASSIGN(TabGroupHighlightPathGenerator);
};

}  // namespace

TabGroupHeader::TabGroupHeader(TabStrip* tab_strip,
                               const tab_groups::TabGroupId& group)
    : tab_strip_(tab_strip) {
  DCHECK(tab_strip);

  set_group(group);
  set_context_menu_controller(this);

  // The size and color of the chip are set in VisualsChanged().
  title_chip_ = AddChildView(std::make_unique<views::View>());

  // The text and color of the title are set in VisualsChanged().
  title_ = title_chip_->AddChildView(std::make_unique<views::Label>());
  title_->SetCollapseWhenHidden(true);
  title_->SetAutoColorReadabilityEnabled(false);
  title_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_->SetElideBehavior(gfx::FADE_TAIL);

  // Enable keyboard focus.
  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
  focus_ring_ = views::FocusRing::Install(this);
  views::HighlightPathGenerator::Install(
      this,
      std::make_unique<TabGroupHighlightPathGenerator>(title_chip_, title_));
}

TabGroupHeader::~TabGroupHeader() = default;

bool TabGroupHeader::OnKeyPressed(const ui::KeyEvent& event) {
  if ((event.key_code() == ui::VKEY_SPACE ||
       event.key_code() == ui::VKEY_RETURN) &&
      !editor_bubble_tracker_.is_open()) {
    editor_bubble_tracker_.Opened(TabGroupEditorBubbleView::Show(
        tab_strip_->controller()->GetBrowser(), group().value(), this));
    return true;
  }

  constexpr int kModifiedFlag =
#if defined(OS_MACOSX)
      ui::EF_COMMAND_DOWN;
#else
      ui::EF_CONTROL_DOWN;
#endif

  if (event.type() == ui::ET_KEY_PRESSED && (event.flags() & kModifiedFlag)) {
    if (event.key_code() == ui::VKEY_RIGHT) {
      tab_strip_->ShiftGroupRight(group().value());
      return true;
    }
    if (event.key_code() == ui::VKEY_LEFT) {
      tab_strip_->ShiftGroupLeft(group().value());
      return true;
    }
  }

  return false;
}

bool TabGroupHeader::OnMousePressed(const ui::MouseEvent& event) {
  // Ignore the click if the editor is already open. Do this so clicking
  // on us again doesn't re-trigger the editor.
  //
  // Though the bubble is deactivated before we receive a mouse event,
  // the actual widget destruction happens in a posted task. That task
  // gets run after we receive the mouse event. If this sounds brittle,
  // that's because it is!
  if (editor_bubble_tracker_.is_open())
    return false;

  tab_strip_->MaybeStartDrag(this, event, tab_strip_->GetSelectionModel());

  return true;
}

bool TabGroupHeader::OnMouseDragged(const ui::MouseEvent& event) {
  tab_strip_->ContinueDrag(this, event);
  return true;
}

void TabGroupHeader::OnMouseReleased(const ui::MouseEvent& event) {
  if (!dragging() && !editor_bubble_tracker_.is_open()) {
    editor_bubble_tracker_.Opened(TabGroupEditorBubbleView::Show(
        tab_strip_->controller()->GetBrowser(), group().value(), this));
  }
  tab_strip_->EndDrag(END_DRAG_COMPLETE);
}

void TabGroupHeader::OnMouseEntered(const ui::MouseEvent& event) {
  // Hide the hover card, since there currently isn't anything to display
  // for a group.
  tab_strip_->UpdateHoverCard(nullptr);
}

void TabGroupHeader::OnThemeChanged() {
  TabSlotView::OnThemeChanged();
  VisualsChanged();
}

void TabGroupHeader::OnGestureEvent(ui::GestureEvent* event) {
  tab_strip_->UpdateHoverCard(nullptr);
  switch (event->type()) {
    case ui::ET_GESTURE_TAP: {
      editor_bubble_tracker_.Opened(TabGroupEditorBubbleView::Show(
          tab_strip_->controller()->GetBrowser(), group().value(), this));
      break;
    }

    case ui::ET_GESTURE_SCROLL_BEGIN: {
      tab_strip_->MaybeStartDrag(this, *event, tab_strip_->GetSelectionModel());
      break;
    }

    default:
      break;
  }
  event->SetHandled();
}

void TabGroupHeader::OnFocus() {
  View::OnFocus();
  tab_strip_->UpdateHoverCard(nullptr);
}

void TabGroupHeader::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kTabList;
  node_data->AddState(ax::mojom::State::kEditable);

  base::string16 title =
      tab_strip_->controller()->GetGroupTitle(group().value());
  base::string16 contents =
      tab_strip_->controller()->GetGroupContentString(group().value());
  if (title.empty()) {
    node_data->SetName(l10n_util::GetStringFUTF16(
        IDS_GROUP_AX_LABEL_UNNAMED_GROUP_FORMAT, contents));
  } else {
    node_data->SetName(l10n_util::GetStringFUTF16(
        IDS_GROUP_AX_LABEL_NAMED_GROUP_FORMAT, title, contents));
  }
}

TabSlotView::ViewType TabGroupHeader::GetTabSlotViewType() const {
  return TabSlotView::ViewType::kTabGroupHeader;
}

TabSizeInfo TabGroupHeader::GetTabSizeInfo() const {
  TabSizeInfo size_info;
  // Group headers have a fixed width based on |title_|'s width.
  const int width = CalculateWidth();
  size_info.pinned_tab_width = width;
  size_info.min_active_width = width;
  size_info.min_inactive_width = width;
  size_info.standard_width = width;
  return size_info;
}

void TabGroupHeader::ShowContextMenuForViewImpl(
    views::View* source,
    const gfx::Point& point,
    ui::MenuSourceType source_type) {
  if (editor_bubble_tracker_.is_open())
    return;

  // When the context menu is triggered via keyboard, the keyboard event
  // propagates to the textfield inside the Editor Bubble. In those cases, we
  // want to tell the Editor Bubble to stop the event by setting
  // stop_context_menu_propagation to true.
  //
  // However, when the context menu is triggered via mouse, the same event
  // sequence doesn't happen. Stopping the context menu propagation in that case
  // would artificially hide the textfield's context menu the first time the
  // user tried to access it. So we don't want to stop the context menu
  // propagation if this call is reached via mouse.
  //
  // Notably, event behavior with a mouse is inconsistent depending on
  // OS. When not on Mac, the OnMouseReleased() event happens first and opens
  // the Editor Bubble early, preempting the Show() call below. On Mac, the
  // ShowContextMenu() event happens first and the Show() call is made here.
  //
  // So, because of the event order on non-Mac, and because there is no native
  // way to open a context menu via keyboard on Mac, we assume that we've
  // reached this function via mouse if and only if the current OS is Mac.
  // Therefore, we don't stop the menu propagation in that case.
  constexpr bool kStopContextMenuPropagation =
#if defined(OS_MACOSX)
      false;
#else
      true;
#endif

  editor_bubble_tracker_.Opened(TabGroupEditorBubbleView::Show(
      tab_strip_->controller()->GetBrowser(), group().value(), this,
      base::nullopt, nullptr, kStopContextMenuPropagation));
}

int TabGroupHeader::CalculateWidth() const {
  // We don't want tabs to visually overlap group headers, so we add that space
  // to the width to compensate. We don't want to actually remove the overlap
  // during layout however; that would cause an the margin to be visually uneven
  // when the header is in the first slot and thus wouldn't overlap anything to
  // the left.
  const int overlap_margin = TabStyle::GetTabOverlap() * 2;

  // The empty and non-empty chips have different sizes and corner radii, but
  // both should look nestled against the group stroke of the tab to the right.
  // This requires a +/- 2px adjustment to the width, which causes the tab to
  // the right to be positioned in the right spot.
  const base::string16 title =
      tab_strip_->controller()->GetGroupTitle(group().value());
  const int right_adjust = title.empty() ? 2 : -2;

  return overlap_margin + title_chip_->width() + right_adjust;
}

void TabGroupHeader::VisualsChanged() {
  const base::string16 title =
      tab_strip_->controller()->GetGroupTitle(group().value());
  const tab_groups::TabGroupColorId color_id =
      tab_strip_->controller()->GetGroupColorId(group().value());
  const SkColor color = tab_strip_->GetPaintedGroupColor(color_id);

  if (title.empty()) {
    // If the title is empty, the chip is just a circle.
    title_->SetVisible(false);

    const int y = (GetLayoutConstant(TAB_HEIGHT) - kEmptyChipSize) / 2;

    title_chip_->SetBounds(TabGroupUnderline::GetStrokeInset(), y,
                           kEmptyChipSize, kEmptyChipSize);
    title_chip_->SetBackground(
        views::CreateRoundedRectBackground(color, kEmptyChipSize / 2));
  } else {
    // If the title is set, the chip is a rounded rect that matches the active
    // tab shape, particularly the tab's corner radius.
    title_->SetVisible(true);
    title_->SetEnabledColor(color_utils::GetColorWithMaxContrast(color));
    title_->SetText(title);

    // Set the radius such that the chip nestles snugly against the tab corner
    // radius, taking into account the group underline stroke.
    const int corner_radius = GetChipCornerRadius();

    // Clamp the width to a maximum of half the standard tab width (not counting
    // overlap).
    const int max_width =
        (TabStyle::GetStandardWidth() - TabStyle::GetTabOverlap()) / 2;
    const int text_width =
        std::min(title_->GetPreferredSize().width(), max_width);
    const int text_height = title_->GetPreferredSize().height();
    const int text_vertical_inset = 1;
    const int text_horizontal_inset = corner_radius + text_vertical_inset;

    const int y =
        (GetLayoutConstant(TAB_HEIGHT) - text_height) / 2 - text_vertical_inset;

    title_chip_->SetBounds(TabGroupUnderline::GetStrokeInset(), y,
                           text_width + 2 * text_horizontal_inset,
                           text_height + 2 * text_vertical_inset);
    title_chip_->SetBackground(
        views::CreateRoundedRectBackground(color, corner_radius));

    title_->SetBounds(text_horizontal_inset, text_vertical_inset, text_width,
                      text_height);
  }

  if (focus_ring_)
    focus_ring_->Layout();
}

void TabGroupHeader::RemoveObserverFromWidget(views::Widget* widget) {
  widget->RemoveObserver(&editor_bubble_tracker_);
}

TabGroupHeader::EditorBubbleTracker::~EditorBubbleTracker() {
  if (is_open_) {
    widget_->RemoveObserver(this);
    widget_->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
  }
}

void TabGroupHeader::EditorBubbleTracker::Opened(views::Widget* bubble_widget) {
  DCHECK(bubble_widget);
  DCHECK(!is_open_);
  widget_ = bubble_widget;
  is_open_ = true;
  bubble_widget->AddObserver(this);
}

void TabGroupHeader::EditorBubbleTracker::OnWidgetDestroyed(
    views::Widget* widget) {
  is_open_ = false;
}
