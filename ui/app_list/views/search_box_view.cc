// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/search_box_view.h"

#include <algorithm>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "components/wallpaper/wallpaper_color_profile.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/app_list_features.h"
#include "ui/app_list/app_list_switches.h"
#include "ui/app_list/app_list_view_delegate.h"
#include "ui/app_list/resources/grit/app_list_resources.h"
#include "ui/app_list/search_box_model.h"
#include "ui/app_list/speech_ui_model.h"
#include "ui/app_list/vector_icons/vector_icons.h"
#include "ui/app_list/views/app_list_view.h"
#include "ui/app_list/views/contents_view.h"
#include "ui/app_list/views/search_box_view_delegate.h"
#include "ui/base/ime/text_input_flags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/shadow_value.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/shadow_border.h"
#include "ui/views/widget/widget.h"

using wallpaper::ColorProfileType;

namespace app_list {

namespace {

constexpr int kPadding = 12;
constexpr int kInnerPadding = 24;
constexpr int kPreferredWidth = 360;
constexpr int kPreferredWidthFullscreen = 544;
constexpr int kSearchBoxPreferredHeight = 48;

constexpr SkColor kHintTextColor = SkColorSetARGBMacro(0xFF, 0xA0, 0xA0, 0xA0);

constexpr int kSearchBoxBorderCornerRadius = 2;
constexpr int kSearchBoxBorderCornerRadiusSearchResult = 4;
constexpr int kMicIconSize = 24;
constexpr int kCloseIconSize = 24;

constexpr int kLightVibrantBlendAlpha = 0xB3;

// Color of placeholder text in zero query state.
constexpr SkColor kZeroQuerySearchboxColor =
    SkColorSetARGBMacro(0x8A, 0x00, 0x00, 0x00);

// A background that paints a solid white rounded rect with a thin grey border.
class SearchBoxBackground : public views::Background {
 public:
  explicit SearchBoxBackground(
      SkColor color,
      int corner_radius = features::IsFullscreenAppListEnabled()
                              ? kSearchBoxBorderCornerRadiusFullscreen
                              : kSearchBoxBorderCornerRadius)
      : corner_radius_(corner_radius), color_(color) {}
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

}  // namespace

// To paint grey background on mic and back buttons
class SearchBoxImageButton : public views::ImageButton {
 public:
  explicit SearchBoxImageButton(views::ButtonListener* listener)
      : ImageButton(listener), selected_(false) {}
  ~SearchBoxImageButton() override {}

  bool selected() { return selected_; }
  void SetSelected(bool selected) {
    if (selected_ == selected)
      return;

    selected_ = selected;
    SchedulePaint();
    if (selected)
      NotifyAccessibilityEvent(ui::AX_EVENT_SELECTION, true);
  }

  bool OnKeyPressed(const ui::KeyEvent& event) override {
    // Disable space key to press the button. The keyboard events received
    // by this view are forwarded from a Textfield (SearchBoxView) and key
    // released events are not forwarded. This leaves the button in pressed
    // state.
    if (event.key_code() == ui::VKEY_SPACE)
      return false;

    return CustomButton::OnKeyPressed(event);
  }

 private:
  // views::View overrides:
  void OnPaintBackground(gfx::Canvas* canvas) override {
    if (state() == STATE_HOVERED || state() == STATE_PRESSED || selected_)
      canvas->FillRect(gfx::Rect(size()), kSelectedColor);
  }

  const char* GetClassName() const override { return "SearchBoxImageButton"; }

  bool selected_;

