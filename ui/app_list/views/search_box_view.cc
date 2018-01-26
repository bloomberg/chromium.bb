// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/search_box_view.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "ash/app_list/model/search/search_box_model.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "components/wallpaper/wallpaper_color_profile.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/app_list_switches.h"
#include "ui/app_list/app_list_util.h"
#include "ui/app_list/app_list_view_delegate.h"
#include "ui/app_list/resources/grit/app_list_resources.h"
#include "ui/app_list/vector_icons/vector_icons.h"
#include "ui/app_list/views/app_list_view.h"
#include "ui/app_list/views/contents_view.h"
#include "ui/app_list/views/search_box_view_delegate.h"
#include "ui/app_list/views/search_result_page_view.h"
#include "ui/base/ime/text_input_flags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/shadow_value.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_util.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/shadow_border.h"
#include "ui/views/widget/widget.h"

using wallpaper::ColorProfileType;

namespace app_list {

namespace {

constexpr int kPadding = 12;
constexpr int kPaddingSearchResult = 16;
constexpr int kInnerPadding = 16;
constexpr int kPreferredWidth = 544;
constexpr int kSearchBoxBorderWidth = 4;

constexpr SkColor kSearchBoxBorderColor =
    SkColorSetARGBMacro(0x3D, 0xFF, 0xFF, 0xFF);

constexpr int kSearchBoxBorderCornerRadiusSearchResult = 4;
constexpr int kCloseIconSize = 24;
constexpr int kSearchBoxFocusBorderCornerRadius = 28;

constexpr int kLightVibrantBlendAlpha = 0xE6;

// Range of the fraction of app list from collapsed to peeking that search box
// should change opacity.
constexpr float kOpacityStartFraction = 0.1f;
constexpr float kOpacityEndFraction = 0.6f;

// Color of placeholder text in zero query state.
constexpr SkColor kZeroQuerySearchboxColor =
    SkColorSetARGBMacro(0x8A, 0x00, 0x00, 0x00);

// Gets the box layout inset horizontal padding for the state of AppListModel.
int GetBoxLayoutPaddingForState(ash::AppListState state) {
  if (state == ash::AppListState::kStateSearchResults)
    return kPaddingSearchResult;
  return kPadding;
}

}  // namespace

// A background that paints a solid white rounded rect with a thin grey border.
class SearchBoxBackground : public views::Background {
 public:
  explicit SearchBoxBackground(SkColor color)
      : corner_radius_(kSearchBoxBorderCornerRadius), color_(color) {}
  ~SearchBoxBackground() override {}

  void set_corner_radius(int corner_radius) { corner_radius_ = corner_radius; }
  void set_color(SkColor color) { color_ = color; }

 private:
  // views::Background overrides:
  void Paint(gfx::Canvas* canvas, views::View* view) const override {
    gfx::Rect bounds = view->GetContentsBounds();

    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(color_);
    canvas->DrawRoundRect(bounds, corner_radius_, flags);
  }

  int corner_radius_;
  SkColor color_;

  DISALLOW_COPY_AND_ASSIGN(SearchBoxBackground);
};

// To paint grey background on mic and back buttons, and close buttons for
// fullscreen launcher.
class SearchBoxImageButton : public views::ImageButton {
 public:
  explicit SearchBoxImageButton(views::ButtonListener* listener)
      : ImageButton(listener) {
    SetFocusBehavior(FocusBehavior::ALWAYS);
  }
  ~SearchBoxImageButton() override {}

  bool OnKeyPressed(const ui::KeyEvent& event) override {
    // Disable space key to press the button. The keyboard events received
    // by this view are forwarded from a Textfield (SearchBoxView) and key
    // released events are not forwarded. This leaves the button in pressed
    // state.
    if (event.key_code() == ui::VKEY_SPACE)
      return false;

    return Button::OnKeyPressed(event);
  }

