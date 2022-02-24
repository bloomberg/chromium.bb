// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_strip.h"

#include <stddef.h>

#include <algorithm>
#include <iterator>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/containers/adapters.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/cxx17_backports.h"
#include "base/feature_list.h"
#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/numerics/safe_conversions.h"
#include "base/scoped_observation.h"
#include "base/stl_util.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_group_theme.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_types.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/browser_tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_drag_controller.h"
#include "chrome/browser/ui/views/tabs/tab_group_header.h"
#include "chrome/browser/ui/views/tabs/tab_group_highlight.h"
#include "chrome/browser/ui/views/tabs/tab_group_underline.h"
#include "chrome/browser/ui/views/tabs/tab_group_views.h"
#include "chrome/browser/ui/views/tabs/tab_hover_card_controller.h"
#include "chrome/browser/ui/views/tabs/tab_slot_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/tab_strip_layout_helper.h"
#include "chrome/browser/ui/views/tabs/tab_strip_layout_types.h"
#include "chrome/browser/ui/views/tabs/tab_strip_observer.h"
#include "chrome/browser/ui/views/tabs/tab_style_views.h"
#include "chrome/browser/ui/views/touch_uma/touch_uma.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "third_party/skia/include/core/SkColorFilter.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/pathops/SkPathOps.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/list_selection_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/color/color_provider.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/animation/throb_animation.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/range/range.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/cascading_property.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/masked_targeter_delegate.h"
#include "ui/views/mouse_watcher_view_host.h"
#include "ui/views/rect_based_targeting_utils.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_model_utils.h"
#include "ui/views/view_observer.h"
#include "ui/views/view_targeter.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_version.h"
#include "ui/display/win/screen_win.h"
#include "ui/gfx/win/hwnd_util.h"
#include "ui/views/win/hwnd_util.h"
#endif

#if defined(USE_AURA)
#include "ui/aura/window.h"
#endif

namespace {

// Size of the drop indicator.
int g_drop_indicator_width = 0;
int g_drop_indicator_height = 0;

// Provides the ability to monitor when a tab's bounds have been animated. Used
// to hook callbacks to adjust things like tabstrip preferred size and tab group
// underlines.
class TabSlotAnimationDelegate : public gfx::AnimationDelegate {
 public:
  using OnAnimationProgressedCallback =
      base::RepeatingCallback<void(TabSlotView*)>;

  TabSlotAnimationDelegate(
      TabStrip* tab_strip,
      TabSlotView* slot_view,
      OnAnimationProgressedCallback on_animation_progressed);
  TabSlotAnimationDelegate(const TabSlotAnimationDelegate&) = delete;
  TabSlotAnimationDelegate& operator=(const TabSlotAnimationDelegate&) = delete;
  ~TabSlotAnimationDelegate() override;

  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationCanceled(const gfx::Animation* animation) override;

 protected:
  TabStrip* tab_strip() { return tab_strip_; }
  TabSlotView* slot_view() { return slot_view_; }

 private:
  const raw_ptr<TabStrip> tab_strip_;
  const raw_ptr<TabSlotView> slot_view_;
  OnAnimationProgressedCallback on_animation_progressed_;
};

TabSlotAnimationDelegate::TabSlotAnimationDelegate(
    TabStrip* tab_strip,
    TabSlotView* slot_view,
    OnAnimationProgressedCallback on_animation_progressed)
    : tab_strip_(tab_strip),
      slot_view_(slot_view),
      on_animation_progressed_(on_animation_progressed) {
  slot_view_->set_animating(true);
}

TabSlotAnimationDelegate::~TabSlotAnimationDelegate() = default;

void TabSlotAnimationDelegate::AnimationProgressed(
    const gfx::Animation* animation) {
  on_animation_progressed_.Run(slot_view());
}

void TabSlotAnimationDelegate::AnimationEnded(const gfx::Animation* animation) {
  slot_view_->set_animating(false);
  AnimationProgressed(animation);
  slot_view_->Layout();
}

void TabSlotAnimationDelegate::AnimationCanceled(
    const gfx::Animation* animation) {
  AnimationEnded(animation);
}

// Animation delegate used when a dragged tab is released. When done sets the
// dragging state to false.
class ResetDraggingStateDelegate : public TabSlotAnimationDelegate {
 public:
  ResetDraggingStateDelegate(
      TabStrip* tab_strip,
      Tab* tab,
      OnAnimationProgressedCallback on_animation_progressed);
  ResetDraggingStateDelegate(const ResetDraggingStateDelegate&) = delete;
  ResetDraggingStateDelegate& operator=(const ResetDraggingStateDelegate&) =
      delete;
  ~ResetDraggingStateDelegate() override;

  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationCanceled(const gfx::Animation* animation) override;
};

ResetDraggingStateDelegate::ResetDraggingStateDelegate(
    TabStrip* tab_strip,
    Tab* tab,
    OnAnimationProgressedCallback on_animation_progressed)
    : TabSlotAnimationDelegate(tab_strip, tab, on_animation_progressed) {}

ResetDraggingStateDelegate::~ResetDraggingStateDelegate() = default;

void ResetDraggingStateDelegate::AnimationEnded(
    const gfx::Animation* animation) {
  static_cast<Tab*>(slot_view())->set_dragging(false);
  TabSlotAnimationDelegate::AnimationEnded(animation);
}

void ResetDraggingStateDelegate::AnimationCanceled(
    const gfx::Animation* animation) {
  AnimationEnded(animation);
}

TabDragController::EventSource EventSourceFromEvent(
    const ui::LocatedEvent& event) {
  return event.IsGestureEvent() ? TabDragController::EVENT_SOURCE_TOUCH
                                : TabDragController::EVENT_SOURCE_MOUSE;
}

// Helper class that manages the tab scrolling animation.
class TabScrollingAnimation : public gfx::LinearAnimation,
                              public gfx::AnimationDelegate {
 public:
  explicit TabScrollingAnimation(
      TabStrip* tab_strip,
      gfx::AnimationContainer* bounds_animator_container,
      base::TimeDelta duration,
      const gfx::Rect start_visible_rect,
      const gfx::Rect end_visible_rect)
      : gfx::LinearAnimation(duration,
                             gfx::LinearAnimation::kDefaultFrameRate,
                             this),
        tab_strip_(tab_strip),
        start_visible_rect_(start_visible_rect),
        end_visible_rect_(end_visible_rect) {
    SetContainer(bounds_animator_container);
  }
  TabScrollingAnimation(const TabScrollingAnimation&) = delete;
  TabScrollingAnimation& operator=(const TabScrollingAnimation&) = delete;
  ~TabScrollingAnimation() override = default;

  void AnimateToState(double state) override {
    gfx::Rect intermediary_rect(
        start_visible_rect_.x() +
            (end_visible_rect_.x() - start_visible_rect_.x()) * state,
        start_visible_rect_.y(), start_visible_rect_.width(),
        start_visible_rect_.height());

    tab_strip_->ScrollRectToVisible(intermediary_rect);
  }

  void AnimationEnded(const gfx::Animation* animation) override {
    tab_strip_->ScrollRectToVisible(end_visible_rect_);
  }

 private:
  const raw_ptr<TabStrip> tab_strip_;
  const gfx::Rect start_visible_rect_;
  const gfx::Rect end_visible_rect_;
};

// A class that calculates a z-value for a TabStrip child view (one of a tab, a
// tab group header, a tab group underline, or a tab group highlight). Can be
// compared with other ZOrderableTabStripElements to determine paint order of
// their associated views.
class ZOrderableTabStripElement {
 public:
  ZOrderableTabStripElement(views::View* const child,
                            absl::optional<const TabGroupUnderline* const>
                                dragging_tabs_current_group_underline)
      : child_(child),
        z_value_(
            CalculateZValue(child, dragging_tabs_current_group_underline)) {}

  bool operator<(const ZOrderableTabStripElement& rhs) const {
    return z_value_ < rhs.z_value_;
  }

  views::View* view() const { return child_; }

 private:
  // Determines the 'height' of |child|, which should be used to determine the
  // paint order of TabStrip's children.  Larger z-values should be painted on
  // top of smaller ones.
  static float CalculateZValue(views::View* child,
                               absl::optional<const TabGroupUnderline* const>
                                   dragging_tabs_current_group_underline) {
    Tab* tab = views::AsViewClass<Tab>(child);
    TabGroupHeader* header = views::AsViewClass<TabGroupHeader>(child);
    TabGroupUnderline* underline = views::AsViewClass<TabGroupUnderline>(child);
    TabGroupHighlight* highlight = views::AsViewClass<TabGroupHighlight>(child);
    DCHECK_EQ(1, !!tab + !!header + !!underline + !!highlight);

    // Construct a bitfield that encodes |child|'s z-value. Higher-order bits
    // encode more important properties - see usage below for details on each.
    // The lowest-order |num_bits_reserved_for_tab_style_z_value| bits are
    // reserved for the factors considered by TabStyle, e.g. selection and hover
    // state.
    constexpr int num_bits_reserved_for_tab_style_z_value =
        base::bits::Log2Ceiling(static_cast<int>(TabStyle::kMaximumZValue) + 1);
    enum ZValue {
      kActiveTab = (1u << (num_bits_reserved_for_tab_style_z_value + 4)),
      kDraggedHeader = (1u << (num_bits_reserved_for_tab_style_z_value + 3)),
      kDragRelevantUnderline =
          (1u << (num_bits_reserved_for_tab_style_z_value + 2)),
      kDraggedTab = (1u << (num_bits_reserved_for_tab_style_z_value + 1)),
      kGroupView = (1u << num_bits_reserved_for_tab_style_z_value)
    };

    unsigned int z_value = 0;

    // The active tab is always on top.
    if (tab && tab->IsActive())
      z_value |= kActiveTab;

    // If we're dragging a header, that is painted above non-active tabs.
    if (header && header->dragging())
      z_value |= kDraggedHeader;

    // If we're dragging tabs into a group, or are dragging a group, the
    // underline for that group is painted above non-active dragged tabs.
    if (underline && dragging_tabs_current_group_underline.has_value() &&
        underline == dragging_tabs_current_group_underline.value())
      z_value |= kDragRelevantUnderline;

    // Dragged tabs are painted above anything that isn't part of the drag.
    if (tab && tab->dragging())
      z_value |= kDraggedTab;

    // Group headers, highlights and underlines are painted above non-active,
    // non-dragged tabs. Note that a group highlight is only visible when the
    // associated group is being dragged in a header drag.
    if (header || underline || highlight)
      z_value |= kGroupView;

    // The remaining (non-active, non-dragged) tabs are painted last. They are
    // ordered by their selected or hovered state, which is animated and thus
    // real-valued.
    const float tab_style_z_value = tab ? tab->tab_style()->GetZValue() : 0.0f;
    return z_value + tab_style_z_value;
  }

  views::View* child_;
  float z_value_;
};  // ZOrderableTabStripElement

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// TabStrip::RemoveTabDelegate
//
// AnimationDelegate used when removing a tab. Does the necessary cleanup when
// done.
class TabStrip::RemoveTabDelegate : public TabSlotAnimationDelegate {
 public:
  RemoveTabDelegate(TabStrip* tab_strip,
                    Tab* tab,
                    OnAnimationProgressedCallback on_animation_progressed);
  RemoveTabDelegate(const RemoveTabDelegate&) = delete;
  RemoveTabDelegate& operator=(const RemoveTabDelegate&) = delete;

  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationCanceled(const gfx::Animation* animation) override;
};

TabStrip::RemoveTabDelegate::RemoveTabDelegate(
    TabStrip* tab_strip,
    Tab* tab,
    OnAnimationProgressedCallback on_animation_progressed)
    : TabSlotAnimationDelegate(tab_strip, tab, on_animation_progressed) {}

void TabStrip::RemoveTabDelegate::AnimationEnded(
    const gfx::Animation* animation) {
  tab_strip()->OnTabCloseAnimationCompleted(static_cast<Tab*>(slot_view()));
}

void TabStrip::RemoveTabDelegate::AnimationCanceled(
    const gfx::Animation* animation) {
  AnimationEnded(animation);
}

///////////////////////////////////////////////////////////////////////////////
// TabStrip::TabDragContextImpl
//
class TabStrip::TabDragContextImpl : public TabDragContext {
 public:
  explicit TabDragContextImpl(TabStrip* tab_strip) : tab_strip_(tab_strip) {}

  bool IsDragStarted() const {
    return drag_controller_ && drag_controller_->started_drag();
  }

  bool IsMutating() const {
    return drag_controller_ && drag_controller_->is_mutating();
  }

  bool IsDraggingWindow() const {
    return drag_controller_ && drag_controller_->is_dragging_window();
  }

  bool IsDraggingTab(content::WebContents* contents) const {
    return contents && drag_controller_ &&
           drag_controller_->IsDraggingTab(contents);
  }

  void MaybeStartDrag(TabSlotView* source,
                      const ui::LocatedEvent& event,
                      const ui::ListSelectionModel& original_selection) {
    std::vector<TabSlotView*> dragging_views;
    int x = source->GetMirroredXInView(event.x());
    int y = event.y();

    // Build the set of selected tabs to drag and calculate the offset from the
    // source.
    ui::ListSelectionModel selection_model;
    if (source->GetTabSlotViewType() ==
        TabSlotView::ViewType::kTabGroupHeader) {
      dragging_views.push_back(source);

      const gfx::Range grouped_tabs =
          tab_strip_->controller()->ListTabsInGroup(source->group().value());
      for (auto index = grouped_tabs.start(); index < grouped_tabs.end();
           ++index) {
        dragging_views.push_back(GetTabAt(index));
        // Set |selection_model| if and only if the original selection does not
        // match the group exactly. See TabDragController::Init() for details
        // on how |selection_model| is used.
        if (!original_selection.IsSelected(index))
          selection_model = original_selection;
      }
      if (grouped_tabs.length() != original_selection.size())
        selection_model = original_selection;
    } else {
      for (int i = 0; i < GetTabCount(); ++i) {
        Tab* other_tab = GetTabAt(i);
        if (tab_strip_->IsTabSelected(other_tab)) {
          dragging_views.push_back(other_tab);
          if (other_tab == source)
            x += GetSizeNeededForViews(dragging_views) - other_tab->width();
        }
      }
      if (!original_selection.IsSelected(tab_strip_->GetModelIndexOf(source)))
        selection_model = original_selection;
    }

    DCHECK(!dragging_views.empty());
    DCHECK(base::Contains(dragging_views, source));

    // Delete the existing DragController before creating a new one. We do this
    // as creating the DragController remembers the WebContents delegates and we
    // need to make sure the existing DragController isn't still a delegate.
    drag_controller_.reset();

    DCHECK(event.type() == ui::ET_MOUSE_PRESSED ||
           event.type() == ui::ET_GESTURE_TAP_DOWN ||
           event.type() == ui::ET_GESTURE_SCROLL_BEGIN);

    drag_controller_ = std::make_unique<TabDragController>();
    drag_controller_->Init(this, source, dragging_views, gfx::Point(x, y),
                           event.x(), std::move(selection_model),
                           EventSourceFromEvent(event));
    if (drag_controller_set_callback_)
      std::move(drag_controller_set_callback_).Run(drag_controller_.get());
  }