  DISALLOW_COPY_AND_ASSIGN(SearchBoxImageButton);
};

SearchBoxView::SearchBoxView(SearchBoxViewDelegate* delegate,
                             AppListViewDelegate* view_delegate,
                             AppListView* app_list_view)
    : delegate_(delegate),
      view_delegate_(view_delegate),
      content_container_(new views::View),
      search_box_(new views::Textfield),
      app_list_view_(app_list_view),
      focused_view_(FOCUS_SEARCH_BOX),
      is_fullscreen_app_list_enabled_(features::IsFullscreenAppListEnabled()) {
  SetLayoutManager(new views::FillLayout);
  SetPreferredSize(gfx::Size(is_fullscreen_app_list_enabled_
                                 ? kPreferredWidthFullscreen
                                 : kPreferredWidth,
                             kSearchBoxPreferredHeight));
  AddChildView(content_container_);

  SetShadow(GetShadowForZHeight(2));
  content_container_->SetBackground(
      base::MakeUnique<SearchBoxBackground>(background_color_));

  views::BoxLayout* layout = new views::BoxLayout(
      views::BoxLayout::kHorizontal, gfx::Insets(0, kPadding),
      kInnerPadding - views::Textfield::kTextPadding);
  content_container_->SetLayoutManager(layout);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_CENTER);
  layout->set_minimum_cross_axis_size(kSearchBoxPreferredHeight);

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

  if (is_fullscreen_app_list_enabled_) {
    search_icon_ = new views::ImageView();
    content_container_->AddChildView(search_icon_);
    search_box_->set_placeholder_text_color(search_box_color_);
    search_box_->set_placeholder_text_draw_flags(
        gfx::Canvas::TEXT_ALIGN_CENTER);
    search_box_->SetFontList(search_box_->GetFontList().DeriveWithSizeDelta(2));
    back_button_->SetVisible(false);
    search_box_->SetCursorEnabled(is_search_box_active_);
  } else {
    search_box_->set_placeholder_text_color(kHintTextColor);
  }
  content_container_->AddChildView(search_box_);
  layout->SetFlexForView(search_box_, 1);

  if (is_fullscreen_app_list_enabled_) {
    close_button_ = new SearchBoxImageButton(this);
    close_button_->SetImage(
        views::ImageButton::STATE_NORMAL,
        gfx::CreateVectorIcon(kIcCloseIcon, kCloseIconSize, search_box_color_));
    close_button_->SetVisible(false);
    content_container_->AddChildView(close_button_);
  }

