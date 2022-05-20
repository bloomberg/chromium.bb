// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/templates/saved_desk_library_view.h"

#include "ash/constants/ash_features.h"
#include "ash/controls/rounded_scroll_bar.h"
#include "ash/public/cpp/desk_template.h"
#include "ash/public/cpp/desks_templates_delegate.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/pill_button.h"
#include "ash/wm/desks/templates/saved_desk_grid_view.h"
#include "ash/wm/desks/templates/saved_desk_item_view.h"
#include "ash/wm/desks/templates/saved_desk_name_view.h"
#include "ash/wm/desks/templates/saved_desk_util.h"
#include "ash/wm/overview/overview_controller.h"
#include "ui/aura/window.h"
#include "ui/aura/window_targeter.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {
namespace {

constexpr char kGridLabelFont[] = "Roboto";
constexpr int kGridLabelFontSize = 16;

// Grids use landscape mode if the available width is greater or equal to this.
constexpr int kLandscapeMinWidth = 756;

// Label dimensions.
constexpr gfx::Size kLabelSizeLandscape = {708, 24};
constexpr gfx::Size kLabelSizePortrait = {464, 24};

// Between child spacing of Library page scroll content view.
constexpr int kLibraryPageScrollContentsBetweenChildSpacingDp = 32;

// Insets of Library page scroll content view.
constexpr gfx::Insets kLibraryPageScrollContentsInsets = gfx::Insets::VH(32, 0);

// Insets for the vertical scroll bar.
constexpr gfx::Insets kLibraryPageVerticalScrollInsets =
    gfx::Insets::TLBR(1, 0, 1, 1);

struct SavedDesks {
  // Saved desks created as templates.
  std::vector<const DeskTemplate*> desk_templates;
  // Saved desks created for save & recall.
  std::vector<const DeskTemplate*> save_and_recall;
};

SavedDesks Group(const std::vector<const DeskTemplate*>& saved_desks) {
  SavedDesks grouped;

  for (auto* saved_desk : saved_desks) {
    switch (saved_desk->type()) {
      case DeskTemplateType::kTemplate:
        grouped.desk_templates.push_back(saved_desk);
        break;
      case DeskTemplateType::kSaveAndRecall:
        grouped.save_and_recall.push_back(saved_desk);
        break;
    }
  }

  return grouped;
}

std::unique_ptr<views::Label> MakeGridLabel(int label_string_id) {
  auto label = std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(label_string_id));
  label->SetFontList(gfx::FontList({kGridLabelFont}, gfx::Font::NORMAL,
                                   kGridLabelFontSize,
                                   gfx::Font::Weight::MEDIUM));
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  return label;
}

}  // namespace

// -----------------------------------------------------------------------------
// SavedDeskLibraryWindowTargeter:

// A custom targeter that only allows us to handle events which are located on
// the children of the library. The library takes up all the available space in
// overview, but we still want some events to fall through to the
// `OverviewGridEventHandler`, if they are not handled by the library's
// children.
class SavedDeskLibraryWindowTargeter : public aura::WindowTargeter {
 public:
  SavedDeskLibraryWindowTargeter(SavedDeskLibraryView* owner) : owner_(owner) {}
  SavedDeskLibraryWindowTargeter(const SavedDeskLibraryWindowTargeter&) =
      delete;
  SavedDeskLibraryWindowTargeter& operator=(
      const SavedDeskLibraryWindowTargeter&) = delete;

  // aura::WindowTargeter:
  bool SubtreeShouldBeExploredForEvent(aura::Window* window,
                                       const ui::LocatedEvent& event) override {
    // Process the event it is for scrolling.
    if (event.IsMouseWheelEvent())
      return true;

    // Check if the located event intersects with the library's children.
    // Convert to screen coordinate.
    gfx::Point screen_location;
    if (event.target()) {
      screen_location = event.target()->GetScreenLocation(event);
    } else {
      screen_location = event.root_location();
      wm::ConvertPointToScreen(window->GetRootWindow(), &screen_location);
    }
    // Process the event if it intersects with grid items.
    if (owner_->IntersectsWithUi(screen_location))
      return true;

    // None of the library's children will handle the event, so `window` won't
    // handle the event and it will fall through to the wallpaper.
    return false;
  }