 private:
  // views::View overrides:
  void OnPaintBackground(gfx::Canvas* canvas) override {
    if (state() == STATE_PRESSED) {
      canvas->FillRect(gfx::Rect(size()), kSelectedColor);
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
  explicit SearchBoxTextfield(SearchBoxView* search_box_view)
      : search_box_view_(search_box_view) {}
  ~SearchBoxTextfield() override = default;

  // Overridden from views::View:
  void ShowContextMenu(const gfx::Point& p,
                       ui::MenuSourceType source_type) override {
    views::View* selected_view =
        search_box_view_->GetSelectedViewInContentsView();
    if (source_type != ui::MENU_SOURCE_KEYBOARD || !selected_view) {
      views::Textfield::ShowContextMenu(p, source_type);
      return;
    }
    selected_view->ShowContextMenu(
        selected_view->GetKeyboardContextMenuLocation(),
        ui::MENU_SOURCE_KEYBOARD);
  }

  void OnFocus() override {
    search_box_view_->OnOnSearchBoxFocusedChanged();
    Textfield::OnFocus();
  }

  void OnBlur() override {
    search_box_view_->OnOnSearchBoxFocusedChanged();
    // Clear selection and set the caret to the end of the text.
    ClearSelection();
    Textfield::OnBlur();
  }

 private:
  SearchBoxView* const search_box_view_;

  DISALLOW_COPY_AND_ASSIGN(SearchBoxTextfield);
};

SearchBoxView::SearchBoxView(SearchBoxViewDelegate* delegate,
                             AppListViewDelegate* view_delegate,
                             AppListView* app_list_view)
    : delegate_(delegate),
      view_delegate_(view_delegate),
      content_container_(new views::View),
      search_box_(new SearchBoxTextfield(this)),
      app_list_view_(app_list_view) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  SetLayoutManager(std::make_unique<views::FillLayout>());
  SetPreferredSize(gfx::Size(kPreferredWidth, kSearchBoxPreferredHeight));
  UpdateSearchBoxBorder();
  AddChildView(content_container_);

  content_container_->SetBackground(
      std::make_unique<SearchBoxBackground>(background_color_));

  box_layout_ =
      content_container_->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::kHorizontal, gfx::Insets(0, kPadding),
          kInnerPadding -
              views::LayoutProvider::Get()->GetDistanceMetric(
                  views::DISTANCE_TEXTFIELD_HORIZONTAL_TEXT_PADDING)));
  box_layout_->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_CENTER);
  box_layout_->set_minimum_cross_axis_size(kSearchBoxPreferredHeight);

  search_box_->SetBorder(views::NullBorder());
  search_box_->SetTextColor(kSearchTextColor);
  search_box_->SetBackgroundColor(background_color_);
  search_box_->set_controller(this);
  search_box_->SetTextInputType(ui::TEXT_INPUT_TYPE_SEARCH);
  search_box_->SetTextInputFlags(ui::TEXT_INPUT_FLAG_AUTOCORRECT_OFF);
  back_button_ = new SearchBoxImageButton(this);
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  back_button_->SetImage(views::ImageButton::STATE_NORMAL,
                         rb.GetImageSkiaNamed(IDR_APP_LIST_FOLDER_BACK_NORMAL));
  back_button_->SetImageAlignment(views::ImageButton::ALIGN_CENTER,
                                  views::ImageButton::ALIGN_MIDDLE);
  SetBackButtonLabel(false);
  content_container_->AddChildView(back_button_);

  is_tablet_mode_ = app_list_view->is_tablet_mode();
  search_icon_ = new views::ImageView();
  content_container_->AddChildView(search_icon_);
  search_box_->set_placeholder_text_color(search_box_color_);
  search_box_->set_placeholder_text_draw_flags(gfx::Canvas::TEXT_ALIGN_CENTER);
  search_box_->SetFontList(search_box_->GetFontList().DeriveWithSizeDelta(2));
  back_button_->SetVisible(false);
  search_box_->SetCursorEnabled(is_search_box_active_);

  content_container_->AddChildView(search_box_);
  box_layout_->SetFlexForView(search_box_, 1);

  // An invisible space view to align |search_box_| to center.
  search_box_right_space_ = new views::View();
  search_box_right_space_->SetPreferredSize(gfx::Size(kSearchIconSize, 0));
  content_container_->AddChildView(search_box_right_space_);

  close_button_ = new SearchBoxImageButton(this);
  close_button_->SetImage(
      views::ImageButton::STATE_NORMAL,
      gfx::CreateVectorIcon(kIcCloseIcon, kCloseIconSize, search_box_color_));
  close_button_->SetVisible(false);
  close_button_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_APP_LIST_CLEAR_SEARCHBOX));
  content_container_->AddChildView(close_button_);

  view_delegate_->AddObserver(this);
  ModelChanged();
}

SearchBoxView::~SearchBoxView() {
  search_model_->search_box()->RemoveObserver(this);
  view_delegate_->RemoveObserver(this);
}