  view_delegate_->GetSpeechUI()->AddObserver(this);
  ModelChanged();
}

SearchBoxView::~SearchBoxView() {
  view_delegate_->GetSpeechUI()->RemoveObserver(this);
  model_->search_box()->RemoveObserver(this);
}

void SearchBoxView::ModelChanged() {
  if (model_)
    model_->search_box()->RemoveObserver(this);

  model_ = view_delegate_->GetModel();
  DCHECK(model_);
  if (is_fullscreen_app_list_enabled_)
    UpdateSearchIcon();
  model_->search_box()->AddObserver(this);
  SpeechRecognitionButtonPropChanged();
  HintTextChanged();
  WallpaperProminentColorsChanged();
}

bool SearchBoxView::HasSearch() const {
  return !search_box_->text().empty();
}

void SearchBoxView::ClearSearch() {
  search_box_->SetText(base::string16());
  view_delegate_->AutoLaunchCanceled();
  // Updates model and fires query changed manually because SetText() above
  // does not generate ContentsChanged() notification.
  UpdateModel();
  NotifyQueryChanged();
  if (is_fullscreen_app_list_enabled_) {
    SetSearchBoxActive(false);
    app_list_view_->SetStateFromSearchBoxView(search_box_->text().empty());
  }
}

void SearchBoxView::SetShadow(const gfx::ShadowValue& shadow) {
  if (is_fullscreen_app_list_enabled_)
    return;

  SetBorder(base::MakeUnique<views::ShadowBorder>(shadow));
  Layout();
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

// Returns true if set internally, i.e. if focused_view_ != CONTENTS_VIEW.
// Note: because we always want to be able to type in the edit box, this is only
// a faux-focus so that buttons can respond to the ENTER key.
bool SearchBoxView::MoveTabFocus(bool move_backwards) {
  if (back_button_)
    back_button_->SetSelected(false);
  if (speech_button_)
    speech_button_->SetSelected(false);
  if (close_button_)
    close_button_->SetSelected(false);

  switch (focused_view_) {
    case FOCUS_BACK_BUTTON:
      focused_view_ = move_backwards ? FOCUS_BACK_BUTTON : FOCUS_SEARCH_BOX;
      break;
    case FOCUS_SEARCH_BOX:
      if (move_backwards) {
        focused_view_ =
            back_button_ && back_button_->visible()
                ? FOCUS_BACK_BUTTON
                : (is_fullscreen_app_list_enabled_ ? FOCUS_CONTENTS_VIEW
                                                   : FOCUS_SEARCH_BOX);
      } else {
        focused_view_ = speech_button_ && speech_button_->visible()
                            ? FOCUS_MIC_BUTTON
                            : (close_button_ && close_button_->visible()
                                   ? FOCUS_CLOSE_BUTTON
                                   : FOCUS_CONTENTS_VIEW);
      }
      break;
    case FOCUS_MIC_BUTTON:
    case FOCUS_CLOSE_BUTTON:
      focused_view_ = move_backwards ? FOCUS_SEARCH_BOX : FOCUS_CONTENTS_VIEW;
      break;
    case FOCUS_CONTENTS_VIEW:
      if (move_backwards) {
        focused_view_ = speech_button_ && speech_button_->visible()
                            ? FOCUS_MIC_BUTTON
                            : (close_button_ && close_button_->visible()
                                   ? FOCUS_CLOSE_BUTTON
                                   : FOCUS_SEARCH_BOX);
      } else {
        focused_view_ =
            !is_fullscreen_app_list_enabled_
                ? FOCUS_CONTENTS_VIEW
                : (back_button_ && back_button_->visible() ? FOCUS_BACK_BUTTON
                                                           : FOCUS_SEARCH_BOX);
      }
      break;
    default:
      DCHECK(false);
  }

  switch (focused_view_) {
    case FOCUS_BACK_BUTTON:
      if (back_button_)
        back_button_->SetSelected(true);
      break;
    case FOCUS_SEARCH_BOX:
      // Set the ChromeVox focus to the search box. However, DO NOT do this if
      // we are in the search results state (i.e., if the search box has text in
      // it), because the focus is about to be shifted to the first search
      // result and we do not want to read out the name of the search box as
      // well.
      if (search_box_->text().empty())
        search_box_->NotifyAccessibilityEvent(ui::AX_EVENT_FOCUS, true);
      break;
    case FOCUS_MIC_BUTTON:
      if (speech_button_)
        speech_button_->SetSelected(true);
      break;
    case FOCUS_CLOSE_BUTTON:
      if (close_button_)
        close_button_->SetSelected(true);
      break;
    default:
      break;
  }

  if (focused_view_ < FOCUS_CONTENTS_VIEW)
    delegate_->SetSearchResultSelection(focused_view_ == FOCUS_SEARCH_BOX);

  return (focused_view_ < FOCUS_CONTENTS_VIEW);
}

void SearchBoxView::ResetTabFocus(bool on_contents) {
  if (back_button_)
    back_button_->SetSelected(false);
  if (speech_button_)
    speech_button_->SetSelected(false);
  if (close_button_)
    close_button_->SetSelected(false);
  focused_view_ = on_contents ? FOCUS_CONTENTS_VIEW : FOCUS_SEARCH_BOX;
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
  if (!is_fullscreen_app_list_enabled_)
    return;

  search_icon_->SetVisible(!show_back_button);
  back_button_->SetVisible(show_back_button);
  content_container_->Layout();
}

void SearchBoxView::SetSearchBoxActive(bool active) {
  if (!is_fullscreen_app_list_enabled_)
    return;

  if (active == is_search_box_active_)
    return;

  is_search_box_active_ = active;
  UpdateSearchIcon();
  search_box_->set_placeholder_text_draw_flags(
      active ? gfx::Canvas::TEXT_ALIGN_LEFT : gfx::Canvas::TEXT_ALIGN_CENTER);
  search_box_->set_placeholder_text_color(active ? kZeroQuerySearchboxColor
                                                 : search_box_color_);
  search_box_->SetCursorEnabled(active);
  search_box_->SchedulePaint();

  if (speech_button_)
    speech_button_->SetVisible(!active);
  close_button_->SetVisible(active);
  if (focused_view_ != FOCUS_CONTENTS_VIEW)
    ResetTabFocus(false);
  content_container_->Layout();
}

void SearchBoxView::HandleSearchBoxEvent(ui::LocatedEvent* located_event) {
  if (!is_fullscreen_app_list_enabled_)
    return;

  if (located_event->type() == ui::ET_MOUSEWHEEL) {
    if (!app_list_view_->HandleScroll(located_event))
      return;

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
  if (!is_fullscreen_app_list_enabled_)
    return false;

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
  if (speech_button_)
    speech_button_->SetEnabled(enabled());
  if (close_button_)
    close_button_->SetEnabled(enabled());
}

const char* SearchBoxView::GetClassName() const {
  return "SearchBoxView";
}

void SearchBoxView::OnGestureEvent(ui::GestureEvent* event) {
  if (!is_fullscreen_app_list_enabled_)
    return;

  HandleSearchBoxEvent(event);
}

void SearchBoxView::OnMouseEvent(ui::MouseEvent* event) {
  HandleSearchBoxEvent(event);
}

int SearchBoxView::GetSearchBoxBorderCornerRadiusForState(
    AppListModel::State state) {
  if (state == AppListModel::STATE_SEARCH_RESULTS)
    return kSearchBoxBorderCornerRadiusSearchResult;
  return kSearchBoxBorderCornerRadiusFullscreen;
}

SkColor SearchBoxView::GetBackgroundColorForState(
    AppListModel::State state) const {
  if (state == AppListModel::STATE_SEARCH_RESULTS)
    return kSearchBoxBackgroundDefault;
  return background_color_;
}

void SearchBoxView::UpdateBackground(double progress,
                                     AppListModel::State current_state,
                                     AppListModel::State target_state) {
  SearchBoxBackground* background =
      static_cast<SearchBoxBackground*>(content_container_->background());
  background->set_corner_radius(gfx::Tween::LinearIntValueBetween(
      progress, GetSearchBoxBorderCornerRadiusForState(current_state),
      GetSearchBoxBorderCornerRadiusForState(target_state)));
  const SkColor color = gfx::Tween::ColorValueBetween(
      progress, GetBackgroundColorForState(current_state),
      GetBackgroundColorForState(target_state));
  background->set_color(color);
  search_box_->SetBackgroundColor(color);
}

void SearchBoxView::ButtonPressed(views::Button* sender,
                                  const ui::Event& event) {
  if (back_button_ && sender == back_button_) {
    delegate_->BackButtonPressed();
  } else if (speech_button_ && sender == speech_button_) {
    view_delegate_->StartSpeechRecognition();
  } else if (close_button_ && sender == close_button_) {
    ClearSearch();
    app_list_view_->SetStateFromSearchBoxView(true);
  } else {
    NOTREACHED();
  }
}

void SearchBoxView::UpdateModel() {
  // Temporarily remove from observer to ignore notifications caused by us.
  model_->search_box()->RemoveObserver(this);
  model_->search_box()->Update(search_box_->text(), false);
  model_->search_box()->SetSelectionModel(search_box_->GetSelectionModel());
  model_->search_box()->AddObserver(this);
}

void SearchBoxView::NotifyQueryChanged() {
  DCHECK(delegate_);
  delegate_->QueryChanged(this);
}

void SearchBoxView::ContentsChanged(views::Textfield* sender,
                                    const base::string16& new_contents) {
  UpdateModel();
  view_delegate_->AutoLaunchCanceled();
  NotifyQueryChanged();
  if (is_fullscreen_app_list_enabled_) {
    SetSearchBoxActive(!search_box_->text().empty());
    // If the query is only whitespace, don't transition the AppListView state.
    base::string16 trimmed_query = search_box_->text();
    base::TrimWhitespace(search_box_->text(), base::TrimPositions::TRIM_ALL,
                         &trimmed_query);
    app_list_view_->SetStateFromSearchBoxView(trimmed_query.empty());
  }
}

bool SearchBoxView::HandleKeyEvent(views::Textfield* sender,
                                   const ui::KeyEvent& key_event) {
  if (key_event.type() == ui::ET_KEY_PRESSED) {
    if (key_event.key_code() == ui::VKEY_TAB &&
        focused_view_ != FOCUS_CONTENTS_VIEW &&
        MoveTabFocus(key_event.IsShiftDown()))
      return true;

    if (focused_view_ == FOCUS_BACK_BUTTON && back_button_ &&
        back_button_->OnKeyPressed(key_event))
      return true;

    if (focused_view_ == FOCUS_MIC_BUTTON && speech_button_ &&
        speech_button_->OnKeyPressed(key_event))
      return true;

    if (focused_view_ == FOCUS_CLOSE_BUTTON && close_button_ &&
        close_button_->OnKeyPressed(key_event))
      return true;

    const bool handled = contents_view_ && contents_view_->visible() &&
                         contents_view_->OnKeyPressed(key_event);

    // Arrow keys may have selected an item.  If they did, move focus off
    // buttons.
    // If they didn't, we still select the first search item, in case they're
    // moving the caret through typed search text.  The UP arrow never moves
    // focus from text/buttons to app list/results, so ignore it.
    if (focused_view_ < FOCUS_CONTENTS_VIEW &&
        (key_event.key_code() == ui::VKEY_LEFT ||
         key_event.key_code() == ui::VKEY_RIGHT ||
         key_event.key_code() == ui::VKEY_DOWN)) {
      if (!handled)
        delegate_->SetSearchResultSelection(true);
      ResetTabFocus(handled);
    }
    return handled;
  }

  if (key_event.type() == ui::ET_KEY_RELEASED) {
    if (focused_view_ == FOCUS_BACK_BUTTON && back_button_ &&
        back_button_->OnKeyReleased(key_event))
      return true;

    if (focused_view_ == FOCUS_MIC_BUTTON && speech_button_ &&
        speech_button_->OnKeyReleased(key_event))
      return true;

    if (focused_view_ == FOCUS_CLOSE_BUTTON && close_button_ &&
        close_button_->OnKeyReleased(key_event))
      return true;

    return contents_view_ && contents_view_->visible() &&
           contents_view_->OnKeyReleased(key_event);
  }

  return false;
}

bool SearchBoxView::HandleMouseEvent(views::Textfield* sender,
                                     const ui::MouseEvent& mouse_event) {
  if (!is_fullscreen_app_list_enabled_)
    return false;

  if (mouse_event.type() == ui::ET_MOUSEWHEEL) {
    return app_list_view_->HandleScroll(&mouse_event);
  } else {
    return OnTextfieldEvent();
  }
}

bool SearchBoxView::HandleGestureEvent(views::Textfield* sender,
                                       const ui::GestureEvent& gesture_event) {
  return OnTextfieldEvent();
}

void SearchBoxView::SpeechRecognitionButtonPropChanged() {
  const SearchBoxModel::SpeechButtonProperty* speech_button_prop =
      model_->search_box()->speech_button();
  if (speech_button_prop) {
    if (!speech_button_) {
      speech_button_ = new SearchBoxImageButton(this);
      content_container_->AddChildView(speech_button_);
    }

    speech_button_->SetAccessibleName(speech_button_prop->accessible_name);
    if (is_fullscreen_app_list_enabled_) {
      speech_button_->SetImage(
          views::Button::STATE_NORMAL,
          gfx::CreateVectorIcon(kIcMicBlackIcon, kMicIconSize,
                                kDefaultSearchboxColor));
    }
    // TODO(warx): consider removing on_tooltip as it is not accessible due to
    // the overlap of speech UI.
    if (view_delegate_->GetSpeechUI()->state() ==
        SPEECH_RECOGNITION_HOTWORD_LISTENING) {
      if (!is_fullscreen_app_list_enabled_) {
        speech_button_->SetImage(views::Button::STATE_NORMAL,
                                 &speech_button_prop->on_icon);
      }
      speech_button_->SetTooltipText(speech_button_prop->on_tooltip);
    } else {
      if (!is_fullscreen_app_list_enabled_) {
        speech_button_->SetImage(views::Button::STATE_NORMAL,
                                 &speech_button_prop->off_icon);
      }
      speech_button_->SetTooltipText(speech_button_prop->off_tooltip);
    }
  } else {
    if (speech_button_) {
      // Deleting a view will detach it from its parent.
      delete speech_button_;
      speech_button_ = nullptr;
    }
  }
  Layout();
}

void SearchBoxView::HintTextChanged() {
  const app_list::SearchBoxModel* search_box = model_->search_box();
  search_box_->set_placeholder_text(search_box->hint_text());
  search_box_->SetAccessibleName(search_box->accessible_name());
}

void SearchBoxView::SelectionModelChanged() {
  search_box_->SelectSelectionModel(model_->search_box()->selection_model());
}

void SearchBoxView::Update() {
  search_box_->SetText(model_->search_box()->text());
  NotifyQueryChanged();
}

void SearchBoxView::WallpaperProminentColorsChanged() {
  if (!is_fullscreen_app_list_enabled_)
    return;

  const std::vector<SkColor> prominent_colors = GetWallpaperProminentColors();
  if (prominent_colors.empty())
    return;
  DCHECK_EQ(static_cast<size_t>(ColorProfileType::NUM_OF_COLOR_PROFILES),
            prominent_colors.size());

  SetSearchBoxColor(
      prominent_colors[static_cast<int>(ColorProfileType::DARK_MUTED)]);
  SetBackgroundColor(
      prominent_colors[static_cast<int>(ColorProfileType::LIGHT_VIBRANT)]);
  UpdateSearchIcon();
  speech_button_->SetImage(
      views::Button::STATE_NORMAL,
      gfx::CreateVectorIcon(kIcMicBlackIcon, kMicIconSize, search_box_color_));
  close_button_->SetImage(
      views::Button::STATE_NORMAL,
      gfx::CreateVectorIcon(kIcCloseIcon, kCloseIconSize, search_box_color_));
  search_box_->set_placeholder_text_color(search_box_color_);
  SearchBoxBackground* background =
      static_cast<SearchBoxBackground*>(content_container_->background());
  background->set_color(background_color_);
  search_box_->SetBackgroundColor(background_color_);
  SchedulePaint();
}

void SearchBoxView::OnSpeechRecognitionStateChanged(
    SpeechRecognitionState new_state) {
  SpeechRecognitionButtonPropChanged();
  SchedulePaint();
}

void SearchBoxView::UpdateSearchIcon() {
  const gfx::VectorIcon& google_icon =
      is_search_box_active() ? kIcGoogleColorIcon : kIcGoogleBlackIcon;
  const gfx::VectorIcon& icon = model_->search_engine_is_google()
                                    ? google_icon
                                    : kIcSearchEngineNotGoogleIcon;
  search_icon_->SetImage(
      gfx::CreateVectorIcon(icon, kSearchIconSize, search_box_color_));
}

const std::vector<SkColor>& SearchBoxView::GetWallpaperProminentColors() const {
  return model_->search_box()->wallpaper_prominent_colors();
}

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

}  // namespace app_list