  void ContinueDrag(views::View* view, const ui::LocatedEvent& event) {
    if (drag_controller_.get() &&
        drag_controller_->event_source() == EventSourceFromEvent(event)) {
      gfx::Point screen_location(event.location());
      views::View::ConvertPointToScreen(view, &screen_location);

      // Note: |tab_strip_| can be destroyed during drag, also destroying
      // |this|.
      base::WeakPtr<TabDragContext> weak_ptr(weak_factory_.GetWeakPtr());
      drag_controller_->Drag(screen_location);

      if (!weak_ptr)
        return;
    }

    // Note: |drag_controller| can be set to null during the drag above.
    if (drag_controller_ && drag_controller_->group())
      tab_strip_->UpdateTabGroupVisuals(*drag_controller_->group());
  }

  bool EndDrag(EndDragReason reason) {
    if (!drag_controller_.get())
      return false;
    bool started_drag = drag_controller_->started_drag();
    drag_controller_->EndDrag(reason);
    return started_drag;
  }

  // TabDragContext:
  views::View* AsView() override { return tab_strip_; }

  const views::View* AsView() const override { return tab_strip_; }

  Tab* GetTabAt(int i) const override { return tab_strip_->tab_at(i); }

  int GetIndexOf(const TabSlotView* view) const override {
    return tab_strip_->GetModelIndexOf(view);
  }

  int GetTabCount() const override { return tab_strip_->GetTabCount(); }

  bool IsTabPinned(const Tab* tab) const override {
    return tab_strip_->IsTabPinned(tab);
  }

  int GetPinnedTabCount() const override {
    return tab_strip_->GetPinnedTabCount();
  }

  TabGroupHeader* GetTabGroupHeader(
      const tab_groups::TabGroupId& group) const override {
    return tab_strip_->group_header(group);
  }

  TabStripModel* GetTabStripModel() override {
    return static_cast<BrowserTabStripController*>(
               tab_strip_->controller_.get())
        ->model();
  }

  TabDragController* GetDragController() override {
    return drag_controller_.get();
  }

  void OwnDragController(
      std::unique_ptr<TabDragController> controller) override {
    DCHECK(controller);
    DCHECK(!drag_controller_);
    drag_controller_ = std::move(controller);
    if (drag_controller_set_callback_)
      std::move(drag_controller_set_callback_).Run(drag_controller_.get());
  }

  void DestroyDragController() override {
    drag_controller_.reset();
  }

  std::unique_ptr<TabDragController> ReleaseDragController() override {
    return std::move(drag_controller_);
  }

  void SetDragControllerCallbackForTesting(
      base::OnceCallback<void(TabDragController*)> callback) override {
    drag_controller_set_callback_ = std::move(callback);
  }

  bool IsDragSessionActive() const override {
    return drag_controller_ != nullptr;
  }

  bool IsActiveDropTarget() const override {
    for (int i = 0; i < GetTabCount(); ++i) {
      const Tab* const tab = GetTabAt(i);
      if (tab->dragging())
        return true;
    }
    return false;
  }

  std::vector<int> GetTabXCoordinates() const override {
    std::vector<int> results;
    for (int i = 0; i < GetTabCount(); ++i)
      results.push_back(ideal_bounds(i).x());
    return results;
  }

  int GetActiveTabWidth() const override {
    return tab_strip_->GetActiveTabWidth();
  }

  int GetTabDragAreaWidth() const override {
    // There are two cases here (with tab scrolling enabled):
    // 1) If the tab strip is not wider than the tab strip region (and thus
    // not scrollable), returning the available width for tabs rather than the
    // actual width for tabs allows tabs to be dragged past the current bounds
    // of the tabstrip, anywhere along the tab strip region.
    // N.B. The available width for tabs in this case needs to ignore tab
    // closing mode.
    // 2) If the tabstrip is wider than the tab strip region (and thus is
    // scrollable), returning the tabstrip width allows tabs to be dragged
    // anywhere within the tabstrip, not just in the leftmost region of it.
    return std::max(tab_strip_->GetAvailableWidthForTabStrip(),
                    tab_strip_->width());
  }

  int TabDragAreaBeginX() const override {
    return tab_strip_->GetMirroredXWithWidthInView(0, GetTabDragAreaWidth());
  }

  int TabDragAreaEndX() const override {
    return TabDragAreaBeginX() + GetTabDragAreaWidth();
  }

  int GetHorizontalDragThreshold() const override {
    constexpr int kHorizontalMoveThreshold = 16;  // DIPs.

    double ratio = static_cast<double>(tab_strip_->GetInactiveTabWidth()) /
                   TabStyle::GetStandardWidth();
    return base::ClampRound(ratio * kHorizontalMoveThreshold);
  }

  int GetInsertionIndexForDraggedBounds(
      const gfx::Rect& dragged_bounds,
      std::vector<TabSlotView*> dragged_views,
      int num_dragged_tabs,
      absl::optional<tab_groups::TabGroupId> group) const override {
    // If the strip has no tabs, the only position to insert at is 0.
    if (!GetTabCount())
      return 0;

    absl::optional<int> index;
    // If we're dragging a group by its header, the first element of
    // |dragged_views| is a group header, and the second one is the first tab
    // in that group.
    int first_dragged_tab_index = group.has_value() ? 1 : 0;
    if (static_cast<size_t>(first_dragged_tab_index) >= dragged_views.size()) {
      // TODO(tbergquist): This shouldn't happen, but we're getting crashes
      // that indicate that it might be anyways. This logging might help
      // narrow down exactly which cases it's happening in.
      NOTREACHED()
          << "Calculating a drag insertion index from invalid dependencies: "
          << "Dragging a group: " << group.has_value()
          << ", dragged_views.size(): " << dragged_views.size()
          << ", num_dragged_tabs: " << num_dragged_tabs;
    } else {
      int first_dragged_tab_model_index =
          tab_strip_->GetModelIndexOf(dragged_views[first_dragged_tab_index]);
      index =
          CalculateInsertionIndex(dragged_bounds, first_dragged_tab_model_index,
                                  num_dragged_tabs, std::move(group));
    }

    if (!index) {
      const int last_tab_right = ideal_bounds(GetTabCount() - 1).right();
      index = (dragged_bounds.right() > last_tab_right) ? GetTabCount() : 0;
    }

    const Tab* last_visible_tab = tab_strip_->GetLastVisibleTab();
    int last_insertion_point =
        last_visible_tab ? (GetIndexOf(last_visible_tab) + 1) : 0;

    // Clamp the insertion point to keep it within the visible region.
    last_insertion_point = std::max(0, last_insertion_point - num_dragged_tabs);

    // Ensure the first dragged tab always stays in the visible index range.
    return std::min(*index, last_insertion_point);
  }

  std::vector<gfx::Rect> CalculateBoundsForDraggedViews(
      const std::vector<TabSlotView*>& views) override {
    DCHECK(!views.empty());

    std::vector<gfx::Rect> bounds;
    const int overlap = TabStyle::GetTabOverlap();
    int x = 0;
    for (const TabSlotView* view : views) {
      const int width = view->width();
      bounds.push_back(gfx::Rect(x, 0, width, view->height()));
      x += width - overlap;
    }

    return bounds;
  }

  void SetBoundsForDrag(const std::vector<TabSlotView*>& views,
                        const std::vector<gfx::Rect>& bounds) override {
    tab_strip_->StopAnimating(false);
    DCHECK_EQ(views.size(), bounds.size());
    for (size_t i = 0; i < views.size(); ++i)
      views[i]->SetBoundsRect(bounds[i]);

    // Ensure that the tab strip and its parent views are correctly re-laid out
    // after repositioning dragged tabs. This avoids visual/layout issues such
    // as https://crbug.com/1151092.
    tab_strip_->PreferredSizeChanged();

    // Reset the layout size as we've effectively laid out a different size.
    // This ensures a layout happens after the drag is done.
    tab_strip_->last_layout_size_ = gfx::Size();
    if (views.at(0)->group().has_value())
      tab_strip_->UpdateTabGroupVisuals(views.at(0)->group().value());
  }

  void StartedDragging(const std::vector<TabSlotView*>& views) override {
    // Let the controller know that the user started dragging tabs.
    tab_strip_->controller_->OnStartedDragging(
        views.size() == static_cast<size_t>(tab_strip_->GetModelCount()));

    // Reset dragging state of existing tabs.
    for (int i = 0; i < GetTabCount(); ++i)
      GetTabAt(i)->set_dragging(false);

    for (size_t i = 0; i < views.size(); ++i) {
      views[i]->set_dragging(true);
      tab_strip_->bounds_animator_.StopAnimatingView(views[i]);
    }

    // Move the dragged tabs to their ideal bounds.
    tab_strip_->UpdateIdealBounds();

    // Sets the bounds of the dragged tab slots.
    for (TabSlotView* view : views) {
      if (view->GetTabSlotViewType() ==
          TabSlotView::ViewType::kTabGroupHeader) {
        view->SetBoundsRect(ideal_bounds(view->group().value()));
      } else {
        int tab_data_index = GetIndexOf(view);
        DCHECK_NE(TabStripModel::kNoTab, tab_data_index);
        view->SetBoundsRect(ideal_bounds(tab_data_index));
      }
    }

    // If this is a header drag, start painting the group highlight.
    TabGroupHeader* header = views::AsViewClass<TabGroupHeader>(views[0]);
    if (header) {
      tab_strip_->group_views_[header->group().value()]
          ->highlight()
          ->SetVisible(true);
    }

    tab_strip_->SetTabSlotVisibility();
    tab_strip_->SchedulePaint();
  }

  void DraggedTabsDetached() override {
    // Let the controller know that the user is not dragging this tabstrip's
    // tabs anymore.
    tab_strip_->controller_->OnStoppedDragging();
  }

  void StoppedDragging(const std::vector<TabSlotView*>& views,
                       const std::vector<int>& initial_positions,
                       bool completed) override {
    // Let the controller know that the user stopped dragging tabs.
    tab_strip_->controller_->OnStoppedDragging();

    bool is_first_view = true;
    for (size_t i = 0; i < views.size(); ++i)
      tab_strip_->StoppedDraggingView(views[i], &is_first_view);
  }

  void LayoutDraggedViewsAt(const std::vector<TabSlotView*>& views,
                            TabSlotView* source_view,
                            const gfx::Point& location,
                            bool initial_drag) override {
    std::vector<gfx::Rect> bounds = CalculateBoundsForDraggedViews(views);
    DCHECK_EQ(views.size(), bounds.size());

    int active_tab_model_index = GetIndexOf(source_view);
    int active_tab_index = static_cast<int>(
        std::find(views.begin(), views.end(), source_view) - views.begin());
    for (size_t i = 0; i < views.size(); ++i) {
      TabSlotView* view = views[i];
      gfx::Rect new_bounds = bounds[i];
      new_bounds.Offset(location.x(), location.y());
      int consecutive_index =
          active_tab_model_index - (active_tab_index - static_cast<int>(i));
      // If this is the initial layout during a drag and the tabs aren't
      // consecutive animate the view into position. Do the same if the tab is
      // already animating (which means we previously caused it to animate).
      if ((initial_drag && GetIndexOf(views[i]) != consecutive_index) ||
          tab_strip_->bounds_animator_.IsAnimating(views[i])) {
        tab_strip_->bounds_animator_.SetTargetBounds(views[i], new_bounds);
      } else {
        view->SetBoundsRect(new_bounds);
      }
    }
    tab_strip_->SetTabSlotVisibility();
    // The rightmost tab may have moved, which would change the tabstrip's
    // preferred width.
    tab_strip_->PreferredSizeChanged();
  }

  // Forces the entire tabstrip to lay out.
  void ForceLayout() override {
    tab_strip_->InvalidateLayout();
    tab_strip_->CompleteAnimationAndLayout();
  }

 private:
  gfx::Rect ideal_bounds(int i) const { return tab_strip_->ideal_bounds(i); }

  gfx::Rect ideal_bounds(tab_groups::TabGroupId group) const {
    return tab_strip_->ideal_bounds(group);
  }

  // Determines the index to move the dragged tabs to. The dragged tabs must
  // already be in the tabstrip. |dragged_bounds| is the union of the bounds
  // of the dragged tabs and group header, if any. |first_dragged_tab_index| is
  // the current model index in this tabstrip of the first dragged tab. The
  // dragged tabs must be in the tabstrip already!
  int CalculateInsertionIndex(
      const gfx::Rect& dragged_bounds,
      int first_dragged_tab_index,
      int num_dragged_tabs,
      absl::optional<tab_groups::TabGroupId> dragged_group) const {
    // This method assumes that the dragged tabs and group are already in the
    // tabstrip (i.e. it doesn't support attaching a drag to a new tabstrip).
    // This assumption is critical because it means that tab width won't change
    // after this method's recommendation is implemented.

    // For each possible insertion index, determine what the ideal bounds of
    // the dragged tabs would be at that index. This corresponds to where they
    // would slide to if the drag session ended now. We want to insert at the
    // index that minimizes the distance between the corresponding ideal bounds
    // and the current bounds of the tabs. This is equivalent to minimizing:
    //   - the distance of the aforementioned slide,
    //   - the width of the gaps in the tabstrip, or
    //   - the amount of tab overlap.
    int min_distance_index = -1;
    int min_distance = std::numeric_limits<int>::max();
    for (int candidate_index = 0; candidate_index <= GetTabCount();
         ++candidate_index) {
      if (!IsValidInsertionIndex(candidate_index, first_dragged_tab_index,
                                 num_dragged_tabs, dragged_group)) {
        continue;
      }

      // If there's a group header here, and we're dragging a group, we might
      // end up on either side of that header. Check both cases to find the
      // best option.
      // TODO(tbergquist): Use this approach to determine if a tab should be
      // added to the group. This is calculated elsewhere and may require some
      // plumbing and/or duplicated code.
      const int left_ideal_x = CalculateIdealX(
          candidate_index, first_dragged_tab_index, dragged_bounds);
      const int left_distance = std::abs(dragged_bounds.x() - left_ideal_x);

      const int right_ideal_x =
          left_ideal_x + CalculateIdealXAdjustmentIfAddedToGroup(
                             candidate_index, dragged_group);
      const int right_distance = std::abs(dragged_bounds.x() - right_ideal_x);

      const int distance = std::min(left_distance, right_distance);
      if (distance < min_distance) {
        min_distance = distance;
        min_distance_index = candidate_index;
      }
    }

    if (min_distance_index == -1) {
      NOTREACHED();
      return 0;
    }

    // When moving a tab within a tabstrip, the target index is expressed as if
    // the tabs are not in the tabstrip, i.e. it acts like the tabs are first
    // removed and then re-inserted at the target index. We need to adjust the
    // target index to account for this.
    if (min_distance_index > first_dragged_tab_index)
      min_distance_index -= num_dragged_tabs;

    return min_distance_index;
  }