void SearchBoxView::ModelChanged() {
  if (search_model_)
    search_model_->search_box()->RemoveObserver(this);

  search_model_ = view_delegate_->GetSearchModel();
  DCHECK(search_model_);
  UpdateSearchIcon();
  search_model_->search_box()->AddObserver(this);

  HintTextChanged();
  OnWallpaperColorsChanged();
}

bool SearchBoxView::HasSearch() const {
  return !search_box_->text().empty();
}

void SearchBoxView::ClearSearch() {
  search_box_->SetText(base::string16());
  UpdateCloseButtonVisisbility();
  // Updates model and fires query changed manually because SetText() above
  // does not generate ContentsChanged() notification.
  UpdateModel(false);
  NotifyQueryChanged();
  app_list_view_->SetStateFromSearchBoxView(search_box_->text().empty());
}

gfx::Rect SearchBoxView::GetViewBoundsForSearchBoxContentsBounds(
    const gfx::Rect& rect) const {
  gfx::Rect view_bounds = rect;
  view_bounds.Inset(-GetInsets());
  return view_bounds;
}

views::ImageButton* SearchBoxView::back_button() {
  return static_cast<views::ImageButton*>(back_button_);
}

views::ImageButton* SearchBoxView::close_button() {
  return static_cast<views::ImageButton*>(close_button_);
}

void SearchBoxView::SetBackButtonLabel(bool folder) {
  if (!back_button_)
    return;

  base::string16 back_button_label(l10n_util::GetStringUTF16(
      folder ? IDS_APP_LIST_FOLDER_CLOSE_FOLDER_ACCESSIBILE_NAME
             : IDS_APP_LIST_BACK));
  back_button_->SetAccessibleName(back_button_label);
  back_button_->SetTooltipText(back_button_label);
}

void SearchBoxView::ShowBackOrGoogleIcon(bool show_back_button) {
  search_icon_->SetVisible(!show_back_button);
  back_button_->SetVisible(show_back_button);
  content_container_->Layout();
}

void SearchBoxView::SetSearchBoxActive(bool active) {
  if (active == is_search_box_active_)
    return;

  is_search_box_active_ = active;
  UpdateSearchIcon();
  UpdateBackgroundColor(background_color_);
  search_box_->set_placeholder_text_draw_flags(
      active ? (base::i18n::IsRTL() ? gfx::Canvas::TEXT_ALIGN_RIGHT
                                    : gfx::Canvas::TEXT_ALIGN_LEFT)
             : gfx::Canvas::TEXT_ALIGN_CENTER);
  search_box_->set_placeholder_text_color(active ? kZeroQuerySearchboxColor
                                                 : search_box_color_);
  search_box_->SetCursorEnabled(active);

  if (active)
    search_box_->RequestFocus();

  search_box_right_space_->SetVisible(!active);

  UpdateSearchBoxBorder();
  UpdateKeyboardVisibility();

  content_container_->Layout();
  SchedulePaint();
}

void SearchBoxView::UpdateKeyboardVisibility() {
  if (!is_tablet_mode_)
    return;

  keyboard::KeyboardController* const keyboard_controller =
      keyboard::KeyboardController::GetInstance();
  if (!keyboard_controller ||
      is_search_box_active_ == keyboard::IsKeyboardVisible()) {
    return;
  }

  if (is_search_box_active_) {
    keyboard_controller->ShowKeyboard(false);
    return;
  }

  keyboard_controller->HideKeyboard(
      keyboard::KeyboardController::HIDE_REASON_MANUAL);
}

void SearchBoxView::HandleSearchBoxEvent(ui::LocatedEvent* located_event) {
  if (located_event->type() == ui::ET_MOUSEWHEEL) {
    if (!app_list_view_->HandleScroll(
            located_event->AsMouseWheelEvent()->offset().y(),
            ui::ET_MOUSEWHEEL)) {
      return;
    }

  } else if (located_event->type() == ui::ET_MOUSE_PRESSED ||
             located_event->type() == ui::ET_GESTURE_TAP) {
    bool event_is_in_searchbox_bounds =
        GetWidget()->GetWindowBoundsInScreen().Contains(
            located_event->root_location());
    if (is_search_box_active_ || !event_is_in_searchbox_bounds ||
        !search_box_->text().empty())
      return;
    // If the event was within the searchbox bounds and in an inactive empty
    // search box, enable the search box.
    SetSearchBoxActive(true);
  }
  located_event->SetHandled();
}