 private:
  SavedDeskLibraryView* const owner_;
};

// -----------------------------------------------------------------------------
// SavedDeskLibraryEventHandler:

// This class is owned by SavedDeskLibraryView for the purpose of handling mouse
// and gesture events.
class SavedDeskLibraryEventHandler : public ui::EventHandler {
 public:
  explicit SavedDeskLibraryEventHandler(SavedDeskLibraryView* owner)
      : owner_(owner) {}
  SavedDeskLibraryEventHandler(const SavedDeskLibraryEventHandler&) = delete;
  SavedDeskLibraryEventHandler& operator=(const SavedDeskLibraryEventHandler&) =
      delete;
  ~SavedDeskLibraryEventHandler() override = default;

  void OnMouseEvent(ui::MouseEvent* event) override {
    owner_->OnLocatedEvent(event, /*is_touch=*/false);
  }

  void OnGestureEvent(ui::GestureEvent* event) override {
    owner_->OnLocatedEvent(event, /*is_touch=*/true);
  }

 private:
  SavedDeskLibraryView* const owner_;
};

// -----------------------------------------------------------------------------
// SavedDeskLibraryView:

// static
std::unique_ptr<views::Widget>
SavedDeskLibraryView::CreateSavedDeskLibraryWidget(aura::Window* root) {
  DCHECK(root);
  DCHECK(root->IsRootWindow());

  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.activatable = views::Widget::InitParams::Activatable::kYes;
  params.accept_events = true;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  // The parent should be a container that covers all the windows but is below
  // some other system UI features such as system tray and capture mode and also
  // below the system modal dialogs.
  // TODO(sammiequon): Investigate if there is a more suitable container for
  // this widget.
  params.parent = root->GetChildById(kShellWindowId_ShelfBubbleContainer);
  params.name = "SavedDeskLibraryWidget";

  auto widget = std::make_unique<views::Widget>(std::move(params));
  widget->SetContentsView(std::make_unique<SavedDeskLibraryView>());

  // Not opaque since we want to view the contents of the layer behind.
  widget->GetLayer()->SetFillsBoundsOpaquely(false);

  widget->GetNativeWindow()->SetId(kShellWindowId_SavedDeskLibraryWindow);

  return widget;
}

SavedDeskLibraryView::SavedDeskLibraryView() {
  // The entire page scrolls.
  scroll_view_ = AddChildView(std::make_unique<views::ScrollView>(
      views::ScrollView::ScrollWithLayers::kEnabled));
  scroll_view_->ClipHeightTo(0, std::numeric_limits<int>::max());
  scroll_view_->SetDrawOverflowIndicator(false);
  // Don't paint a background. The overview grid already has one.
  scroll_view_->SetBackgroundColor(absl::nullopt);
  // Arrow keys are used to select app icons.
  scroll_view_->SetAllowKeyboardScrolling(false);

  // Scroll view will have a gradient mask layer.
  scroll_view_->SetPaintToLayer(ui::LAYER_NOT_DRAWN);

  // Set up scroll bars.
  scroll_view_->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);
  // Use ash style rounded scroll bar just like `AppListBubbleAppsPage`.
  auto vertical_scroll =
      std::make_unique<RoundedScrollBar>(/*horizontal=*/false);
  vertical_scroll->SetInsets(kLibraryPageVerticalScrollInsets);
  vertical_scroll->SetSnapBackOnDragOutside(false);
  scroll_view_->SetVerticalScrollBar(std::move(vertical_scroll));

  // Set up scroll contents.
  auto scroll_contents = std::make_unique<views::View>();
  auto* layout =
      scroll_contents->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical,
          kLibraryPageScrollContentsInsets,
          kLibraryPageScrollContentsBetweenChildSpacingDp));
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  // Create grids depending on which features are enabled.
  if (saved_desk_util::AreDesksTemplatesEnabled()) {
    grid_labels_.push_back(scroll_contents->AddChildView(
        MakeGridLabel(IDS_ASH_DESKS_TEMPLATES_LIBRARY_TEMPLATES_GRID_LABEL)));
    desk_template_grid_view_ =
        scroll_contents->AddChildView(std::make_unique<SavedDeskGridView>());
    grid_views_.push_back(desk_template_grid_view_);
  }
  if (saved_desk_util::IsDeskSaveAndRecallEnabled()) {
    grid_labels_.push_back(scroll_contents->AddChildView(MakeGridLabel(
        IDS_ASH_DESKS_TEMPLATES_LIBRARY_SAVE_AND_RECALL_GRID_LABEL)));
    save_and_recall_grid_view_ =
        scroll_contents->AddChildView(std::make_unique<SavedDeskGridView>());
    grid_views_.push_back(save_and_recall_grid_view_);
  }

  feedback_button_ = scroll_contents->AddChildView(std::make_unique<PillButton>(
      base::BindRepeating(&SavedDeskLibraryView::OnFeedbackButtonPressed,
                          base::Unretained(this)),
      l10n_util::GetStringUTF16(
          IDS_ASH_PERSISTENT_DESKS_BAR_CONTEXT_MENU_FEEDBACK),
      PillButton::Type::kIcon, &kPersistentDesksBarFeedbackIcon));

  scroll_view_->SetContents(std::move(scroll_contents));
}