  // Dragging can't insert tabs into some indices.
  bool IsValidInsertionIndex(
      int candidate_index,
      int first_dragged_tab_index,
      int num_dragged_tabs,
      absl::optional<tab_groups::TabGroupId> dragged_group) const {
    if (candidate_index == 0)
      return true;

    // If |candidate_index| is right after one of the tabs we're dragging,
    // inserting here would be nonsensical - we can't insert the dragged tabs
    // into the middle of the dragged tabs. That's just silly.
    if (candidate_index > first_dragged_tab_index &&
        candidate_index <= first_dragged_tab_index + num_dragged_tabs) {
      return false;
    }

    // This might be in the middle of a group, which may or may not be fine.
    absl::optional<tab_groups::TabGroupId> left_group =
        GetTabAt(candidate_index - 1)->group();
    absl::optional<tab_groups::TabGroupId> right_group =
        tab_strip_->IsValidModelIndex(candidate_index)
            ? GetTabAt(candidate_index)->group()
            : absl::nullopt;
    if (left_group.has_value() && left_group == right_group) {
      // Can't drag a group into another group.
      if (dragged_group.has_value())
        return false;
      // Can't drag a tab into a collapsed group.
      if (tab_strip_->controller()->IsGroupCollapsed(left_group.value()))
        return false;
    }

    return true;
  }

  // Determines the x position that the dragged tabs would have if they were
  // inserted at |candidate_index|. If there's a group header at that index,
  // this assumes the dragged tabs *would not* be inserted into the group,
  // and would therefore end up to the left of that header.
  int CalculateIdealX(int candidate_index,
                      int first_dragged_tab_index,
                      gfx::Rect dragged_bounds) const {
    if (candidate_index == 0)
      return 0;

    const int tab_overlap = TabStyle::GetTabOverlap();

    // We'll insert just right of the tab at |candidate_index| - 1.
    int ideal_x = ideal_bounds(candidate_index - 1).right();

    // If the dragged tabs are currently left of |candidate_index|, moving
    // them to |candidate_index| would move the tab at |candidate_index| - 1
    // to the left by |num_dragged_tabs| slots. This would change the ideal x
    // for the dragged tabs, as well, by the width of the dragged tabs.
    if (candidate_index - 1 > first_dragged_tab_index)
      ideal_x -= dragged_bounds.width() - tab_overlap;

    return ideal_x - tab_overlap;
  }

  // There might be a group starting at |candidate_index|. If there is,
  // this determines how the ideal x would change if the dragged tabs were
  // added to that group, thereby moving them to that header's right.
  int CalculateIdealXAdjustmentIfAddedToGroup(
      int candidate_index,
      absl::optional<tab_groups::TabGroupId> dragged_group) const {
    // If the tab to the right of |candidate_index| is the first tab in a
    // (non-collapsed) group, we are sharing this model index with a group
    // header. We might end up on either side of it, so we need to check
    // both positions.
    if (!dragged_group.has_value() &&
        tab_strip_->IsValidModelIndex(candidate_index)) {
      absl::optional<tab_groups::TabGroupId> left_group =
          tab_strip_->IsValidModelIndex(candidate_index - 1)
              ? GetTabAt(candidate_index - 1)->group()
              : absl::nullopt;
      absl::optional<tab_groups::TabGroupId> right_group =
          GetTabAt(candidate_index)->group();
      if (right_group.has_value() && left_group != right_group) {
        if (tab_strip_->controller()->IsGroupCollapsed(right_group.value()))
          return 0;
        const int header_width =
            GetTabGroupHeader(*right_group)->bounds().width() -
            TabStyle::GetTabOverlap();
        return header_width;
      }
    }

    return 0;
  }

  // Sets the ideal bounds x-coordinates to |positions|.
  void SetIdealBoundsFromPositions(const std::vector<int>& positions) {
    if (static_cast<size_t>(GetTabCount()) != positions.size())
      return;

    for (int i = 0; i < GetTabCount(); ++i) {
      gfx::Rect bounds(ideal_bounds(i));
      bounds.set_x(positions[i]);
      tab_strip_->tab_container_->tabs_view_model()->set_ideal_bounds(i,
                                                                      bounds);
    }
  }

  const raw_ptr<TabStrip> tab_strip_;

  // The controller for a drag initiated from a Tab. Valid for the lifetime of
  // the drag session.
  std::unique_ptr<TabDragController> drag_controller_;

  // Only used in tests.
  base::OnceCallback<void(TabDragController*)> drag_controller_set_callback_;

