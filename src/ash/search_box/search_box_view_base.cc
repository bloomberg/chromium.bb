// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/search_box/search_box_view_base.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "ash/public/cpp/app_list/app_list_color_provider.h"
#include "ash/search_box/search_box_view_delegate.h"
#include "base/bind.h"
#include "base/macros.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/base/ime/text_input_flags.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/border.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

constexpr int kInnerPadding = 16;

// Preferred width of search box.
constexpr int kSearchBoxPreferredWidth = 544;

// The keyboard select colour (6% black).
constexpr SkColor kSelectedColor = SkColorSetARGB(15, 0, 0, 0);

constexpr SkColor kSearchTextColor = SkColorSetRGB(0x33, 0x33, 0x33);

}  // namespace

// A background that paints a solid white rounded rect with a thin grey
// border.
class SearchBoxBackground : public views::Background {
 public:
  SearchBoxBackground(int corner_radius, SkColor color)
      : corner_radius_(corner_radius) {
    SetNativeControlColor(color);
  }
  ~SearchBoxBackground() override = default;

  void SetCornerRadius(int corner_radius) { corner_radius_ = corner_radius; }

 private:
  // views::Background overrides:
  void Paint(gfx::Canvas* canvas, views::View* view) const override {
    gfx::Rect bounds = view->GetContentsBounds();

    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(get_color());
    canvas->DrawRoundRect(bounds, corner_radius_, flags);
  }

  int corner_radius_;

  DISALLOW_COPY_AND_ASSIGN(SearchBoxBackground);
};

// To paint grey background on mic and back buttons, and close buttons for
// fullscreen launcher.
class SearchBoxImageButton : public views::ImageButton {
 public:
  explicit SearchBoxImageButton(PressedCallback callback)
      : ImageButton(std::move(callback)) {
    SetFocusBehavior(FocusBehavior::ALWAYS);

    // Avoid drawing default dashed focus and draw customized focus in
    // OnPaintBackground();
    SetInstallFocusRingOnFocus(false);

    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
    views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::ON);
    // InkDropState will reset after clicking.
    SetHasInkDropActionOnClick(true);
    views::InkDrop::Get(this)->SetCreateHighlightCallback(base::BindRepeating(
        [](Button* host) {
          constexpr SkColor ripple_color =
              SkColorSetA(gfx::kGoogleGrey900, 0x12);
          auto highlight = std::make_unique<views::InkDropHighlight>(
              gfx::SizeF(host->size()), ripple_color);
          highlight->set_visible_opacity(1.f);
          return highlight;
        },
        this));
    views::InkDrop::Get(this)->SetCreateRippleCallback(base::BindRepeating(
        [](SearchBoxImageButton* host)
            -> std::unique_ptr<views::InkDropRipple> {
          const gfx::Point center = host->GetLocalBounds().CenterPoint();
          const int ripple_radius = host->GetInkDropRadius();
          gfx::Rect bounds(center.x() - ripple_radius,
                           center.y() - ripple_radius, 2 * ripple_radius,
                           2 * ripple_radius);
          constexpr SkColor ripple_color =
              SkColorSetA(gfx::kGoogleGrey900, 0x17);

          return std::make_unique<views::FloodFillInkDropRipple>(
              host->size(), host->GetLocalBounds().InsetsFrom(bounds),
              views::InkDrop::Get(host)->GetInkDropCenterBasedOnLastEvent(),
              ripple_color, 1.0f);
        },
        this));

    SetPreferredSize({kSearchBoxButtonSizeDip, kSearchBoxButtonSizeDip});
    SetImageHorizontalAlignment(ALIGN_CENTER);
    SetImageVerticalAlignment(ALIGN_MIDDLE);

    views::InstallCircleHighlightPathGenerator(this);
  }
  ~SearchBoxImageButton() override {}

  // views::View overrides:
  bool OnKeyPressed(const ui::KeyEvent& event) override {
    // Disable space key to press the button. The keyboard events received
    // by this view are forwarded from a Textfield (SearchBoxView) and key
    // released events are not forwarded. This leaves the button in pressed
    // state.
    if (event.key_code() == ui::VKEY_SPACE)
      return false;

    return Button::OnKeyPressed(event);
  }

  void OnFocus() override {
    views::ImageButton::OnFocus();
    SchedulePaint();
  }

  void OnBlur() override {
    views::ImageButton::OnBlur();
    SchedulePaint();
  }

 private:
  int GetInkDropRadius() const { return width() / 2; }

  // views::View overrides:
  void OnPaintBackground(gfx::Canvas* canvas) override {
    if (HasFocus()) {
      cc::PaintFlags circle_flags;
      circle_flags.setAntiAlias(true);
      circle_flags.setColor(kSelectedColor);
      circle_flags.setStyle(cc::PaintFlags::kFill_Style);
      canvas->DrawCircle(GetLocalBounds().CenterPoint(), GetInkDropRadius(),
                         circle_flags);
    }
  }

  const char* GetClassName() const override { return "SearchBoxImageButton"; }

  DISALLOW_COPY_AND_ASSIGN(SearchBoxImageButton);
};