SavedDeskLibraryView::~SavedDeskLibraryView() {
  if (auto* widget_window = GetWidgetWindow()) {
    widget_window->RemovePreTargetHandler(event_handler_.get());
    widget_window->RemoveObserver(this);
  }
}

SavedDeskItemView* SavedDeskLibraryView::GetItemForUUID(
    const base::GUID& uuid) {
  for (auto* grid_view : grid_views()) {
    if (auto* item = grid_view->GetItemForUUID(uuid))
      return item;
  }
  return nullptr;
}

void SavedDeskLibraryView::PopulateGridUI(
    const std::vector<const DeskTemplate*>& entries,
    const gfx::Rect& grid_bounds,
    const base::GUID& last_saved_desk_uuid) {
  GetWidget()->SetBounds(grid_bounds);

  SavedDesks grouped_entries = Group(entries);
  if (desk_template_grid_view_) {
    desk_template_grid_view_->PopulateGridUI(grouped_entries.desk_templates,
                                             last_saved_desk_uuid);
  }
  if (save_and_recall_grid_view_) {
    save_and_recall_grid_view_->PopulateGridUI(grouped_entries.save_and_recall,
                                               last_saved_desk_uuid);
  }

  Layout();
}

void SavedDeskLibraryView::AddOrUpdateTemplates(
    const std::vector<const DeskTemplate*>& entries,
    bool initializing_grid_view,
    const base::GUID& last_saved_desk_uuid) {
  SavedDesks grouped_entries = Group(entries);
  if (desk_template_grid_view_) {
    desk_template_grid_view_->AddOrUpdateTemplates(
        grouped_entries.desk_templates, initializing_grid_view,
        last_saved_desk_uuid);
  }
  if (save_and_recall_grid_view_) {
    save_and_recall_grid_view_->AddOrUpdateTemplates(
        grouped_entries.save_and_recall, initializing_grid_view,
        last_saved_desk_uuid);
  }

  Layout();
}

void SavedDeskLibraryView::DeleteTemplates(
    const std::vector<std::string>& uuids) {
  if (desk_template_grid_view_)
    desk_template_grid_view_->DeleteTemplates(uuids);
  if (save_and_recall_grid_view_)
    save_and_recall_grid_view_->DeleteTemplates(uuids);

  Layout();
}

void SavedDeskLibraryView::OnFeedbackButtonPressed() {
  std::string extra_diagnostics;
  for (auto* grid : grid_views()) {
    for (auto* item : grid->grid_items())
      extra_diagnostics += (item->desk_template()->ToString() + "\n");
  }

  // Note that this will activate the dialog which will exit overview and delete
  // `this`.
  Shell::Get()->desks_templates_delegate()->OpenFeedbackDialog(
      extra_diagnostics);
}

bool SavedDeskLibraryView::IsAnimating() {
  for (auto* grid_view : grid_views()) {
    if (grid_view->IsAnimating())
      return true;
  }

  return false;
}