  base::WeakPtrFactory<TabDragContext> weak_factory_{this};
};

///////////////////////////////////////////////////////////////////////////////
// TabStrip, public:

TabStrip::TabStrip(std::unique_ptr<TabStripController> controller)
    : controller_(std::move(controller)),
      tab_container_(
          AddChildView(std::make_unique<TabContainer>(controller_.get()))),
      bounds_animator_(tab_container_),
      hover_card_controller_(std::make_unique<TabHoverCardController>(this)),
      drag_context_(std::make_unique<TabDragContextImpl>(this)) {
  // TODO(pbos): This is probably incorrect, the background of individual tabs
  // depend on their selected state. This should probably be pushed down into
  // tabs.
  views::SetCascadingThemeProviderColor(this, views::kCascadingBackgroundColor,
                                        ThemeProperties::COLOR_TOOLBAR);
  Init();

  SetProperty(views::kElementIdentifierKey, kTabStripElementId);
}

TabStrip::~TabStrip() {
  // Eliminate the hover card first to avoid order-of-operation issues.
  hover_card_controller_.reset();

  // The animations may reference the tabs. Shut down the animation before we
  // delete the tabs.
  StopAnimating(false);

  // Disengage the drag controller before doing any additional cleanup. This
  // call can interact with child views so we can't reliably do it during member
  // destruction.
  drag_context_->DestroyDragController();

  // Make sure we unhook ourselves as a message loop observer so that we don't
  // crash in the case where the user closes the window after closing a tab
  // but before moving the mouse.
  RemoveMessageLoopObserver();

  // Since TabGroupViews expects be able to remove the views it creates, clear
  // |group_views_| before removing the remaining children below.
  group_views_.clear();

  // The child tabs may call back to us from their destructors. Delete them so
  // that if they call back we aren't in a weird state.
  RemoveAllChildViews();

  CHECK(!IsInObserverList());
}

void TabStrip::SetAvailableWidthCallback(
    base::RepeatingCallback<int()> available_width_callback) {
  available_width_callback_ = available_width_callback;
}

// static
int TabStrip::GetSizeNeededForViews(const std::vector<TabSlotView*>& views) {
  int width = 0;
  for (const TabSlotView* view : views)
    width += view->width();
  if (!views.empty())
    width -= TabStyle::GetTabOverlap() * (views.size() - 1);
  return width;
}

void TabStrip::AddObserver(TabStripObserver* observer) {
  observers_.AddObserver(observer);
}

void TabStrip::RemoveObserver(TabStripObserver* observer) {
  observers_.RemoveObserver(observer);
}

void TabStrip::FrameColorsChanged() {
  for (int i = 0; i < GetTabCount(); ++i)
    tab_at(i)->FrameColorsChanged();
  UpdateContrastRatioValues();
  SchedulePaint();
}

void TabStrip::SetBackgroundOffset(int background_offset) {
  if (background_offset == background_offset_)
    return;
  background_offset_ = background_offset;
  OnPropertyChanged(&background_offset_, views::kPropertyEffectsPaint);
}

bool TabStrip::IsRectInWindowCaption(const gfx::Rect& rect) {
  return tab_container_->IsRectInWindowCaption(rect);
}

bool TabStrip::IsTabStripCloseable() const {
  return !drag_context_->IsDragSessionActive();
}

bool TabStrip::IsTabStripEditable() const {
  return !drag_context_->IsDragSessionActive() &&
         !drag_context_->IsActiveDropTarget();
}

bool TabStrip::IsTabCrashed(int tab_index) const {
  return tab_at(tab_index)->data().IsCrashed();
}

bool TabStrip::TabHasNetworkError(int tab_index) const {
  return tab_at(tab_index)->data().network_state == TabNetworkState::kError;
}

absl::optional<TabAlertState> TabStrip::GetTabAlertState(int tab_index) const {
  return Tab::GetAlertStateToShow(tab_at(tab_index)->data().alert_state);
}

void TabStrip::UpdateLoadingAnimations(const base::TimeDelta& elapsed_time) {
  for (int i = 0; i < GetTabCount(); i++)
    tab_at(i)->StepLoadingAnimation(elapsed_time);
}

void TabStrip::AddTabAt(int model_index, TabRendererData data, bool is_active) {
  const bool pinned = data.pinned;
  Tab* tab = tab_container_->AddTab(
      std::make_unique<Tab>(this), model_index,
      pinned ? TabPinned::kPinned : TabPinned::kUnpinned);
  tab->set_context_menu_controller(&context_menu_controller_);
  tab->AddObserver(this);
  selected_tabs_.IncrementFrom(model_index);

  // Setting data must come after all state from the model has been updated
  // above for the tab. Accessibility, in particular, reacts to data changed
  // callbacks.
  tab->SetData(std::move(data));

  // Don't animate the first tab, it looks weird, and don't animate anything
  // if the containing window isn't visible yet.
  if (GetTabCount() > 1 && GetWidget() && GetWidget()->IsVisible()) {
    StartInsertTabAnimation(model_index);
  } else {
    CompleteAnimationAndLayout();
  }

  tab_container_->UpdateAccessibleTabIndices();

  for (TabStripObserver& observer : observers_)
    observer.OnTabAdded(model_index);

  // Stop dragging when a new tab is added and dragging a window. Doing
  // otherwise results in a confusing state if the user attempts to reattach. We
  // could allow this and make TabDragController update itself during the add,
  // but this comes up infrequently enough that it's not worth the complexity.
  //
  // At the start of AddTabAt() the model and tabs are out sync. Any queries to
  // find a tab given a model index can go off the end of |tabs_|. As such, it
  // is important that we complete the drag *after* adding the tab so that the
  // model and tabstrip are in sync.
  if (!drag_context_->IsMutating() && drag_context_->IsDraggingWindow())
    EndDrag(END_DRAG_COMPLETE);

  Profile* profile = controller()->GetProfile();
  if (profile) {
    if (profile->IsGuestSession())
      base::UmaHistogramCounts100("Tab.Count.Guest", GetTabCount());
    else if (profile->IsIncognitoProfile())
      base::UmaHistogramCounts100("Tab.Count.Incognito", GetTabCount());
  }

  if (new_tab_button_pressed_start_time_.has_value()) {
    base::UmaHistogramTimes(
        "TabStrip.TimeToCreateNewTabFromPress",
        base::TimeTicks::Now() - new_tab_button_pressed_start_time_.value());
    new_tab_button_pressed_start_time_.reset();
  }

  LogTabWidthsForTabScrolling();
}

void TabStrip::MoveTab(int from_model_index,
                       int to_model_index,
                       TabRendererData data) {
  DCHECK_GT(GetTabCount(), 0);

  Tab* moving_tab = tab_at(from_model_index);
  const bool pinned = data.pinned;
  moving_tab->SetData(std::move(data));

  tab_container_->MoveTab(moving_tab, from_model_index, to_model_index);

  selected_tabs_.Move(from_model_index, to_model_index, /*length=*/1);

  tab_container_->layout_helper()->SetTabPinned(
      to_model_index, pinned ? TabPinned::kPinned : TabPinned::kUnpinned);
  StartMoveTabAnimation();

  tab_container_->UpdateAccessibleTabIndices();

  for (TabStripObserver& observer : observers_)
    observer.OnTabMoved(from_model_index, to_model_index);
}

void TabStrip::RemoveTabAt(content::WebContents* contents,
                           int model_index,
                           bool was_active) {
  StartRemoveTabAnimation(model_index, was_active);

  tab_container_->UpdateAccessibleTabIndices();

  UpdateHoverCard(nullptr, HoverCardUpdateType::kTabRemoved);

  for (TabStripObserver& observer : observers_)
    observer.OnTabRemoved(model_index);

  // Stop dragging when a new tab is removed and dragging a window. Doing
  // otherwise results in a confusing state if the user attempts to reattach. We
  // could allow this and make TabDragController update itself during the
  // remove operation, but this comes up infrequently enough that it's not worth
  // the complexity.
  //
  // At the start of RemoveTabAt() the model and tabs are out sync. Any queries
  // to find a tab given a model index can go off the end of |tabs_|. As such,
  // it is important that we complete the drag *after* removing the tab so that
  // the model and tabstrip are in sync.
  if (!drag_context_->IsMutating() && drag_context_->IsDraggingTab(contents))
    EndDrag(END_DRAG_COMPLETE);
}

void TabStrip::ScrollTabToVisible(int model_index) {
  views::ScrollView* scroll_container =
      views::ScrollView::GetScrollViewForContents(this);
  if (!scroll_container) {
    return;
  }

  // If the tab strip won't be scrollable after the current tabstrip animations
  // complete, scroll animation wouldn't be meaningful.
  if (ideal_bounds(GetTabCount() - 1).right() <= GetAvailableWidthForTabStrip())
    return;

  if (tab_scrolling_animation_)
    tab_scrolling_animation_->Stop();

  gfx::Rect visible_content_rect = scroll_container->GetVisibleRect();
  gfx::Rect active_tab_ideal_bounds = ideal_bounds(model_index);

  if ((active_tab_ideal_bounds.x() >= visible_content_rect.x()) &&
      (active_tab_ideal_bounds.right() <= visible_content_rect.right())) {
    return;
  }

  bool scroll_left = active_tab_ideal_bounds.x() < visible_content_rect.x();
  if (scroll_left) {
    // Scroll the left edge of |visible_content_rect| to show the left edge of
    // the tab at |model_index|. We can leave the width entirely up to the
    // ScrollView.
    gfx::Rect start_left_edge(visible_content_rect.x(),
                              visible_content_rect.y(), 0, 0);
    gfx::Rect target_left_edge(active_tab_ideal_bounds.x(),
                               visible_content_rect.y(), 0, 0);
    tab_scrolling_animation_ = std::make_unique<TabScrollingAnimation>(
        this, bounds_animator_.container(),
        bounds_animator_.GetAnimationDuration(), start_left_edge,
        target_left_edge);
    tab_scrolling_animation_->Start();
  } else {
    // Scroll the right edge of |visible_content_rect| to show the right edge
    // of the tab at |model_index|. We can leave the width entirely up to the
    // ScrollView.
    gfx::Rect start_right_edge(visible_content_rect.right(),
                               visible_content_rect.y(), 0, 0);
    gfx::Rect target_right_edge(active_tab_ideal_bounds.right(),
                                visible_content_rect.y(), 0, 0);
    tab_scrolling_animation_ = std::make_unique<TabScrollingAnimation>(
        this, bounds_animator_.container(),
        bounds_animator_.GetAnimationDuration(), start_right_edge,
        target_right_edge);
    tab_scrolling_animation_->Start();
  }
}

void TabStrip::SetTabData(int model_index, TabRendererData data) {
  Tab* tab = tab_at(model_index);
  const bool pinned = data.pinned;
  const bool pinned_state_changed = tab->data().pinned != pinned;
  tab->SetData(std::move(data));

  if (HoverCardIsShowingForTab(tab))
    UpdateHoverCard(tab, HoverCardUpdateType::kTabDataChanged);

  if (pinned_state_changed) {
    tab_container_->layout_helper()->SetTabPinned(
        model_index, pinned ? TabPinned::kPinned : TabPinned::kUnpinned);
    if (GetWidget() && GetWidget()->IsVisible())
      StartPinnedTabAnimation();
    else
      CompleteAnimationAndLayout();
  }
}

void TabStrip::AddTabToGroup(absl::optional<tab_groups::TabGroupId> group,
                             int model_index) {
  tab_at(model_index)->set_group(group);

  // Expand the group if the tab that is getting grouped is the active tab. This
  // can result in the group expanding in a series of actions where the final
  // active tab is not in the group.
  if (model_index == selected_tabs_.active() && group.has_value() &&
      controller()->IsGroupCollapsed(group.value())) {
    controller()->ToggleTabGroupCollapsedState(
        group.value(), ToggleTabGroupCollapsedStateOrigin::kImplicitAction);
  }

  if (group.has_value())
    ExitTabClosingMode();
}

void TabStrip::OnGroupCreated(const tab_groups::TabGroupId& group) {
  auto group_view =
      std::make_unique<TabGroupViews>(tab_container_, this, group);
  tab_container_->layout_helper()->InsertGroupHeader(group,
                                                     group_view->header());
  group_views_[group] = std::move(group_view);
}

void TabStrip::OnGroupEditorOpened(const tab_groups::TabGroupId& group) {
  // The context menu relies on a Browser object which is not provided in
  // TabStripTest.
  if (this->controller()->GetBrowser()) {
    group_views_[group]->header()->ShowContextMenuForViewImpl(
        this, gfx::Point(), ui::MENU_SOURCE_NONE);
  }
}

void TabStrip::OnGroupContentsChanged(const tab_groups::TabGroupId& group) {
  // The group header may be in the wrong place if the tab didn't actually
  // move in terms of model indices.
  OnGroupMoved(group);
  UpdateIdealBounds();
  AnimateToIdealBounds();
}

void TabStrip::OnGroupVisualsChanged(
    const tab_groups::TabGroupId& group,
    const tab_groups::TabGroupVisualData* old_visuals,
    const tab_groups::TabGroupVisualData* new_visuals) {
  group_views_[group]->OnGroupVisualsChanged();
  // The group title may have changed size, so update bounds.
  // First exit tab closing mode, unless this change was a collapse, in which
  // case we want to stay in tab closing mode.
  bool is_collapsing = old_visuals && !old_visuals->is_collapsed() &&
                       new_visuals->is_collapsed();
  if (!is_collapsing)
    ExitTabClosingMode();
  UpdateIdealBounds();
  AnimateToIdealBounds();
}

void TabStrip::ToggleTabGroup(const tab_groups::TabGroupId& group,
                              bool is_collapsing,
                              ToggleTabGroupCollapsedStateOrigin origin) {
  if (is_collapsing && GetWidget()) {
    in_tab_close_ = true;
    if (origin == ToggleTabGroupCollapsedStateOrigin::kMouse) {
      AddMessageLoopObserver();
    } else if (origin == ToggleTabGroupCollapsedStateOrigin::kGesture) {
      StartResizeLayoutTabsFromTouchTimer();
    } else {
      return;
    }

    // The current group header is expanded which is slightly smaller than the
    // size when the header is collapsed. Calculate the size of the header once
    // collapsed for maintaining its position. See
    // TabGroupHeader::CalculateWidth() for more details.
    const int empty_group_title_adjustment =
        GetGroupTitle(group).empty() ? 2 : -2;
    const int title_chip_width =
        group_views_[group]->header()->GetTabSizeInfo().standard_width -
        2 * TabStyle::GetTabOverlap() - empty_group_title_adjustment;
    const int collapsed_header_width =
        title_chip_width + 2 * TabGroupUnderline::GetStrokeInset();
    override_available_width_for_tabs_ =
        ideal_bounds(GetModelCount() - 1).right() -
        group_views_[group]->GetBounds().width() + collapsed_header_width;
  } else {
    ExitTabClosingMode();
  }
}

void TabStrip::OnGroupMoved(const tab_groups::TabGroupId& group) {
  DCHECK(group_views_[group]);

  tab_container_->layout_helper()->UpdateGroupHeaderIndex(group);

  TabGroupHeader* group_header = group_views_[group]->header();
  const int first_tab_model_index =
      controller_->GetFirstTabInGroup(group).value();

  tab_container_->MoveGroupHeader(group_header, first_tab_model_index);
}

void TabStrip::OnGroupClosed(const tab_groups::TabGroupId& group) {
  bounds_animator_.StopAnimatingView(group_header(group));
  tab_container_->layout_helper()->RemoveGroupHeader(group);

  UpdateIdealBounds();
  AnimateToIdealBounds();
  group_views_.erase(group);
}

void TabStrip::ShiftGroupLeft(const tab_groups::TabGroupId& group) {
  ShiftGroupRelative(group, -1);
}

void TabStrip::ShiftGroupRight(const tab_groups::TabGroupId& group) {
  ShiftGroupRelative(group, 1);
}

bool TabStrip::ShouldTabBeVisible(const Tab* tab) const {
  // When the tabstrip is scrollable, it can grow to accommodate any number of
  // tabs, so tabs can never become clipped.
  // N.B. Tabs can still be not-visible because they're in a collapsed group,
  // but that's handled elsewhere.
  // N.B. This is separate from the tab being potentially scrolled offscreen -
  // this solely determines whether the tab should be clipped for the
  // pre-scrolling overflow behavior.
  if (base::FeatureList::IsEnabled(features::kScrollableTabStrip))
    return true;

  // Detached tabs should always be invisible (as they close).
  if (tab->detached())
    return false;

  // If the tab would be clipped by the trailing edge of the strip, even if the
  // tabstrip were resized to its greatest possible width, it shouldn't be
  // visible.
  int right_edge = tab->bounds().right();
  const int tabstrip_right = tab->dragging()
                                 ? drag_context_->GetTabDragAreaWidth()
                                 : GetAvailableWidthForTabStrip();
  if (right_edge > tabstrip_right)
    return false;

  // Non-clipped dragging tabs should always be visible.
  if (tab->dragging())
    return true;

  // Let all non-clipped closing tabs be visible.  These will probably finish
  // closing before the user changes the active tab, so there's little reason to
  // try and make the more complex logic below apply.
  if (tab->closing())
    return true;

  // Now we need to check whether the tab isn't currently clipped, but could
  // become clipped if we changed the active tab, widening either this tab or
  // the tabstrip portion before it.

  // Pinned tabs don't change size when activated, so any tab in the pinned tab
  // region is safe.
  if (tab->data().pinned)
    return true;

  // If the active tab is on or before this tab, we're safe.
  if (controller_->GetActiveIndex() <= GetModelIndexOf(tab))
    return true;

  // We need to check what would happen if the active tab were to move to this
  // tab or before. If animating, we want to use the target bounds in this
  // calculation.
  if (IsAnimating())
    right_edge = bounds_animator_.GetTargetBounds(tab).right();
  return (right_edge + GetActiveTabWidth() - GetInactiveTabWidth()) <=
         tabstrip_right;
}

bool TabStrip::ShouldDrawStrokes() const {
  // If the controller says we can't draw strokes, don't.
  if (!controller_->CanDrawStrokes())
    return false;

  // The tabstrip normally avoids strokes and relies on the active tab
  // contrasting sufficiently with the frame background.  When there isn't
  // enough contrast, fall back to a stroke.  Always compute the contrast ratio
  // against the active frame color, to avoid toggling the stroke on and off as
  // the window activation state changes.
  constexpr float kMinimumContrastRatioForOutlines = 1.3f;
  const SkColor background_color = GetTabBackgroundColor(
      TabActive::kActive, BrowserFrameActiveState::kActive);
  const SkColor frame_color =
      controller_->GetFrameColor(BrowserFrameActiveState::kActive);
  const float contrast_ratio =
      color_utils::GetContrastRatio(background_color, frame_color);
  if (contrast_ratio < kMinimumContrastRatioForOutlines)
    return true;

  // Don't want to have to run a full feature query every time this function is
  // called.
  static const bool tab_outlines_in_low_contrast =
      base::FeatureList::IsEnabled(features::kTabOutlinesInLowContrastThemes);
  if (tab_outlines_in_low_contrast) {
    constexpr float kMinimumAbsoluteContrastForOutlines = 0.2f;
    const float background_luminance =
        color_utils::GetRelativeLuminance(background_color);
    const float frame_luminance =
        color_utils::GetRelativeLuminance(frame_color);
    const float contrast_difference =
        std::fabs(background_luminance - frame_luminance);
    if (contrast_difference < kMinimumAbsoluteContrastForOutlines)
      return true;
  }

  return false;
}

void TabStrip::SetSelection(const ui::ListSelectionModel& new_selection) {
  DCHECK_GE(new_selection.active(), 0)
      << "We should never transition to a state where no tab is active.";
  Tab* const new_active_tab = tab_at(new_selection.active());
  Tab* const old_active_tab =
      selected_tabs_.active() >= 0 ? tab_at(selected_tabs_.active()) : nullptr;

  if (new_active_tab != old_active_tab) {
    if (old_active_tab) {
      old_active_tab->ActiveStateChanged();
      if (old_active_tab->group().has_value())
        UpdateTabGroupVisuals(old_active_tab->group().value());
    }
    if (new_active_tab->group().has_value()) {
      const tab_groups::TabGroupId new_group = new_active_tab->group().value();
      // If the tab that is about to be activated is in a collapsed group,
      // automatically expand the group.
      if (controller()->IsGroupCollapsed(new_group))
        controller()->ToggleTabGroupCollapsedState(
            new_group, ToggleTabGroupCollapsedStateOrigin::kImplicitAction);
      UpdateTabGroupVisuals(new_group);
    }

    new_active_tab->ActiveStateChanged();
    tab_container_->layout_helper()->SetActiveTab(selected_tabs_.active(),
                                                  new_selection.active());
    if (base::FeatureList::IsEnabled(features::kScrollableTabStrip)) {
      ScrollTabToVisible(new_selection.active());
    }
  }

  if (GetActiveTabWidth() == GetInactiveTabWidth()) {
    // When tabs are wide enough, selecting a new tab cannot change the
    // ideal bounds, so only a repaint is necessary.
    SchedulePaint();
  } else if (IsAnimating()) {
    // The selection change will have modified the ideal bounds of the tabs
    // in |selected_tabs_| and |new_selection|.  We need to recompute.
    // Note: This is safe even if we're in the midst of mouse-based tab
    // closure--we won't expand the tabstrip back to the full window
    // width--because PrepareForCloseAt() will have set
    // |override_available_width_for_tabs_| already.
    UpdateIdealBounds();
    AnimateToIdealBounds();
  } else {
    // As in the animating case above, the selection change will have
    // affected the desired bounds of the tabs, but since we're not animating
    // we can just snap to the new bounds.
    CompleteAnimationAndLayout();
  }

  // Use STLSetDifference to get the indices of elements newly selected
  // and no longer selected, since selected_indices() is always sorted.
  ui::ListSelectionModel::SelectedIndices no_longer_selected =
      base::STLSetDifference<ui::ListSelectionModel::SelectedIndices>(
          selected_tabs_.selected_indices(), new_selection.selected_indices());
  ui::ListSelectionModel::SelectedIndices newly_selected =
      base::STLSetDifference<ui::ListSelectionModel::SelectedIndices>(
          new_selection.selected_indices(), selected_tabs_.selected_indices());

  new_active_tab->NotifyAccessibilityEvent(ax::mojom::Event::kSelection, true);
  selected_tabs_ = new_selection;

  UpdateHoverCard(nullptr, HoverCardUpdateType::kSelectionChanged);

  // Notify all tabs whose selected state changed.
  for (auto tab_index :
       base::STLSetUnion<ui::ListSelectionModel::SelectedIndices>(
           no_longer_selected, newly_selected)) {
    tab_at(tab_index)->SelectedStateChanged();
  }
}

void TabStrip::OnWidgetActivationChanged(views::Widget* widget, bool active) {
  if (active && selected_tabs_.active() >= 0) {
    // When the browser window is activated, fire a selection event on the
    // currently active tab, to help enable per-tab modes in assistive
    // technologies.
    tab_at(selected_tabs_.active())
        ->NotifyAccessibilityEvent(ax::mojom::Event::kSelection, true);
  }
  UpdateHoverCard(nullptr, HoverCardUpdateType::kEvent);
}

void TabStrip::OnBoundsAnimatorDone(views::BoundsAnimator* animator) {
  // Send the Container a message to simulate a mouse moved event at the current
  // mouse position. This tickles the Tab the mouse is currently over to show
  // the "hot" state of the close button, or to show the hover card, etc.  Note
  // that this is not required (and indeed may crash!) during a drag session.
  if (!GetDragContext()->IsDragSessionActive()) {
    // The widget can apparently be null during shutdown.
    views::Widget* widget = GetWidget();
    if (widget)
      widget->SynthesizeMouseMoveEvent();
  }
}

void TabStrip::SetTabNeedsAttention(int model_index, bool attention) {
  tab_at(model_index)->SetTabNeedsAttention(attention);
}

int TabStrip::GetModelIndexOf(const TabSlotView* view) const {
  return tab_container_->GetModelIndexOf(view);
}

int TabStrip::GetTabCount() const {
  return tab_container_->GetTabCount();
}

int TabStrip::GetModelCount() const {
  return controller_->GetCount();
}

bool TabStrip::IsValidModelIndex(int model_index) const {
  return controller_->IsValidIndex(model_index);
}

TabDragContext* TabStrip::GetDragContext() {
  return drag_context_.get();
}

int TabStrip::GetPinnedTabCount() const {
  return tab_container_->layout_helper()->GetPinnedTabCount();
}

bool TabStrip::IsAnimating() const {
  return bounds_animator_.IsAnimating();
}

void TabStrip::StopAnimating(bool layout) {
  if (!IsAnimating())
    return;

  bounds_animator_.Cancel();

  if (layout)
    CompleteAnimationAndLayout();
}

absl::optional<int> TabStrip::GetFocusedTabIndex() const {
  for (int i = 0; i < GetTabCount(); ++i) {
    if (tab_at(i)->HasFocus())
      return i;
  }
  return absl::nullopt;
}

views::View* TabStrip::GetTabViewForPromoAnchor(int index_hint) {
  return tab_at(base::clamp(index_hint, 0, GetTabCount() - 1));
}

views::View* TabStrip::GetDefaultFocusableChild() {
  int active = controller_->GetActiveIndex();
  return active != TabStripModel::kNoTab ? tab_at(active) : nullptr;
}

bool TabStrip::WantsToReceiveAllDragEvents() const {
  return TabDragController::IsSystemDragAndDropSessionRunning();
}

const ui::ListSelectionModel& TabStrip::GetSelectionModel() const {
  return controller_->GetSelectionModel();
}

void TabStrip::SelectTab(Tab* tab, const ui::Event& event) {
  int model_index = GetModelIndexOf(tab);

  if (IsValidModelIndex(model_index)) {
    if (!tab->IsActive()) {
      int current_selection = selected_tabs_.active();
      base::UmaHistogramSparse("Tabs.DesktopTabOffsetOfSwitch",
                               current_selection - model_index);
      base::UmaHistogramSparse("Tabs.DesktopTabOffsetFromLeftOfSwitch",
                               model_index);
      base::UmaHistogramSparse("Tabs.DesktopTabOffsetFromRightOfSwitch",
                               GetModelCount() - model_index - 1);
      base::UmaHistogramEnumeration("TabStrip.Tab.Views.ActivationAction",
                                    TabStripModel::TabActivationTypes::kTab);

      if (tab->group().has_value()) {
        base::RecordAction(
            base::UserMetricsAction("TabGroups_SwitchGroupedTab"));
      }
    }

    // Selecting a tab via mouse affects what statistics we collect.
    if (event.type() == ui::ET_MOUSE_PRESSED && !tab->IsActive() &&
        hover_card_controller_) {
      hover_card_controller_->TabSelectedViaMouse(tab);
    }

    controller_->SelectTab(model_index, event);
  }
}

void TabStrip::ExtendSelectionTo(Tab* tab) {
  int model_index = GetModelIndexOf(tab);
  if (IsValidModelIndex(model_index))
    controller_->ExtendSelectionTo(model_index);
}

void TabStrip::ToggleSelected(Tab* tab) {
  int model_index = GetModelIndexOf(tab);
  if (IsValidModelIndex(model_index))
    controller_->ToggleSelected(model_index);
}

void TabStrip::AddSelectionFromAnchorTo(Tab* tab) {
  int model_index = GetModelIndexOf(tab);
  if (IsValidModelIndex(model_index))
    controller_->AddSelectionFromAnchorTo(model_index);
}

void TabStrip::CloseTab(Tab* tab, CloseTabSource source) {
  if (tab->closing()) {
    // If the tab is already closing, close the next tab. We do this so that the
    // user can rapidly close tabs by clicking the close button and not have
    // the animations interfere with that.
    std::vector<Tab*> all_tabs = tab_container_->layout_helper()->GetTabs();
    auto it = std::find(all_tabs.begin(), all_tabs.end(), tab);
    while (it < all_tabs.end() && (*it)->closing()) {
      it++;
    }

    if (it == all_tabs.end())
      return;
    tab = *it;
  }

  CloseTabInternal(GetModelIndexOf(tab), source);
}

void TabStrip::ToggleTabAudioMute(Tab* tab) {
  int model_index = GetModelIndexOf(tab);
  if (IsValidModelIndex(model_index))
    controller_->ToggleTabAudioMute(model_index);
}

void TabStrip::ShiftTabNext(Tab* tab) {
  ShiftTabRelative(tab, 1);
}

void TabStrip::ShiftTabPrevious(Tab* tab) {
  ShiftTabRelative(tab, -1);
}

void TabStrip::MoveTabFirst(Tab* tab) {
  if (tab->closing())
    return;

  const int start_index = GetModelIndexOf(tab);
  if (!IsValidModelIndex(start_index))
    return;

  int target_index = 0;
  if (!controller_->IsTabPinned(start_index)) {
    while (target_index < start_index && controller_->IsTabPinned(target_index))
      ++target_index;
  }

  if (!IsValidModelIndex(target_index))
    return;

  if (target_index != start_index)
    controller_->MoveTab(start_index, target_index);

  // The tab may unintentionally land in the first group in the tab strip, so we
  // remove the group to ensure consistent behavior. Even if the tab is already
  // at the front, it should "move" out of its current group.
  if (tab->group().has_value())
    controller_->RemoveTabFromGroup(target_index);

  GetViewAccessibility().AnnounceText(
      l10n_util::GetStringUTF16(IDS_TAB_AX_ANNOUNCE_MOVED_FIRST));
}

void TabStrip::MoveTabLast(Tab* tab) {
  if (tab->closing())
    return;

  const int start_index = GetModelIndexOf(tab);
  if (!IsValidModelIndex(start_index))
    return;

  int target_index;
  if (controller_->IsTabPinned(start_index)) {
    int temp_index = start_index + 1;
    while (temp_index < GetTabCount() && controller_->IsTabPinned(temp_index))
      ++temp_index;
    target_index = temp_index - 1;
  } else {
    target_index = GetTabCount() - 1;
  }

  if (!IsValidModelIndex(target_index))
    return;

  if (target_index != start_index)
    controller_->MoveTab(start_index, target_index);

  // The tab may unintentionally land in the last group in the tab strip, so we
  // remove the group to ensure consistent behavior. Even if the tab is already
  // at the back, it should "move" out of its current group.
  if (tab->group().has_value())
    controller_->RemoveTabFromGroup(target_index);

  GetViewAccessibility().AnnounceText(
      l10n_util::GetStringUTF16(IDS_TAB_AX_ANNOUNCE_MOVED_LAST));
}

void TabStrip::ShowContextMenuForTab(Tab* tab,
                                     const gfx::Point& p,
                                     ui::MenuSourceType source_type) {
  controller_->ShowContextMenuForTab(tab, p, source_type);
}

bool TabStrip::IsActiveTab(const Tab* tab) const {
  int model_index = GetModelIndexOf(tab);
  return IsValidModelIndex(model_index) &&
         controller_->IsActiveTab(model_index);
}

bool TabStrip::IsTabSelected(const Tab* tab) const {
  int model_index = GetModelIndexOf(tab);
  return IsValidModelIndex(model_index) &&
         controller_->IsTabSelected(model_index);
}

bool TabStrip::IsTabPinned(const Tab* tab) const {
  if (tab->closing())
    return false;

  int model_index = GetModelIndexOf(tab);
  return IsValidModelIndex(model_index) &&
         controller_->IsTabPinned(model_index);
}

bool TabStrip::IsTabFirst(const Tab* tab) const {
  return GetModelIndexOf(tab) == 0;
}

bool TabStrip::IsFocusInTabs() const {
  return GetFocusManager() && Contains(GetFocusManager()->GetFocusedView());
}

void TabStrip::MaybeStartDrag(
    TabSlotView* source,
    const ui::LocatedEvent& event,
    const ui::ListSelectionModel& original_selection) {
  // Don't accidentally start any drag operations during animations if the
  // mouse is down... during an animation tabs are being resized automatically,
  // so the View system can misinterpret this easily if the mouse is down that
  // the user is dragging.
  if (IsAnimating() || controller_->HasAvailableDragActions() == 0)
    return;

  // Check that the source is either a valid tab or a tab group header, which
  // are the only valid drag targets.
  if (!IsValidModelIndex(GetModelIndexOf(source))) {
    DCHECK_EQ(source->GetTabSlotViewType(),
              TabSlotView::ViewType::kTabGroupHeader);
  }

  drag_context_->MaybeStartDrag(source, event, original_selection);
}

void TabStrip::ContinueDrag(views::View* view, const ui::LocatedEvent& event) {
  drag_context_->ContinueDrag(view, event);
}

bool TabStrip::EndDrag(EndDragReason reason) {
  return drag_context_->EndDrag(reason);
}

Tab* TabStrip::GetTabAt(const gfx::Point& point) {
  views::View* view = GetEventHandlerForPoint(point);
  if (!view)
    return nullptr;  // No tab contains the point.

  // Walk up the view hierarchy until we find a tab, or the TabStrip.
  while (view && view != this && view->GetID() != VIEW_ID_TAB)
    view = view->parent();

  return view && view->GetID() == VIEW_ID_TAB ? static_cast<Tab*>(view)
                                              : nullptr;
}

const Tab* TabStrip::GetAdjacentTab(const Tab* tab, int offset) {
  int index = GetModelIndexOf(tab);
  if (index < 0)
    return nullptr;
  index += offset;
  return IsValidModelIndex(index) ? tab_at(index) : nullptr;
}

void TabStrip::OnMouseEventInTab(views::View* source,
                                 const ui::MouseEvent& event) {
  // Record time from cursor entering the tabstrip to first tap on a tab to
  // switch.
  if (mouse_entered_tabstrip_time_.has_value() &&
      event.type() == ui::ET_MOUSE_PRESSED && views::IsViewClass<Tab>(source)) {
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "TabStrip.TimeToSwitch",
        base::TimeTicks::Now() - mouse_entered_tabstrip_time_.value());
    mouse_entered_tabstrip_time_.reset();
  }
}