// To show context menu of selected view instead of that of focused view which
// is always this view when the user uses keyboard shortcut to open context
// menu.
class SearchBoxTextfield : public views::Textfield {
 public:
  explicit SearchBoxTextfield(SearchBoxViewBase* search_box_view)
      : search_box_view_(search_box_view) {}
  ~SearchBoxTextfield() override = default;

  // Overridden from views::View:
  void OnFocus() override {
    search_box_view_->OnSearchBoxFocusedChanged();
    Textfield::OnFocus();
  }

  void OnBlur() override {
    search_box_view_->OnSearchBoxFocusedChanged();
    // Clear selection and set the caret to the end of the text.
    ClearSelection();
    Textfield::OnBlur();

    // Search box focus announcement overlaps with opening or closing folder
    // alert, so we ignored the search box in those cases. Now reset the flag
    // here.
    auto& accessibility = GetViewAccessibility();
    if (accessibility.IsIgnored()) {
      accessibility.OverrideIsIgnored(false);
      accessibility.NotifyAccessibilityEvent(ax::mojom::Event::kTreeChanged);
    }
  }

  void OnGestureEvent(ui::GestureEvent* event) override {
    switch (event->type()) {
      case ui::ET_GESTURE_LONG_PRESS:
      case ui::ET_GESTURE_LONG_TAP:
        // Prevent Long Press from being handled at all, if inactive
        if (!search_box_view_->is_search_box_active()) {
          event->SetHandled();
          break;
        }
        // If |search_box_view_| is active, handle it as normal below
        FALLTHROUGH;
      default:
        // Handle all other events as normal
        Textfield::OnGestureEvent(event);
    }
  }

 private:
  SearchBoxViewBase* const search_box_view_;

  DISALLOW_COPY_AND_ASSIGN(SearchBoxTextfield);
};

SearchBoxViewBase::SearchBoxViewBase(SearchBoxViewDelegate* delegate)
    : delegate_(delegate),
      content_container_(new views::View),
      search_box_(new SearchBoxTextfield(this)) {
  DCHECK(delegate_);
  SetLayoutManager(std::make_unique<views::FillLayout>());
  AddChildView(content_container_);

  box_layout_ =
      content_container_->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal,
          gfx::Insets(0, kSearchBoxPadding),
          kInnerPadding -
              views::LayoutProvider::Get()->GetDistanceMetric(
                  views::DISTANCE_TEXTFIELD_HORIZONTAL_TEXT_PADDING)));
  box_layout_->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  box_layout_->set_minimum_cross_axis_size(kSearchBoxPreferredHeight);

  search_box_->SetBorder(views::NullBorder());
  search_box_->SetTextColor(kSearchTextColor);
  search_box_->SetBackgroundColor(SK_ColorTRANSPARENT);
  search_box_->set_controller(this);
  search_box_->SetTextInputType(ui::TEXT_INPUT_TYPE_SEARCH);
  search_box_->SetTextInputFlags(ui::TEXT_INPUT_FLAG_AUTOCORRECT_OFF);
  search_box_->SetFontList(search_box_->GetFontList().DeriveWithSizeDelta(2));
  search_box_->SetCursorEnabled(is_search_box_active_);

  back_button_ = new SearchBoxImageButton(base::BindRepeating(
      &SearchBoxViewDelegate::BackButtonPressed, base::Unretained(delegate_)));
  content_container_->AddChildView(back_button_);

  search_icon_ = new views::ImageView();
  content_container_->AddChildView(search_icon_);

  content_container_->AddChildView(search_box_);
  box_layout_->SetFlexForView(search_box_, 1);

  // An invisible space view to align |search_box_| to center.
  search_box_right_space_ = new views::View();
  search_box_right_space_->SetPreferredSize(gfx::Size(kSearchBoxIconSize, 0));
  content_container_->AddChildView(search_box_right_space_);

  assistant_button_ = new SearchBoxImageButton(
      base::BindRepeating(&SearchBoxViewDelegate::AssistantButtonPressed,
                          base::Unretained(delegate_)));
  assistant_button_->SetFlipCanvasOnPaintForRTLUI(false);
  // Default hidden, child class should decide if it should shown.
  assistant_button_->SetVisible(false);
  content_container_->AddChildView(assistant_button_);

  close_button_ = new SearchBoxImageButton(base::BindRepeating(
      &SearchBoxViewDelegate::CloseButtonPressed, base::Unretained(delegate_)));
  content_container_->AddChildView(close_button_);
}