bool SearchBoxView::OnTextfieldEvent() {
  if (is_search_box_active_)
    return false;

  SetSearchBoxActive(true);
  return true;
}

bool SearchBoxView::OnMouseWheel(const ui::MouseWheelEvent& event) {
  if (contents_view_)
    return contents_view_->OnMouseWheel(event);
  return false;
}

void SearchBoxView::OnEnabledChanged() {
  search_box_->SetEnabled(enabled());
  if (close_button_)
    close_button_->SetEnabled(enabled());
}

const char* SearchBoxView::GetClassName() const {
  return "SearchBoxView";
}

void SearchBoxView::OnGestureEvent(ui::GestureEvent* event) {
  HandleSearchBoxEvent(event);
}

void SearchBoxView::OnMouseEvent(ui::MouseEvent* event) {
  HandleSearchBoxEvent(event);
}

void SearchBoxView::OnKeyEvent(ui::KeyEvent* event) {
  app_list_view_->RedirectKeyEventToSearchBox(event);

  if (!CanProcessUpDownKeyTraversal(*event))
    return;

  // If focus is in search box view, up key moves focus to the last element of
  // contents view, while down key moves focus to the first element of contents
  // view.
  views::View* v = GetFocusManager()->GetNextFocusableView(
      contents_view_, GetWidget(), event->key_code() == ui::VKEY_UP, false);
  if (v)
    v->RequestFocus();
  event->SetHandled();
}

ax::mojom::Role SearchBoxView::GetAccessibleWindowRole() const {
  // Default role of root view is ax::mojom::Role::kWindow which traps ChromeVox
  // focus within the root view. Assign ax::mojom::Role::kGroup here to allow
  // the focus to move from elements in search box to app list view.
  return ax::mojom::Role::kGroup;
}

bool SearchBoxView::ShouldAdvanceFocusToTopLevelWidget() const {
  // Focus should be able to move from search box to items in app list view.
  return true;
}

void SearchBoxView::ButtonPressed(views::Button* sender,
                                  const ui::Event& event) {
  if (back_button_ && sender == back_button_) {
    delegate_->BackButtonPressed();
  } else if (close_button_ && sender == close_button_) {
    ClearSearch();
    app_list_view_->SetStateFromSearchBoxView(true);
  } else {
    NOTREACHED();
  }
}

void SearchBoxView::UpdateBackground(double progress,
                                     ash::AppListState current_state,
                                     ash::AppListState target_state) {
  GetSearchBoxBackground()->set_corner_radius(gfx::Tween::LinearIntValueBetween(
      progress, GetSearchBoxBorderCornerRadiusForState(current_state),
      GetSearchBoxBorderCornerRadiusForState(target_state)));
  const SkColor color = gfx::Tween::ColorValueBetween(
      progress, GetBackgroundColorForState(current_state),
      GetBackgroundColorForState(target_state));
  UpdateBackgroundColor(color);
}

void SearchBoxView::UpdateLayout(double progress,
                                 ash::AppListState current_state,
                                 ash::AppListState target_state) {
  box_layout_->set_inside_border_insets(
      gfx::Insets(0, gfx::Tween::LinearIntValueBetween(
                         progress, GetBoxLayoutPaddingForState(current_state),
                         GetBoxLayoutPaddingForState(target_state))));
  InvalidateLayout();
}

void SearchBoxView::OnTabletModeChanged(bool started) {
  is_tablet_mode_ = started;
  UpdateKeyboardVisibility();
}

int SearchBoxView::GetSearchBoxBorderCornerRadiusForState(
    ash::AppListState state) const {
  if (state == ash::AppListState::kStateSearchResults &&
      !app_list_view_->is_in_drag()) {
    return kSearchBoxBorderCornerRadiusSearchResult;
  }
  return kSearchBoxBorderCornerRadius;
}

SkColor SearchBoxView::GetBackgroundColorForState(
    ash::AppListState state) const {
  if (state == ash::AppListState::kStateSearchResults)
    return kCardBackgroundColor;
  return background_color_;
}