bool SavedDeskLibraryView::IntersectsWithUi(const gfx::Point& screen_location) {
  // Check saved desk items.
  for (auto* grid : grid_views()) {
    for (auto* item : grid->grid_items()) {
      if (item->GetBoundsInScreen().Contains(screen_location))
        return true;
    }
  }
  // Check feedback button.
  return feedback_button_->GetBoundsInScreen().Contains(screen_location);
}

aura::Window* SavedDeskLibraryView::GetWidgetWindow() {
  auto* widget = GetWidget();
  return widget ? widget->GetNativeWindow() : nullptr;
}

void SavedDeskLibraryView::OnLocatedEvent(ui::LocatedEvent* event,
                                          bool is_touch) {
  auto* widget_window = GetWidgetWindow();
  if (widget_window && widget_window->event_targeting_policy() ==
                           aura::EventTargetingPolicy::kNone) {
    // If this is true, then we're in the process of fading out `this` and don't
    // want to handle any events anymore so do nothing.
    return;
  }

  // We also don't want to handle any events while we are animating.
  if (IsAnimating()) {
    event->StopPropagation();
    event->SetHandled();
    return;
  }

  switch (event->type()) {
    case ui::ET_MOUSE_MOVED:
    case ui::ET_MOUSE_ENTERED:
    case ui::ET_MOUSE_RELEASED:
    case ui::ET_MOUSE_EXITED:
    case ui::ET_GESTURE_LONG_PRESS:
    case ui::ET_GESTURE_LONG_TAP: {
      const gfx::Point screen_location =
          event->target() ? event->target()->GetScreenLocation(*event)
                          : event->root_location();
      for (auto* grid_view : grid_views()) {
        for (SavedDeskItemView* grid_item : grid_view->grid_items())
          grid_item->UpdateHoverButtonsVisibility(screen_location, is_touch);
      }
      break;
    }
    default:
      break;
  }
}

void SavedDeskLibraryView::AddedToWidget() {
  event_handler_ = std::make_unique<SavedDeskLibraryEventHandler>(this);

  auto* widget_window = GetWidgetWindow();
  DCHECK(widget_window);
  widget_window->AddObserver(this);
  widget_window->AddPreTargetHandler(event_handler_.get());
  widget_window->SetEventTargeter(
      std::make_unique<SavedDeskLibraryWindowTargeter>(this));
}

void SavedDeskLibraryView::Layout() {
  if (bounds().IsEmpty())
    return;

  const bool landscape = width() >= kLandscapeMinWidth;
  for (auto* grid_view : grid_views()) {
    grid_view->set_layout_mode(landscape
                                   ? SavedDeskGridView::LayoutMode::LANDSCAPE
                                   : SavedDeskGridView::LayoutMode::PORTRAIT);
  }

  DCHECK_EQ(grid_views_.size(), grid_labels_.size());
  for (size_t i = 0; i != grid_views_.size(); ++i) {
    // Make the grid label invisible if the corresponding grid view is
    // empty. This will exclude it from the box layout.
    grid_labels_[i]->SetVisible(!grid_views_[i]->grid_items().empty());
    grid_labels_[i]->SetPreferredSize(landscape ? kLabelSizeLandscape
                                                : kLabelSizePortrait);
  }

  scroll_view_->SetBoundsRect({0, 0, width(), height()});
}

void SavedDeskLibraryView::OnThemeChanged() {
  views::View::OnThemeChanged();

  auto* color_provider = AshColorProvider::Get();
  feedback_button_->SetBackgroundColor(color_provider->GetBaseLayerColor(
      AshColorProvider::BaseLayerType::kTransparent80));

  for (views::Label* label : grid_labels_) {
    label->SetBackgroundColor(SK_ColorTRANSPARENT);
    label->SetEnabledColor(color_provider->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kTextColorPrimary));
  }
}

void SavedDeskLibraryView::OnWindowDestroying(aura::Window* window) {
  auto* widget_window = GetWidgetWindow();
  DCHECK_EQ(window, widget_window);
  DCHECK(event_handler_);
  widget_window->RemovePreTargetHandler(event_handler_.get());
  widget_window->RemoveObserver(this);
  event_handler_ = nullptr;
}

BEGIN_METADATA(SavedDeskLibraryView, views::View)
END_METADATA

}  // namespace ash