SearchBoxViewBase::~SearchBoxViewBase() = default;

void SearchBoxViewBase::Init(const InitParams& params) {
  show_close_button_when_active_ = params.show_close_button_when_active;
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetMasksToBounds(true);
  if (params.create_background) {
    content_container_->SetBackground(std::make_unique<SearchBoxBackground>(
        kSearchBoxBorderCornerRadius,
        AppListColorProvider::Get()->GetSearchBoxBackgroundColor()));
  }
  UpdateSearchBoxBorder();
  UpdatePlaceholderTextStyle();
  SetupAssistantButton();
  SetupBackButton();
  SetupCloseButton();
  ModelChanged();
}

bool SearchBoxViewBase::HasSearch() const {
  return !search_box_->GetText().empty();
}

gfx::Rect SearchBoxViewBase::GetViewBoundsForSearchBoxContentsBounds(
    const gfx::Rect& rect) const {
  gfx::Rect view_bounds = rect;
  view_bounds.Inset(-GetInsets());
  return view_bounds;
}

views::ImageButton* SearchBoxViewBase::assistant_button() {
  return static_cast<views::ImageButton*>(assistant_button_);
}

views::ImageButton* SearchBoxViewBase::back_button() {
  return static_cast<views::ImageButton*>(back_button_);
}

views::ImageButton* SearchBoxViewBase::close_button() {
  return static_cast<views::ImageButton*>(close_button_);
}

void SearchBoxViewBase::ShowBackOrGoogleIcon(bool show_back_button) {
  search_icon_->SetVisible(!show_back_button);
  back_button_->SetVisible(show_back_button);
  content_container_->Layout();
}

void SearchBoxViewBase::SetSearchBoxActive(bool active,
                                           ui::EventType event_type) {
  if (active == is_search_box_active_)
    return;

  is_search_box_active_ = active;
  UpdateSearchIcon();
  UpdatePlaceholderTextStyle();
  search_box_->SetCursorEnabled(active);

  if (active) {
    search_box_->RequestFocus();
    RecordSearchBoxActivationHistogram(event_type);
  } else {
    search_box_->DestroyTouchSelection();
  }

  UpdateSearchBoxBorder();
  // Keep the current keyboard visibility if the user already started typing.
  if (event_type != ui::ET_KEY_PRESSED && event_type != ui::ET_KEY_RELEASED)
    UpdateKeyboardVisibility();
  UpdateButtonsVisisbility();
  OnSearchBoxActiveChanged(active);

  NotifyActiveChanged();

  content_container_->Layout();
  SchedulePaint();
}

bool SearchBoxViewBase::OnTextfieldEvent(ui::EventType type) {
  if (is_search_box_active_)
    return false;

  SetSearchBoxActive(true, type);
  return true;
}

gfx::Size SearchBoxViewBase::CalculatePreferredSize() const {
  return gfx::Size(kSearchBoxPreferredWidth, kSearchBoxPreferredHeight);
}

void SearchBoxViewBase::OnEnabledChanged() {
  search_box_->SetEnabled(GetEnabled());
  if (close_button_)
    close_button_->SetEnabled(GetEnabled());
}

const char* SearchBoxViewBase::GetClassName() const {
  return "SearchBoxView";
}

void SearchBoxViewBase::OnKeyEvent(ui::KeyEvent* event) {
  delegate_->OnSearchBoxKeyEvent(event);
}

void SearchBoxViewBase::OnGestureEvent(ui::GestureEvent* event) {
  HandleSearchBoxEvent(event);
}

void SearchBoxViewBase::OnMouseEvent(ui::MouseEvent* event) {
  HandleSearchBoxEvent(event);
}

void SearchBoxViewBase::NotifyGestureEvent() {
  search_box_->DestroyTouchSelection();
}

void SearchBoxViewBase::OnSearchBoxFocusedChanged() {
  UpdateSearchBoxBorder();
  Layout();
  SchedulePaint();
}