void SearchBoxView::UpdateOpacity() {
  // The opacity of searchbox is a function of the fractional displacement of
  // the app list from collapsed(0) to peeking(1) state. When the fraction
  // changes from |kOpacityStartFraction| to |kOpaticyEndFraction|, the opacity
  // of searchbox changes from 0.f to 1.0f.
  ContentsView* contents_view = static_cast<ContentsView*>(contents_view_);
  int app_list_y_position_in_screen =
      contents_view->app_list_view()->app_list_y_position_in_screen();
  float fraction =
      std::max<float>(0, contents_view->app_list_view()->GetScreenBottom() -
                             kShelfSize - app_list_y_position_in_screen) /
      (kPeekingAppListHeight - kShelfSize);

  float opacity =
      std::min(std::max((fraction - kOpacityStartFraction) /
                            (kOpacityEndFraction - kOpacityStartFraction),
                        0.f),
               1.0f);

  AppListView* app_list_view = contents_view->app_list_view();
  bool should_restore_opacity =
      !app_list_view->is_in_drag() &&
      (app_list_view->app_list_state() != AppListViewState::CLOSED);
  // Restores the opacity of searchbox if the gesture dragging ends.
  this->layer()->SetOpacity(should_restore_opacity ? 1.0f : opacity);
  contents_view->search_results_page_view()->layer()->SetOpacity(
      should_restore_opacity ? 1.0f : opacity);
}

views::View* SearchBoxView::GetSelectedViewInContentsView() const {
  if (!contents_view_)
    return nullptr;
  return static_cast<ContentsView*>(contents_view_)->GetSelectedView();
}

void SearchBoxView::OnOnSearchBoxFocusedChanged() {
  UpdateSearchBoxBorder();
  Layout();
  SchedulePaint();
}

void SearchBoxView::UpdateModel(bool initiated_by_user) {
  // Temporarily remove from observer to ignore notifications caused by us.
  search_model_->search_box()->RemoveObserver(this);
  search_model_->search_box()->Update(search_box_->text(), initiated_by_user);
  search_model_->search_box()->SetSelectionModel(
      search_box_->GetSelectionModel());
  search_model_->search_box()->AddObserver(this);
}

void SearchBoxView::NotifyQueryChanged() {
  DCHECK(delegate_);
  delegate_->QueryChanged(this);
}

void SearchBoxView::ContentsChanged(views::Textfield* sender,
                                    const base::string16& new_contents) {
  // Set search box focused when query changes.
  search_box_->RequestFocus();
  UpdateModel(true);
  NotifyQueryChanged();
  SetSearchBoxActive(true);
  UpdateCloseButtonVisisbility();
  const bool is_trimmed_query_empty = IsSearchBoxTrimmedQueryEmpty();
  // If the query is only whitespace, don't transition the AppListView state.
  app_list_view_->SetStateFromSearchBoxView(is_trimmed_query_empty);
}

bool SearchBoxView::HandleKeyEvent(views::Textfield* sender,
                                   const ui::KeyEvent& key_event) {
  if (key_event.type() == ui::ET_KEY_PRESSED &&
      key_event.key_code() == ui::VKEY_RETURN) {
    if (!IsSearchBoxTrimmedQueryEmpty()) {
      // Hitting Enter when focus is on search box opens the first result.
      ui::KeyEvent event(key_event);
      views::View* first_result_view =
          static_cast<ContentsView*>(contents_view_)
              ->search_results_page_view()
              ->first_result_view();
      if (first_result_view)
        first_result_view->OnKeyEvent(&event);
      return true;
    }

    if (!is_search_box_active()) {
      SetSearchBoxActive(true);
      return true;
    }
    return false;
  }

  if (CanProcessLeftRightKeyTraversal(key_event))
    return ProcessLeftRightKeyTraversalForTextfield(search_box_, key_event);
  return false;
}

bool SearchBoxView::HandleMouseEvent(views::Textfield* sender,
                                     const ui::MouseEvent& mouse_event) {
  if (mouse_event.type() == ui::ET_MOUSEWHEEL) {
    return app_list_view_->HandleScroll(
        (&mouse_event)->AsMouseWheelEvent()->offset().y(), ui::ET_MOUSEWHEEL);
  } else {
    return OnTextfieldEvent();
  }
}

bool SearchBoxView::HandleGestureEvent(views::Textfield* sender,
                                       const ui::GestureEvent& gesture_event) {
  return OnTextfieldEvent();
}

void SearchBoxView::HintTextChanged() {
  const app_list::SearchBoxModel* search_box = search_model_->search_box();
  search_box_->set_placeholder_text(search_box->hint_text());
  search_box_->SetAccessibleName(search_box->accessible_name());
}

void SearchBoxView::SelectionModelChanged() {
  search_box_->SelectSelectionModel(
      search_model_->search_box()->selection_model());
}