void TabStrip::UpdateHoverCard(Tab* tab, HoverCardUpdateType update_type) {
  // Some operations (including e.g. starting a drag) can cause the tab focus
  // to change at the same time as the tabstrip is starting to animate; the
  // hover card should not be visible at this time.
  // See crbug.com/1220840 for an example case.
  if (bounds_animator_.IsAnimating()) {
    tab = nullptr;
    update_type = HoverCardUpdateType::kAnimating;
  }

  if (!hover_card_controller_)
    return;

  hover_card_controller_->UpdateHoverCard(tab, update_type);
}

bool TabStrip::ShowDomainInHoverCards() const {
  const auto* app_controller = controller_->GetBrowser()->app_controller();
  return !app_controller || !app_controller->system_app();
}

bool TabStrip::HoverCardIsShowingForTab(Tab* tab) {
  return hover_card_controller_ &&
         hover_card_controller_->IsHoverCardShowingForTab(tab);
}

int TabStrip::GetBackgroundOffset() const {
  return background_offset_;
}

int TabStrip::GetStrokeThickness() const {
  return ShouldDrawStrokes() ? 1 : 0;
}

bool TabStrip::CanPaintThrobberToLayer() const {
  // Disable layer-painting of throbbers if dragging or if any tab animation is
  // in progress. Also disable in fullscreen: when "immersive" the tab strip
  // could be sliding in or out; for other modes, there's no tab strip.
  const bool dragging = drag_context_->IsDragStarted();
  const views::Widget* widget = GetWidget();
  return widget && !dragging && !IsAnimating() && !widget->IsFullscreen();
}

bool TabStrip::HasVisibleBackgroundTabShapes() const {
  return controller_->HasVisibleBackgroundTabShapes();
}

bool TabStrip::ShouldPaintAsActiveFrame() const {
  return controller_->ShouldPaintAsActiveFrame();
}

SkColor TabStrip::GetToolbarTopSeparatorColor() const {
  return controller_->GetToolbarTopSeparatorColor();
}

SkColor TabStrip::GetTabSeparatorColor() const {
  return separator_color_;
}

SkColor TabStrip::GetTabBackgroundColor(
    TabActive active,
    BrowserFrameActiveState active_state) const {
  const ui::ThemeProvider* tp = GetThemeProvider();
  if (!tp)
    return SK_ColorBLACK;

  constexpr int kColorIds[2][2] = {
      {ThemeProperties::COLOR_TAB_BACKGROUND_INACTIVE_FRAME_INACTIVE,
       ThemeProperties::COLOR_TAB_BACKGROUND_INACTIVE_FRAME_ACTIVE},
      {ThemeProperties::COLOR_TAB_BACKGROUND_ACTIVE_FRAME_INACTIVE,
       ThemeProperties::COLOR_TAB_BACKGROUND_ACTIVE_FRAME_ACTIVE}};

  using State = BrowserFrameActiveState;
  const bool tab_active = active == TabActive::kActive;
  const bool frame_active =
      (active_state == State::kActive) ||
      ((active_state == State::kUseCurrent) && ShouldPaintAsActiveFrame());
  return tp->GetColor(kColorIds[tab_active][frame_active]);
}

SkColor TabStrip::GetTabForegroundColor(TabActive active,
                                        SkColor background_color) const {
  const ui::ColorProvider* cp = GetColorProvider();
  if (!cp)
    return SK_ColorBLACK;

  constexpr ChromeColorIds kColorIds[2][2] = {
      {kColorTabForegroundInactiveFrameInactive,
       kColorTabForegroundInactiveFrameActive},
      {kColorTabForegroundActiveFrameInactive,
       kColorTabForegroundActiveFrameActive}};

  const bool tab_active = active == TabActive::kActive;
  const bool frame_active = ShouldPaintAsActiveFrame();
  return cp->GetColor(kColorIds[tab_active][frame_active]);
}

// Returns the accessible tab name for the tab.
std::u16string TabStrip::GetAccessibleTabName(const Tab* tab) const {
  const int model_index = GetModelIndexOf(tab);
  return IsValidModelIndex(model_index) ? controller_->GetAccessibleTabName(tab)
                                        : std::u16string();
}

absl::optional<int> TabStrip::GetCustomBackgroundId(
    BrowserFrameActiveState active_state) const {
  if (!TitlebarBackgroundIsTransparent())
    return controller_->GetCustomBackgroundId(active_state);

  constexpr int kBackgroundIdGlass = IDR_THEME_TAB_BACKGROUND_V;
  return GetThemeProvider()->HasCustomImage(kBackgroundIdGlass)
             ? absl::make_optional(kBackgroundIdGlass)
             : absl::nullopt;
}

gfx::Rect TabStrip::GetTabAnimationTargetBounds(const Tab* tab) {
  return bounds_animator_.GetTargetBounds(tab);
}

void TabStrip::MouseMovedOutOfHost() {
  ResizeLayoutTabs();
}

float TabStrip::GetHoverOpacityForTab(float range_parameter) const {
  return gfx::Tween::FloatValueBetween(range_parameter, hover_opacity_min_,
                                       hover_opacity_max_);
}

float TabStrip::GetHoverOpacityForRadialHighlight() const {
  return radial_highlight_opacity_;
}

std::u16string TabStrip::GetGroupTitle(
    const tab_groups::TabGroupId& group) const {
  return controller_->GetGroupTitle(group);
}

tab_groups::TabGroupColorId TabStrip::GetGroupColorId(
    const tab_groups::TabGroupId& group) const {
  return controller_->GetGroupColorId(group);
}

SkColor TabStrip::GetPaintedGroupColor(
    const tab_groups::TabGroupColorId& color_id) const {
  return GetThemeProvider()->GetColor(
      GetTabGroupTabStripColorId(color_id, ShouldPaintAsActiveFrame()));
}

///////////////////////////////////////////////////////////////////////////////
// TabStrip, views::AccessiblePaneView overrides:

void TabStrip::Layout() {
  if (base::FeatureList::IsEnabled(features::kScrollableTabStrip)) {
    // With tab scrolling, the tabstrip is solely responsible for its own
    // width.
    // It should never be larger than its preferred width.
    const int max_width = CalculatePreferredSize().width();
    // It should never be smaller than its minimum width.
    const int min_width = GetMinimumSize().width();
    // If it can, it should fit within the tab strip region.
    const int available_width = available_width_callback_.Run();
    // It should be as wide as possible subject to the above constraints.
    const int width = std::min(max_width, std::max(min_width, available_width));
    SetBounds(0, 0, width, GetLayoutConstant(TAB_HEIGHT));
    SetTabSlotVisibility();
  }

  tab_container_->SetBounds(0, 0, width(), height());

  if (IsAnimating()) {
    // Hide tabs that have animated at least partially out of the clip region.
    SetTabSlotVisibility();
    return;
  }

  // Only do a layout if our size or the available width changed.
  const int available_width = GetAvailableWidthForTabStrip();
  if (last_layout_size_ == size() && last_available_width_ == available_width)
    return;
  if (drag_context_->IsDragSessionActive())
    return;
  CompleteAnimationAndLayout();
}

void TabStrip::PaintChildren(const views::PaintInfo& paint_info) {
  // Groups that are being dragged by their header, or that contain the dragged
  // tabs, need an adjusted z-value. Find that group, if it exists.
  absl::optional<tab_groups::TabGroupId> dragging_tabs_current_group =
      absl::nullopt;

  TabDragController* drag_controller = drag_context_->GetDragController();
  if (drag_controller) {
    dragging_tabs_current_group = drag_controller->group();
  }

  if (!dragging_tabs_current_group.has_value()) {
    for (const Tab* tab : tab_container_->layout_helper()->GetTabs()) {
      if (tab->dragging()) {
        dragging_tabs_current_group = tab->group();
        break;
      }
    }
  }

  absl::optional<const TabGroupUnderline*>
      dragging_tabs_current_group_underline =
          dragging_tabs_current_group.has_value()
              ? absl::optional<const TabGroupUnderline*>(
                    group_views_[dragging_tabs_current_group.value()]
                        ->underline())
              : absl::nullopt;

  std::vector<ZOrderableTabStripElement> orderable_children;
  for (views::View* child : tab_container_->children())
    orderable_children.emplace_back(child,
                                    dragging_tabs_current_group_underline);

  // Sort in non-descending order. Stable sort breaks z-value ties by index (for
  // tabs).
  std::stable_sort(orderable_children.begin(), orderable_children.end());

  for (const ZOrderableTabStripElement& child : orderable_children)
    child.view()->Paint(paint_info);
}

gfx::Size TabStrip::GetMinimumSize() const {
  return tab_container_->GetMinimumSize();
}

gfx::Size TabStrip::CalculatePreferredSize() const {
  int preferred_width;
  // The tabstrip needs to always exactly fit the bounds of the tabs so that
  // NTB can be laid out just to the right of the rightmost tab. When the tabs
  // aren't at their ideal bounds (i.e. during animation or a drag), we need to
  // size ourselves to exactly fit wherever the tabs *currently* are.
  if (IsAnimating() || drag_context_->IsDragSessionActive()) {
    // The visual order of the tabs can be out of sync with the logical order,
    // so we have to check all of them to find the visually trailing-most one.
    int max_x = 0;
    for (auto* child : tab_container_->children()) {
      max_x = std::max(max_x, child->bounds().right());
    }
    // The tabs span from 0 to |max_x|, so |max_x| is the current width
    // occupied by tabs. We report the current width as our preferred width so
    // that the tab strip is sized to exactly fit the current position of the
    // tabs.
    preferred_width = max_x;
  } else {
    preferred_width =
        override_available_width_for_tabs_
            ? override_available_width_for_tabs_.value()
            : tab_container_->layout_helper()->CalculatePreferredWidth();
  }

  return gfx::Size(preferred_width, GetLayoutConstant(TAB_HEIGHT));
}

bool TabStrip::CanDrop(const OSExchangeData& data) {
  return WantsToReceiveAllDragEvents();
}

bool TabStrip::GetDropFormats(int* formats,
                              std::set<ui::ClipboardFormatType>* format_types) {
  if (!WantsToReceiveAllDragEvents())
    return false;

  format_types->insert(
      ui::ClipboardFormatType::GetType(ui::kMimeTypeWindowDrag));
  return true;
}

void TabStrip::OnDragEntered(const ui::DropTargetEvent& event) {
  DCHECK(WantsToReceiveAllDragEvents());
  TabDragController::OnSystemDragAndDropUpdated(event);
}

int TabStrip::OnDragUpdated(const ui::DropTargetEvent& event) {
  DCHECK(WantsToReceiveAllDragEvents());
  TabDragController::OnSystemDragAndDropUpdated(event);
  return ui::DragDropTypes::DRAG_MOVE;
}
void TabStrip::OnDragExited() {
  DCHECK(WantsToReceiveAllDragEvents());
  TabDragController::OnSystemDragAndDropExited();
}

BrowserRootView::DropIndex TabStrip::GetDropIndex(
    const ui::DropTargetEvent& event) {
  // Force animations to stop, otherwise it makes the index calculation tricky.
  StopAnimating(true);

  // If the UI layout is right-to-left, we need to mirror the mouse
  // coordinates since we calculate the drop index based on the
  // original (and therefore non-mirrored) positions of the tabs.
  const int x = GetMirroredXInView(event.x());

  std::vector<TabSlotView*> views =
      tab_container_->layout_helper()->GetTabSlotViews();

  // Loop until we find a tab or group header that intersects |event|'s
  // location.
  for (TabSlotView* view : views) {
    const int max_x = view->x() + view->width();
    if (x >= max_x)
      continue;

    if (view->GetTabSlotViewType() == TabSlotView::ViewType::kTab) {
      Tab* const tab = static_cast<Tab*>(view);
      // Closing tabs should be skipped.
      if (tab->closing())
        continue;

      // GetModelIndexOf is an O(n) operation. Since we will definitely
      // return from the loop at this point, it is only called once.
      // Hence the loop is still O(n). Calling this every loop iteration
      // must be avoided since it will become O(n^2).
      const int model_index = GetModelIndexOf(tab);
      const bool first_in_group =
          tab->group().has_value() &&
          model_index == controller_->GetFirstTabInGroup(tab->group().value());

      // When hovering over the left or right quarter of a tab, the drop
      // indicator will point between tabs.
      const int hot_width = tab->width() / 4;

      if (x >= (max_x - hot_width))
        return {model_index + 1, true /* drop_before */,
                false /* drop_in_group */};
      else if (x < tab->x() + hot_width)
        return {model_index, true /* drop_before */, first_in_group};
      else
        return {model_index, false /* drop_before */,
                false /* drop_in_group */};
    } else {
      TabGroupHeader* const group_header = static_cast<TabGroupHeader*>(view);
      const int first_tab_index =
          controller_->GetFirstTabInGroup(group_header->group().value())
              .value();

      if (x < max_x - group_header->width() / 2)
        return {first_tab_index, true /* drop_before */,
                false /* drop_in_group */};
      else
        return {first_tab_index, true /* drop_before */,
                true /* drop_in_group */};
    }
  }

  // The drop isn't over a tab, add it to the end.
  return {GetTabCount(), true, false};
}

views::View* TabStrip::GetViewForDrop() {
  return this;
}

void TabStrip::HandleDragUpdate(
    const absl::optional<BrowserRootView::DropIndex>& index) {
  SetDropArrow(index);
}

void TabStrip::HandleDragExited() {
  SetDropArrow({});
}

///////////////////////////////////////////////////////////////////////////////
// TabStrip, private:

void TabStrip::Init() {
  SetID(VIEW_ID_TAB_STRIP);
  // So we only get enter/exit messages when the mouse enters/exits the whole
  // tabstrip, even if it is entering/exiting a specific Tab, too.
  SetNotifyEnterExitOnChild(true);

  if (g_drop_indicator_width == 0) {
    // Direction doesn't matter, both images are the same size.
    gfx::ImageSkia* drop_image = GetDropArrowImage(true);
    g_drop_indicator_width = drop_image->width();
    g_drop_indicator_height = drop_image->height();
  }

  UpdateContrastRatioValues();

  if (!gfx::Animation::ShouldRenderRichAnimation())
    bounds_animator_.SetAnimationDuration(base::TimeDelta());

  bounds_animator_.AddObserver(this);
}

std::map<tab_groups::TabGroupId, TabGroupHeader*> TabStrip::GetGroupHeaders() {
  std::map<tab_groups::TabGroupId, TabGroupHeader*> group_headers;
  for (const auto& group_view_pair : group_views_) {
    group_headers.insert(std::make_pair(group_view_pair.first,
                                        group_view_pair.second->header()));
  }
  return group_headers;
}

void TabStrip::NewTabButtonPressed(const ui::Event& event) {
  new_tab_button_pressed_start_time_ = base::TimeTicks::Now();

  base::RecordAction(base::UserMetricsAction("NewTab_Button"));
  UMA_HISTOGRAM_ENUMERATION("Tab.NewTab", TabStripModel::NEW_TAB_BUTTON,
                            TabStripModel::NEW_TAB_ENUM_COUNT);
  if (event.IsMouseEvent()) {
    // Prevent the hover card from popping back in immediately. This forces a
    // normal fade-in.
    if (hover_card_controller_)
      hover_card_controller_->PreventImmediateReshow();

    const ui::MouseEvent& mouse = static_cast<const ui::MouseEvent&>(event);
    if (mouse.IsOnlyMiddleMouseButton()) {
      if (ui::Clipboard::IsSupportedClipboardBuffer(
              ui::ClipboardBuffer::kSelection)) {
        ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
        CHECK(clipboard);
        std::u16string clipboard_text;
        clipboard->ReadText(ui::ClipboardBuffer::kSelection,
                            /* data_dst = */ nullptr, &clipboard_text);
        if (!clipboard_text.empty())
          controller_->CreateNewTabWithLocation(clipboard_text);
      }
      return;
    }
  }

  controller_->CreateNewTab();
  if (event.type() == ui::ET_GESTURE_TAP)
    TouchUMA::RecordGestureAction(TouchUMA::kGestureNewTabTap);
}

void TabStrip::StartInsertTabAnimation(int model_index) {
  PrepareForAnimation();

  ExitTabClosingMode();

  gfx::Rect bounds = tab_at(model_index)->bounds();
  bounds.set_height(GetLayoutConstant(TAB_HEIGHT));

  // Adjust the starting bounds of the new tab.
  const int tab_overlap = TabStyle::GetTabOverlap();
  if (model_index > 0) {
    // If we have a tab to our left, start at its right edge.
    bounds.set_x(tab_at(model_index - 1)->bounds().right() - tab_overlap);
  } else if (model_index + 1 < GetTabCount()) {
    // Otherwise, if we have a tab to our right, start at its left edge.
    bounds.set_x(tab_at(model_index + 1)->bounds().x());
  } else {
    NOTREACHED() << "First tab inserted into the tabstrip should not animate.";
  }

  // Start at the width of the overlap in order to animate at the same speed
  // the surrounding tabs are moving, since at this width the subsequent tab
  // is naturally positioned at the same X coordinate.
  bounds.set_width(tab_overlap);
  tab_at(model_index)->SetBoundsRect(bounds);

  // Animate in to the full width.
  UpdateIdealBounds();
  AnimateToIdealBounds();
}

void TabStrip::StartRemoveTabAnimation(int model_index, bool was_active) {
  const int model_count = GetModelCount();
  const int tab_overlap = TabStyle::GetTabOverlap();
  if (in_tab_close_ && model_count > 0 && model_index != model_count) {
    // The user closed a tab other than the last tab. Set
    // override_available_width_for_tabs_ so that as the user closes tabs with
    // the mouse a tab continues to fall under the mouse.
    int next_active_index = controller_->GetActiveIndex();
    DCHECK(IsValidModelIndex(next_active_index));
    if (model_index <= next_active_index) {
      // At this point, model's internal state has already been updated.
      // |contents| has been detached from model and the active index has been
      // updated. But the tab for |contents| isn't removed yet. Thus, we need to
      // fix up next_active_index based on it.
      next_active_index++;
    }
    Tab* next_active_tab = tab_at(next_active_index);
    Tab* tab_being_removed = tab_at(model_index);

    int size_delta = tab_being_removed->width();
    if (!tab_being_removed->data().pinned && was_active &&
        GetActiveTabWidth() > GetInactiveTabWidth()) {
      // When removing an active, non-pinned tab, an inactive tab will be made
      // active and thus given the active width. Thus the width being removed
      // from the strip is really the current width of whichever inactive tab
      // will be made active.
      size_delta = next_active_tab->width();
    }

    override_available_width_for_tabs_ =
        ideal_bounds(model_count).right() - size_delta + tab_overlap;
  }

  PrepareForAnimation();

  Tab* tab = tab_at(model_index);
  tab->SetClosing(true);

  RemoveTabFromViewModel(model_index);

  UpdateIdealBounds();
  AnimateToIdealBounds();

  if (in_tab_close_ && model_count > 0 &&
      override_available_width_for_tabs_ >
          ideal_bounds(model_count - 1).right()) {
    // Tab closing mode is no longer constraining tab widths - they're at full
    // size. Exit tab closing mode so that it doesn't artificially inflate the
    // tabstrip's bounds.
    ExitTabClosingMode();
  }

  // TODO(pkasting): When closing multiple tabs, we get repeated RemoveTabAt()
  // calls, each of which closes a new tab and thus generates different ideal
  // bounds.  We should update the animations of any other tabs that are
  // currently being closed to reflect the new ideal bounds, or else change from
  // removing one tab at a time to animating the removal of all tabs at once.

  // Compute the target bounds for animating this tab closed.  The tab's left
  // edge should stay joined to the right edge of the previous tab, if any.
  gfx::Rect tab_bounds = tab->bounds();
  tab_bounds.set_x((model_index > 0)
                       ? (ideal_bounds(model_index - 1).right() - tab_overlap)
                       : 0);

  // The tab should animate to the width of the overlap in order to close at the
  // same speed the surrounding tabs are moving, since at this width the
  // subsequent tab is naturally positioned at the same X coordinate.
  tab_bounds.set_width(tab_overlap);

  // Animate the tab closed.
  bounds_animator_.AnimateViewTo(
      tab, tab_bounds,
      std::make_unique<RemoveTabDelegate>(
          this, tab,
          base::BindRepeating(&TabStrip::OnTabSlotAnimationProgressed,
                              base::Unretained(this))));
}

void TabStrip::StartMoveTabAnimation() {
  PrepareForAnimation();
  UpdateIdealBounds();
  AnimateToIdealBounds();
}