bool SearchBoxViewBase::IsSearchBoxTrimmedQueryEmpty() const {
  std::u16string trimmed_query;
  base::TrimWhitespace(search_box_->GetText(), base::TrimPositions::TRIM_ALL,
                       &trimmed_query);
  return trimmed_query.empty();
}

void SearchBoxViewBase::ClearSearch() {
  // Avoid setting |search_box_| text to empty if it is already empty.
  if (search_box_->GetText() == std::u16string())
    return;

  search_box_->SetText(std::u16string());
  UpdateButtonsVisisbility();
  // Updates model and fires query changed manually because SetText() above
  // does not generate ContentsChanged() notification.
  UpdateModel(false);
  NotifyQueryChanged();
}

void SearchBoxViewBase::OnSearchBoxActiveChanged(bool active) {}

void SearchBoxViewBase::NotifyQueryChanged() {
  DCHECK(delegate_);
  delegate_->QueryChanged(this);
}

void SearchBoxViewBase::NotifyActiveChanged() {
  DCHECK(delegate_);
  delegate_->ActiveChanged(this);
}

void SearchBoxViewBase::UpdateButtonsVisisbility() {
  DCHECK(close_button_ && assistant_button_);

  const bool should_show_close_button =
      !search_box_->GetText().empty() ||
      (show_close_button_when_active_ && is_search_box_active_);
  const bool should_show_assistant_button =
      show_assistant_button_ && !should_show_close_button;
  const bool should_show_search_box_right_space =
      !(should_show_close_button || should_show_assistant_button);

  if (close_button_->GetVisible() == should_show_close_button &&
      assistant_button_->GetVisible() == should_show_assistant_button &&
      search_box_right_space_->GetVisible() ==
          should_show_search_box_right_space) {
    return;
  }

  close_button_->SetVisible(should_show_close_button);
  assistant_button_->SetVisible(should_show_assistant_button);
  search_box_right_space_->SetVisible(should_show_search_box_right_space);

  content_container_->Layout();
}

void SearchBoxViewBase::ContentsChanged(views::Textfield* sender,
                                        const std::u16string& new_contents) {
  // Set search box focused when query changes.
  search_box_->RequestFocus();
  UpdateModel(true);
  NotifyQueryChanged();
  if (!new_contents.empty())
    SetSearchBoxActive(true, ui::ET_KEY_PRESSED);
  UpdateButtonsVisisbility();
}

bool SearchBoxViewBase::HandleMouseEvent(views::Textfield* sender,
                                         const ui::MouseEvent& mouse_event) {
  return OnTextfieldEvent(mouse_event.type());
}

bool SearchBoxViewBase::HandleGestureEvent(
    views::Textfield* sender,
    const ui::GestureEvent& gesture_event) {
  return OnTextfieldEvent(gesture_event.type());
}

void SearchBoxViewBase::SetSearchBoxBackgroundCornerRadius(int corner_radius) {
  auto* background =
      static_cast<SearchBoxBackground*>(GetSearchBoxBackground());
  if (background)
    background->SetCornerRadius(corner_radius);
}

void SearchBoxViewBase::SetSearchIconImage(gfx::ImageSkia image) {
  search_icon_->SetImage(image);
}

void SearchBoxViewBase::SetShowAssistantButton(bool show) {
  show_assistant_button_ = show;
  UpdateButtonsVisisbility();
}

void SearchBoxViewBase::HandleSearchBoxEvent(ui::LocatedEvent* located_event) {
  if (located_event->type() == ui::ET_MOUSE_PRESSED ||
      located_event->type() == ui::ET_GESTURE_TAP) {
    const bool event_is_in_searchbox_bounds =
        GetWidget()->GetWindowBoundsInScreen().Contains(
            located_event->root_location());
    // Don't handle an event out of the searchbox bounds.
    if (!event_is_in_searchbox_bounds)
      return;

    // If the event is in an inactive empty search box, enable the search box.
    if (!is_search_box_active_ && search_box_->GetText().empty()) {
      SetSearchBoxActive(true, located_event->type());
      return;
    }

    // Otherwise, update the keyboard in case it was hidden. Tapping again
    // should reopen it.
    UpdateKeyboardVisibility();
  }
}

// TODO(crbug.com/755219): Unify this with SetBackgroundColor.
void SearchBoxViewBase::UpdateBackgroundColor(SkColor color) {
  auto* background = GetSearchBoxBackground();
  if (background)
    background->SetNativeControlColor(color);
}

views::Background* SearchBoxViewBase::GetSearchBoxBackground() {
  return content_container_->background();
}

}  // namespace ash