void SearchBoxView::Update() {
  search_box_->SetText(search_model_->search_box()->text());
  UpdateCloseButtonVisisbility();
  NotifyQueryChanged();
}

void SearchBoxView::OnWallpaperColorsChanged() {
  std::vector<SkColor> prominent_colors;
  GetWallpaperProminentColors(&prominent_colors);
  if (prominent_colors.empty())
    return;
  DCHECK_EQ(static_cast<size_t>(ColorProfileType::NUM_OF_COLOR_PROFILES),
            prominent_colors.size());

  SetSearchBoxColor(
      prominent_colors[static_cast<int>(ColorProfileType::DARK_MUTED)]);
  SetBackgroundColor(
      prominent_colors[static_cast<int>(ColorProfileType::LIGHT_VIBRANT)]);
  UpdateSearchIcon();
  close_button_->SetImage(
      views::Button::STATE_NORMAL,
      gfx::CreateVectorIcon(kIcCloseIcon, kCloseIconSize, search_box_color_));
  search_box_->set_placeholder_text_color(search_box_color_);
  UpdateBackgroundColor(background_color_);
  SchedulePaint();
}

void SearchBoxView::UpdateSearchIcon() {
  const gfx::VectorIcon& google_icon =
      is_search_box_active() ? kIcGoogleColorIcon : kIcGoogleBlackIcon;
  const gfx::VectorIcon& icon = search_model_->search_engine_is_google()
                                    ? google_icon
                                    : kIcSearchEngineNotGoogleIcon;
  search_icon_->SetImage(
      gfx::CreateVectorIcon(icon, kSearchIconSize, search_box_color_));
}

void SearchBoxView::GetWallpaperProminentColors(std::vector<SkColor>* colors) {
  view_delegate_->GetWallpaperProminentColors(colors);
}

// TODO(crbug.com/755219): Unify this with UpdateBackgroundColor.
void SearchBoxView::SetBackgroundColor(SkColor light_vibrant) {
  const SkColor light_vibrant_mixed = color_utils::AlphaBlend(
      SK_ColorWHITE, light_vibrant, kLightVibrantBlendAlpha);
  background_color_ = SK_ColorTRANSPARENT == light_vibrant
                          ? kSearchBoxBackgroundDefault
                          : light_vibrant_mixed;
}

void SearchBoxView::SetSearchBoxColor(SkColor color) {
  search_box_color_ =
      SK_ColorTRANSPARENT == color ? kDefaultSearchboxColor : color;
}

// TODO(crbug.com/755219): Unify this with SetBackgroundColor.
void SearchBoxView::UpdateBackgroundColor(SkColor color) {
  if (is_search_box_active_)
    color = kSearchBoxBackgroundDefault;
  GetSearchBoxBackground()->set_color(color);
  search_box_->SetBackgroundColor(color);
}

void SearchBoxView::UpdateCloseButtonVisisbility() {
  if (!close_button_)
    return;
  bool should_show_close_button_ = !search_box_->text().empty();
  if (close_button_->visible() == should_show_close_button_)
    return;
  close_button_->SetVisible(should_show_close_button_);
  content_container_->Layout();
}

SearchBoxBackground* SearchBoxView::GetSearchBoxBackground() const {
  return static_cast<SearchBoxBackground*>(content_container_->background());
}

bool SearchBoxView::IsSearchBoxTrimmedQueryEmpty() const {
  base::string16 trimmed_query;
  base::TrimWhitespace(search_box_->text(), base::TrimPositions::TRIM_ALL,
                       &trimmed_query);
  return trimmed_query.empty();
}

void SearchBoxView::UpdateSearchBoxBorder() {
  if (search_box_->HasFocus() && !is_search_box_active()) {
    // Show a gray ring around search box to indicate that the search box is
    // selected. Do not show it when search box is active, because blinking
    // cursor already indicates that.
    SetBorder(views::CreateRoundedRectBorder(kSearchBoxBorderWidth,
                                             kSearchBoxFocusBorderCornerRadius,
                                             kSearchBoxBorderColor));
    return;
  }

  // Creates an empty border as a placeholder for colored border so that
  // re-layout won't move views below the search box.
  SetBorder(
      views::CreateEmptyBorder(kSearchBoxBorderWidth, kSearchBoxBorderWidth,
                               kSearchBoxBorderWidth, kSearchBoxBorderWidth));
}

}  // namespace app_list