void TabStrip::AnimateToIdealBounds() {
  UpdateHoverCard(nullptr, HoverCardUpdateType::kAnimating);

  for (int i = 0; i < GetTabCount(); ++i) {
    // If the tab is being dragged manually, skip it.
    Tab* tab = tab_at(i);
    if (tab->dragging() && !bounds_animator_.IsAnimating(tab))
      continue;

    // Also skip tabs already being animated to the same ideal bounds.  Calling
    // AnimateViewTo() again restarts the animation, which changes the timing of
    // how the tab animates, leading to hitches.
    const gfx::Rect& target_bounds = ideal_bounds(i);
    if (bounds_animator_.GetTargetBounds(tab) == target_bounds)
      continue;

    // Set an animation delegate for the tab so it will clip appropriately.
    // Don't do this if dragging() is true.  In this case the tab was
    // previously being dragged and is now animating back to its ideal
    // bounds; it already has an associated ResetDraggingStateDelegate that
    // will reset this dragging state. Replacing this delegate would mean
    // this code would also need to reset the dragging state immediately,
    // and that could allow the new tab button to be drawn atop this tab.
    if (bounds_animator_.IsAnimating(tab) && tab->dragging()) {
      bounds_animator_.SetTargetBounds(tab, target_bounds);
    } else {
      bounds_animator_.AnimateViewTo(
          tab, target_bounds,
          std::make_unique<TabSlotAnimationDelegate>(
              this, tab,
              base::BindRepeating(&TabStrip::OnTabSlotAnimationProgressed,
                                  base::Unretained(this))));
    }
  }

  for (const auto& header_pair : group_views_) {
    TabGroupHeader* const header = header_pair.second->header();

    // If the header is being dragged manually, skip it.
    if (header->dragging() && !bounds_animator_.IsAnimating(header))
      continue;

    bounds_animator_.AnimateViewTo(
        header,
        tab_container_->layout_helper()->group_header_ideal_bounds().at(
            header_pair.first),
        std::make_unique<TabSlotAnimationDelegate>(
            this, header,
            base::BindRepeating(&TabStrip::OnTabSlotAnimationProgressed,
                                base::Unretained(this))));
  }

  // Because the preferred size of the tabstrip depends on the IsAnimating()
  // condition, but starting an animation  doesn't necessarily invalidate the
  // existing preferred size and layout (which may now be incorrect), we need to
  // signal this explicitly.
  PreferredSizeChanged();
}

void TabStrip::SnapToIdealBounds() {
  for (int i = 0; i < GetTabCount(); ++i)
    tab_at(i)->SetBoundsRect(ideal_bounds(i));

  for (const auto& header_pair : group_views_) {
    header_pair.second->header()->SetBoundsRect(
        tab_container_->layout_helper()->group_header_ideal_bounds().at(
            header_pair.first));
    header_pair.second->UpdateBounds();
  }

  PreferredSizeChanged();
}

void TabStrip::ExitTabClosingMode() {
  in_tab_close_ = false;
  override_available_width_for_tabs_.reset();
  tab_container_->layout_helper()->ExitTabClosingMode();
}

bool TabStrip::ShouldHighlightCloseButtonAfterRemove() {
  return in_tab_close_;
}

bool TabStrip::TitlebarBackgroundIsTransparent() const {
#if BUILDFLAG(IS_WIN)
  // Windows 8+ uses transparent window contents (because the titlebar area is
  // drawn by the system and not Chrome), but the actual titlebar is opaque.
  if (base::win::GetVersion() >= base::win::Version::WIN8)
    return false;
#endif
  return GetWidget()->ShouldWindowContentsBeTransparent();
}

void TabStrip::CompleteAnimationAndLayout() {
  last_available_width_ = GetAvailableWidthForTabStrip();
  last_layout_size_ = size();

  bounds_animator_.Cancel();
  if (tab_scrolling_animation_)
    tab_scrolling_animation_->SetCurrentValue(1);

  UpdateIdealBounds();
  SnapToIdealBounds();

  SetTabSlotVisibility();
  SchedulePaint();
}

void TabStrip::SetTabSlotVisibility() {
  bool last_tab_visible = false;
  absl::optional<tab_groups::TabGroupId> last_tab_group = absl::nullopt;
  std::vector<Tab*> tabs = tab_container_->layout_helper()->GetTabs();
  for (Tab* tab : base::Reversed(tabs)) {
    absl::optional<tab_groups::TabGroupId> current_group = tab->group();
    if (current_group != last_tab_group && last_tab_group.has_value()) {
      TabGroupViews* group_view = group_views_.at(last_tab_group.value()).get();
      group_view->header()->SetVisible(last_tab_visible);
      group_view->underline()->SetVisible(last_tab_visible);
    }
    last_tab_visible = ShouldTabBeVisible(tab);
    last_tab_group = tab->closing() ? absl::nullopt : current_group;

    // Collapsed tabs disappear once they've reached their minimum size. This
    // is different than very small non-collapsed tabs, because in that case
    // the tab (and its favicon) must still be visible.
    bool is_collapsed =
        (current_group.has_value() &&
         controller()->IsGroupCollapsed(current_group.value()) &&
         tab->bounds().width() <= TabStyle::GetTabOverlap());
    tab->SetVisible(is_collapsed ? false : last_tab_visible);
  }
}

int TabStrip::GetActiveTabWidth() const {
  return tab_container_->layout_helper()->active_tab_width();
}

int TabStrip::GetInactiveTabWidth() const {
  return tab_container_->layout_helper()->inactive_tab_width();
}

const Tab* TabStrip::GetLastVisibleTab() const {
  for (int i = GetTabCount() - 1; i >= 0; --i) {
    const Tab* tab = tab_at(i);

    // The tab is marked not visible in a collapsed group, but is "visible" in
    // the tabstrip if the header is visible.
    if (tab->GetVisible() ||
        (tab->group().has_value() &&
         group_header(tab->group().value())->GetVisible())) {
      return tab;
    }
  }
  // While in normal use the tabstrip should always be wide enough to have at
  // least one visible tab, it can be zero-width in tests, meaning we get here.
  return nullptr;
}

void TabStrip::CloseTabInternal(int model_index, CloseTabSource source) {
  if (!IsValidModelIndex(model_index))
    return;

  // If we're not allowed to close this tab for whatever reason, we should not
  // proceed.
  if (!controller_->BeforeCloseTab(model_index, source))
    return;

  if (!in_tab_close_ && IsAnimating()) {
    // Cancel any current animations. We do this as remove uses the current
    // ideal bounds and we need to know ideal bounds is in a good state.
    StopAnimating(true);
  }

  if (GetWidget()) {
    in_tab_close_ = true;
    resize_layout_timer_.Stop();
    if (source == CLOSE_TAB_FROM_TOUCH)
      StartResizeLayoutTabsFromTouchTimer();
    else
      AddMessageLoopObserver();
  }

  UpdateHoverCard(nullptr, HoverCardUpdateType::kTabRemoved);
  if (tab_at(model_index)->group().has_value())
    base::RecordAction(base::UserMetricsAction("CloseGroupedTab"));
  controller_->CloseTab(model_index);
}

void TabStrip::RemoveTabFromViewModel(int index) {
  Tab* closing_tab = tab_at(index);
  bool closing_tab_was_active = closing_tab->IsActive();

  UpdateHoverCard(nullptr, HoverCardUpdateType::kTabRemoved);

  // We still need to keep the tab alive until the remove tab animation
  // completes. Defer destroying it until then.
  tab_container_->RemoveTabFromViewModel(index);
  selected_tabs_.DecrementFrom(index);

  if (closing_tab_was_active)
    closing_tab->ActiveStateChanged();
}

void TabStrip::OnTabCloseAnimationCompleted(Tab* tab) {
  DCHECK(tab->closing());

  std::unique_ptr<Tab> deleter(tab);
  tab_container_->layout_helper()->OnTabDestroyed(tab);
}

void TabStrip::StoppedDraggingView(TabSlotView* view, bool* is_first_view) {
  if (view &&
      view->GetTabSlotViewType() == TabSlotView::ViewType::kTabGroupHeader) {
    view->set_dragging(false);
    // Disable the group highlight now that the drag is ended.
    group_views_[view->group().value()]->highlight()->SetVisible(false);
    return;
  }

  int tab_data_index = GetModelIndexOf(view);
  if (tab_data_index == -1) {
    // The tab was removed before the drag completed. Don't do anything.
    return;
  }

  if (*is_first_view) {
    *is_first_view = false;
    PrepareForAnimation();

    // Animate the view back to its correct position.
    UpdateIdealBounds();
    AnimateToIdealBounds();
  }

  // Install a delegate to reset the dragging state when done. We have to leave
  // dragging true for the tab otherwise it'll draw beneath the new tab button.
  bounds_animator_.AnimateViewTo(
      view, ideal_bounds(tab_data_index),
      std::make_unique<ResetDraggingStateDelegate>(
          this, static_cast<Tab*>(view),
          base::BindRepeating(&TabStrip::OnTabSlotAnimationProgressed,
                              base::Unretained(this))));
}

void TabStrip::UpdateContrastRatioValues() {
  // There may be no controller in unit tests, and the call to
  // GetTabBackgroundColor() below requires one, so bail early if it is absent.
  if (!controller_)
    return;

  const SkColor inactive_bg = GetTabBackgroundColor(
      TabActive::kInactive, BrowserFrameActiveState::kUseCurrent);
  const auto get_blend = [inactive_bg](SkColor target, float contrast) {
    return color_utils::BlendForMinContrast(inactive_bg, inactive_bg, target,
                                            contrast);
  };

  const SkColor active_bg = GetTabBackgroundColor(
      TabActive::kActive, BrowserFrameActiveState::kUseCurrent);
  const auto get_hover_opacity = [active_bg, &get_blend](float contrast) {
    return get_blend(active_bg, contrast).alpha / 255.0f;
  };

  // The contrast ratio for the hover effect on standard-width tabs.
  // In the default color scheme, this corresponds to a hover opacity of 0.4.
  constexpr float kStandardWidthContrast = 1.11f;
  hover_opacity_min_ = get_hover_opacity(kStandardWidthContrast);

  // The contrast ratio for the hover effect on min-width tabs.
  // In the default color scheme, this corresponds to a hover opacity of 0.65.
  constexpr float kMinWidthContrast = 1.19f;
  hover_opacity_max_ = get_hover_opacity(kMinWidthContrast);

  // The contrast ratio for the radial gradient effect on hovered tabs.
  // In the default color scheme, this corresponds to a hover opacity of 0.45.
  constexpr float kRadialGradientContrast = 1.13728f;
  radial_highlight_opacity_ = get_hover_opacity(kRadialGradientContrast);

  const SkColor inactive_fg =
      GetTabForegroundColor(TabActive::kInactive, inactive_bg);
  // The contrast ratio for the separator between inactive tabs.
  constexpr float kTabSeparatorContrast = 2.5f;
  separator_color_ = get_blend(inactive_fg, kTabSeparatorContrast).color;
}

void TabStrip::ShiftTabRelative(Tab* tab, int offset) {
  DCHECK_EQ(1, std::abs(offset));
  const int start_index = GetModelIndexOf(tab);
  int target_index = start_index + offset;

  if (!IsValidModelIndex(start_index))
    return;

  if (tab->closing())
    return;

  const auto old_group = tab->group();
  if (!IsValidModelIndex(target_index) ||
      controller_->IsTabPinned(start_index) !=
          controller_->IsTabPinned(target_index)) {
    // Even if we've reached the boundary of where the tab could go, it may
    // still be able to "move" out of its current group.
    if (old_group.has_value()) {
      AnnounceTabRemovedFromGroup(old_group.value());
      controller_->RemoveTabFromGroup(start_index);
    }
    return;
  }

  // If the tab is at a group boundary and the group is expanded, instead of
  // actually moving the tab just change its group membership.
  absl::optional<tab_groups::TabGroupId> target_group =
      tab_at(target_index)->group();
  if (old_group != target_group) {
    if (old_group.has_value()) {
      AnnounceTabRemovedFromGroup(old_group.value());
      controller_->RemoveTabFromGroup(start_index);
      return;
    } else if (target_group.has_value()) {
      // If the tab is at a group boundary and the group is collapsed, treat the
      // collapsed group as a tab and find the next available slot for the tab
      // to move to.
      if (controller_->IsGroupCollapsed(target_group.value())) {
        int candidate_index = target_index + offset;
        while (IsValidModelIndex(candidate_index) &&
               tab_at(candidate_index)->group() == target_group) {
          candidate_index += offset;
        }
        if (IsValidModelIndex(candidate_index)) {
          target_index = candidate_index - offset;
        } else {
          target_index = offset < 0 ? 0 : GetModelCount() - 1;
        }
      } else {
        // Read before adding the tab to the group so that the group description
        // isn't the tab we just added.
        AnnounceTabAddedToGroup(target_group.value());
        controller_->AddTabToGroup(start_index, target_group.value());
        return;
      }
    }
  }

  controller_->MoveTab(start_index, target_index);
  GetViewAccessibility().AnnounceText(l10n_util::GetStringUTF16(
      ((offset > 0) ^ base::i18n::IsRTL()) ? IDS_TAB_AX_ANNOUNCE_MOVED_RIGHT
                                           : IDS_TAB_AX_ANNOUNCE_MOVED_LEFT));
}

void TabStrip::ShiftGroupRelative(const tab_groups::TabGroupId& group,
                                  int offset) {
  DCHECK_EQ(1, std::abs(offset));
  gfx::Range tabs_in_group = controller_->ListTabsInGroup(group);

  const int start_index = tabs_in_group.start();
  int target_index = start_index + offset;

  if (offset > 0)
    target_index += tabs_in_group.length() - 1;

  if (!IsValidModelIndex(start_index) || !IsValidModelIndex(target_index))
    return;

  // Avoid moving into the middle of another group by accounting for its size.
  absl::optional<tab_groups::TabGroupId> target_group =
      tab_at(target_index)->group();
  if (target_group.has_value()) {
    target_index +=
        offset *
        (controller_->ListTabsInGroup(target_group.value()).length() - 1);
  }

  if (!IsValidModelIndex(target_index))
    return;

  if (controller_->IsTabPinned(start_index) !=
      controller_->IsTabPinned(target_index))
    return;

  controller_->MoveGroup(group, target_index);
}

void TabStrip::ResizeLayoutTabs() {
  // We've been called back after the TabStrip has been emptied out (probably
  // just prior to the window being destroyed). We need to do nothing here or
  // else GetTabAt below will crash.
  if (GetTabCount() == 0)
    return;

  // It is critically important that this is unhooked here, otherwise we will
  // keep spying on messages forever.
  RemoveMessageLoopObserver();

  ExitTabClosingMode();
  int pinned_tab_count = GetPinnedTabCount();
  if (pinned_tab_count == GetTabCount()) {
    // Only pinned tabs, we know the tab widths won't have changed (all
    // pinned tabs have the same width), so there is nothing to do.
    return;
  }
  // Don't try and avoid layout based on tab sizes. If tabs are small enough
  // then the width of the active tab may not change, but other widths may
  // have. This is particularly important if we've overflowed (all tabs are at
  // the min).
  StartResizeLayoutAnimation();
}

void TabStrip::LogTabWidthsForTabScrolling() {
  int active_tab_width = GetActiveTabWidth();
  int inactive_tab_width = GetInactiveTabWidth();

  if (active_tab_width > 1) {
    UMA_HISTOGRAM_EXACT_LINEAR("Tabs.ActiveTabWidth", active_tab_width, 257);
  }
  if (inactive_tab_width > 1) {
    UMA_HISTOGRAM_EXACT_LINEAR("Tabs.InactiveTabWidth", inactive_tab_width,
                               257);
  }
}

void TabStrip::ResizeLayoutTabsFromTouch() {
  // Don't resize if the user is interacting with the tabstrip.
  if (!drag_context_->IsDragSessionActive())
    ResizeLayoutTabs();
  else
    StartResizeLayoutTabsFromTouchTimer();
}

void TabStrip::StartResizeLayoutTabsFromTouchTimer() {
  // Amount of time we delay before resizing after a close from a touch.
  constexpr auto kTouchResizeLayoutTime = base::Seconds(2);

  resize_layout_timer_.Stop();
  resize_layout_timer_.Start(FROM_HERE, kTouchResizeLayoutTime, this,
                             &TabStrip::ResizeLayoutTabsFromTouch);
}

void TabStrip::AddMessageLoopObserver() {
  if (!mouse_watcher_) {
    // Expand the watched region downwards below the bottom of the tabstrip.
    // This allows users to move the cursor horizontally, to another tab,
    // without accidentally exiting closing mode if they drift verticaally
    // slightly out of the tabstrip.
    constexpr int kTabStripAnimationVSlop = 40;
    // Expand the watched region to the right to cover the NTB. This prevents
    // the scenario where the user goes to click on the NTB while they're in
    // closing mode, and closing mode exits just as they reach the NTB.
    constexpr int kTabStripAnimationHSlop = 60;
    mouse_watcher_ = std::make_unique<views::MouseWatcher>(
        std::make_unique<views::MouseWatcherViewHost>(
            this,
            gfx::Insets(0, base::i18n::IsRTL() ? kTabStripAnimationHSlop : 0,
                        kTabStripAnimationVSlop,
                        base::i18n::IsRTL() ? 0 : kTabStripAnimationHSlop)),
        this);
  }
  mouse_watcher_->Start(GetWidget()->GetNativeWindow());
}

void TabStrip::RemoveMessageLoopObserver() {
  mouse_watcher_ = nullptr;
}

gfx::Rect TabStrip::GetDropBounds(int drop_index,
                                  bool drop_before,
                                  bool drop_in_group,
                                  bool* is_beneath) {
  DCHECK_NE(drop_index, -1);

  // The X location the indicator points to.
  int center_x = -1;

  if (GetTabCount() == 0) {
    // If the tabstrip is empty, it doesn't matter where the drop arrow goes.
    // The tabstrip can only be transiently empty, e.g. during shutdown.
    return gfx::Rect();
  }

  Tab* tab = tab_at(std::min(drop_index, GetTabCount() - 1));
  const bool first_in_group =
      drop_index < GetTabCount() && tab->group().has_value() &&
      GetModelIndexOf(tab) ==
          controller_->GetFirstTabInGroup(tab->group().value());

  const int overlap = TabStyle::GetTabOverlap();
  if (!drop_before || !first_in_group || drop_in_group) {
    // Dropping between tabs, or between a group header and the group's first
    // tab.
    center_x = tab->x();
    const int width = tab->width();
    if (drop_index < GetTabCount())
      center_x += drop_before ? (overlap / 2) : (width / 2);
    else
      center_x += width - (overlap / 2);
  } else {
    // Dropping before a group header.
    TabGroupHeader* const header = group_header(tab->group().value());
    center_x = header->x() + overlap / 2;
  }

  // Mirror the center point if necessary.
  center_x = GetMirroredXInView(center_x);

  // Determine the screen bounds.
  gfx::Point drop_loc(center_x - g_drop_indicator_width / 2,
                      -g_drop_indicator_height);
  ConvertPointToScreen(this, &drop_loc);
  gfx::Rect drop_bounds(drop_loc.x(), drop_loc.y(), g_drop_indicator_width,
                        g_drop_indicator_height);

  // If the rect doesn't fit on the monitor, push the arrow to the bottom.
  display::Screen* screen = display::Screen::GetScreen();
  display::Display display = screen->GetDisplayMatching(drop_bounds);
  *is_beneath = !display.bounds().Contains(drop_bounds);
  if (*is_beneath)
    drop_bounds.Offset(0, drop_bounds.height() + height());

  return drop_bounds;
}

void TabStrip::SetDropArrow(
    const absl::optional<BrowserRootView::DropIndex>& index) {
  if (!index) {
    controller_->OnDropIndexUpdate(-1, false);
    drop_arrow_.reset();
    return;
  }

  // Let the controller know of the index update.
  controller_->OnDropIndexUpdate(index->value, index->drop_before);

  if (drop_arrow_ && (index == drop_arrow_->index()))
    return;

  bool is_beneath;
  gfx::Rect drop_bounds = GetDropBounds(index->value, index->drop_before,
                                        index->drop_in_group, &is_beneath);

  if (!drop_arrow_) {
    drop_arrow_ = std::make_unique<DropArrow>(*index, !is_beneath, GetWidget());
  } else {
    drop_arrow_->set_index(*index);
    drop_arrow_->SetPointDown(!is_beneath);
  }

  // Reposition the window.
  drop_arrow_->SetWindowBounds(drop_bounds);
}

// static
gfx::ImageSkia* TabStrip::GetDropArrowImage(bool is_down) {
  return ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
      is_down ? IDR_TAB_DROP_DOWN : IDR_TAB_DROP_UP);
}

// TabStrip:TabContextMenuController:
// ----------------------------------------------------------

TabStrip::TabContextMenuController::TabContextMenuController(TabStrip* parent)
    : parent_(parent) {}

void TabStrip::TabContextMenuController::ShowContextMenuForViewImpl(
    views::View* source,
    const gfx::Point& point,
    ui::MenuSourceType source_type) {
  // We are only intended to be installed as a context-menu handler for tabs, so
  // this cast should be safe.
  DCHECK(views::IsViewClass<Tab>(source));
  Tab* const tab = static_cast<Tab*>(source);
  if (tab->closing())
    return;
  parent_->controller()->ShowContextMenuForTab(tab, point, source_type);
}

// TabStrip:DropArrow:
// ----------------------------------------------------------

TabStrip::DropArrow::DropArrow(const BrowserRootView::DropIndex& index,
                               bool point_down,
                               views::Widget* context)
    : index_(index), point_down_(point_down) {
  arrow_window_ = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.z_order = ui::ZOrderLevel::kFloatingUIElement;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.accept_events = false;
  params.bounds = gfx::Rect(g_drop_indicator_width, g_drop_indicator_height);
  params.context = context->GetNativeWindow();
  arrow_window_->Init(std::move(params));
  arrow_view_ =
      arrow_window_->SetContentsView(std::make_unique<views::ImageView>());
  arrow_view_->SetImage(GetDropArrowImage(point_down_));
  scoped_observation_.Observe(arrow_window_.get());

  arrow_window_->Show();
}

TabStrip::DropArrow::~DropArrow() {
  // Close eventually deletes the window, which deletes arrow_view too.
  if (arrow_window_)
    arrow_window_->Close();
}

void TabStrip::DropArrow::SetPointDown(bool down) {
  if (point_down_ == down)
    return;

  point_down_ = down;
  arrow_view_->SetImage(GetDropArrowImage(point_down_));
}

void TabStrip::DropArrow::SetWindowBounds(const gfx::Rect& bounds) {
  arrow_window_->SetBounds(bounds);
}

void TabStrip::DropArrow::OnWidgetDestroying(views::Widget* widget) {
  DCHECK(scoped_observation_.IsObservingSource(arrow_window_.get()));
  scoped_observation_.Reset();
  arrow_window_ = nullptr;
}

///////////////////////////////////////////////////////////////////////////////

void TabStrip::PrepareForAnimation() {
  if (!drag_context_->IsDragSessionActive() &&
      !TabDragController::IsAttachedTo(GetDragContext())) {
    for (int i = 0; i < GetTabCount(); ++i)
      tab_at(i)->set_dragging(false);
  }
}

void TabStrip::UpdateIdealBounds() {
  if (GetTabCount() == 0)
    return;  // Should only happen during creation/destruction, ignore.

  // Update |last_available_width_| in case there is a different amount of
  // available width than there was in the last layout (e.g. if the tabstrip
  // is currently hidden).
  last_available_width_ = GetAvailableWidthForTabStrip();

  const int available_width_for_tabs = CalculateAvailableWidthForTabs();
  tab_container_->layout_helper()->UpdateIdealBounds(available_width_for_tabs);
}

int TabStrip::CalculateAvailableWidthForTabs() const {
  return override_available_width_for_tabs_.value_or(
      GetAvailableWidthForTabStrip());
}

int TabStrip::GetAvailableWidthForTabStrip() const {
  return available_width_callback_
             ? available_width_callback_.Run()
             : parent()->GetAvailableSize(this).width().value();
}

void TabStrip::StartResizeLayoutAnimation() {
  PrepareForAnimation();
  UpdateIdealBounds();
  AnimateToIdealBounds();
}

void TabStrip::StartPinnedTabAnimation() {
  ExitTabClosingMode();

  PrepareForAnimation();

  UpdateIdealBounds();
  AnimateToIdealBounds();
}

void TabStrip::OnTabSlotAnimationProgressed(TabSlotView* view) {
  // The rightmost tab moving might have changed the tabstrip's preferred width.
  PreferredSizeChanged();
  if (view->group())
    UpdateTabGroupVisuals(view->group().value());
}

void TabStrip::UpdateTabGroupVisuals(tab_groups::TabGroupId group_id) {
  const auto group_views = group_views_.find(group_id);
  if (group_views != group_views_.end())
    group_views->second->UpdateBounds();
}

const gfx::Rect& TabStrip::ideal_bounds(tab_groups::TabGroupId group) const {
  return tab_container_->layout_helper()->group_header_ideal_bounds().at(group);
}

bool TabStrip::OnMouseDragged(const ui::MouseEvent& event) {
  ContinueDrag(this, event);
  return true;
}

void TabStrip::OnMouseReleased(const ui::MouseEvent& event) {
  EndDrag(END_DRAG_COMPLETE);
}

void TabStrip::OnMouseCaptureLost() {
  EndDrag(END_DRAG_CAPTURE_LOST);
}

void TabStrip::OnMouseEntered(const ui::MouseEvent& event) {
  mouse_entered_tabstrip_time_ = base::TimeTicks::Now();
}

void TabStrip::OnMouseExited(const ui::MouseEvent& event) {
  UpdateHoverCard(nullptr, HoverCardUpdateType::kHover);
}

void TabStrip::AddedToWidget() {
  GetWidget()->AddObserver(this);
}

void TabStrip::RemovedFromWidget() {
  GetWidget()->RemoveObserver(this);
}

void TabStrip::OnGestureEvent(ui::GestureEvent* event) {
  switch (event->type()) {
    case ui::ET_GESTURE_SCROLL_END:
    case ui::ET_SCROLL_FLING_START:
    case ui::ET_GESTURE_END:
      EndDrag(END_DRAG_COMPLETE);
      break;

    case ui::ET_GESTURE_LONG_TAP: {
      EndDrag(END_DRAG_CANCEL);
      tab_container_->HandleLongTap(event);
      break;
    }

    case ui::ET_GESTURE_SCROLL_UPDATE:
      ContinueDrag(this, *event);
      break;

    case ui::ET_GESTURE_TAP_DOWN:
      EndDrag(END_DRAG_CANCEL);
      break;

    case ui::ET_GESTURE_TAP: {
      const int active_index = controller_->GetActiveIndex();
      DCHECK_NE(-1, active_index);
      Tab* active_tab = tab_at(active_index);
      TouchUMA::GestureActionType action = TouchUMA::kGestureTabNoSwitchTap;
      if (active_tab->tab_activated_with_last_tap_down())
        action = TouchUMA::kGestureTabSwitchTap;
      TouchUMA::RecordGestureAction(action);
      break;
    }

    default:
      break;
  }
  event->SetHandled();
}

void TabStrip::OnViewFocused(views::View* observed_view) {
  int index = tab_container_->tabs_view_model()->GetIndexOfView(observed_view);
  if (index != -1)
    controller_->OnKeyboardFocusedTabChanged(index);
}

void TabStrip::OnViewBlurred(views::View* observed_view) {
  controller_->OnKeyboardFocusedTabChanged(absl::nullopt);
}

void TabStrip::OnTouchUiChanged() {
  StopAnimating(true);
  PreferredSizeChanged();
}

void TabStrip::AnnounceTabAddedToGroup(tab_groups::TabGroupId group_id) {
  const std::u16string group_title = controller()->GetGroupTitle(group_id);
  const std::u16string contents_string =
      controller()->GetGroupContentString(group_id);
  GetViewAccessibility().AnnounceText(
      group_title.empty()
          ? l10n_util::GetStringFUTF16(
                IDS_TAB_AX_ANNOUNCE_TAB_ADDED_TO_UNNAMED_GROUP, contents_string)
          : l10n_util::GetStringFUTF16(
                IDS_TAB_AX_ANNOUNCE_TAB_ADDED_TO_NAMED_GROUP, group_title,
                contents_string));
}

void TabStrip::AnnounceTabRemovedFromGroup(tab_groups::TabGroupId group_id) {
  const std::u16string group_title = controller()->GetGroupTitle(group_id);
  const std::u16string contents_string =
      controller()->GetGroupContentString(group_id);
  GetViewAccessibility().AnnounceText(
      group_title.empty()
          ? l10n_util::GetStringFUTF16(
                IDS_TAB_AX_ANNOUNCE_TAB_REMOVED_FROM_UNNAMED_GROUP,
                contents_string)
          : l10n_util::GetStringFUTF16(
                IDS_TAB_AX_ANNOUNCE_TAB_REMOVED_FROM_NAMED_GROUP, group_title,
                contents_string));
}

BEGIN_METADATA(TabStrip, views::View)
ADD_PROPERTY_METADATA(int, BackgroundOffset)
ADD_READONLY_PROPERTY_METADATA(int, TabCount)
ADD_READONLY_PROPERTY_METADATA(int, ModelCount)
ADD_READONLY_PROPERTY_METADATA(int, PinnedTabCount)
ADD_READONLY_PROPERTY_METADATA(absl::optional<int>, FocusedTabIndex)
ADD_READONLY_PROPERTY_METADATA(int, StrokeThickness)
ADD_READONLY_PROPERTY_METADATA(SkColor,
                               ToolbarTopSeparatorColor,
                               ui::metadata::SkColorConverter)
ADD_READONLY_PROPERTY_METADATA(SkColor,
                               TabSeparatorColor,
                               ui::metadata::SkColorConverter)
ADD_READONLY_PROPERTY_METADATA(float, HoverOpacityForRadialHighlight)
ADD_READONLY_PROPERTY_METADATA(int, ActiveTabWidth)
ADD_READONLY_PROPERTY_METADATA(int, InactiveTabWidth)
ADD_READONLY_PROPERTY_METADATA(int, AvailableWidthForTabStrip)
END_METADATA
