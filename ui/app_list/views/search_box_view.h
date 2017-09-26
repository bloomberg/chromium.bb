// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_VIEWS_SEARCH_BOX_VIEW_H_
#define UI_APP_LIST_VIEWS_SEARCH_BOX_VIEW_H_

#include <string>

#include "base/macros.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/app_list_model.h"
#include "ui/app_list/app_list_view_delegate_observer.h"
#include "ui/app_list/search_box_model_observer.h"
#include "ui/app_list/speech_ui_model_observer.h"
#include "ui/gfx/shadow_value.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/view.h"

namespace views {
class BoxLayout;
class ImageView;
class Textfield;
}  // namespace views

namespace app_list {

// Possible locations for partial keyboard focus (but note that the search
// box always handles typing).
enum SearchBoxFocus {
  FOCUS_NONE,           // No focus
  FOCUS_BACK_BUTTON,    // Back button, only responds to ENTER
  FOCUS_SEARCH_BOX,     // Nothing else has partial focus
  FOCUS_MIC_BUTTON,     // Mic button, only responds to ENTER
  FOCUS_CLOSE_BUTTON,   // Close button, only responds to ENTER
  FOCUS_CONTENTS_VIEW,  // Something outside the SearchBox is selected
};

class AppListView;
class AppListViewDelegate;
class SearchBoxModel;
class SearchBoxViewDelegate;
class SearchBoxBackground;
class SearchBoxImageButton;

// SearchBoxView consists of an icon and a Textfield. SearchBoxModel is its data
// model that controls what icon to display, what placeholder text to use for
// Textfield. The text and selection model part could be set to change the
// contents and selection model of the Textfield.
class APP_LIST_EXPORT SearchBoxView : public views::View,
                                      public views::TextfieldController,
                                      public views::ButtonListener,
                                      public SearchBoxModelObserver,
                                      public SpeechUIModelObserver,
                                      public AppListViewDelegateObserver {
 public:
  SearchBoxView(SearchBoxViewDelegate* delegate,
                AppListViewDelegate* view_delegate,
                AppListView* app_list_view = nullptr);
  ~SearchBoxView() override;

  void ModelChanged();
  bool HasSearch() const;
  void ClearSearch();

  // Sets the shadow border of the search box.
  void SetShadow(const gfx::ShadowValue& shadow);

  // Returns the bounds to use for the view (including the shadow) given the
  // desired bounds of the search box contents.
  gfx::Rect GetViewBoundsForSearchBoxContentsBounds(
      const gfx::Rect& rect) const;

  views::ImageButton* back_button();
  views::ImageButton* close_button();
  views::Textfield* search_box() { return search_box_; }

  void set_contents_view(views::View* contents_view) {
    contents_view_ = contents_view;
  }

  // Moves focus in response to arrow key.
  bool MoveArrowFocus(const ui::KeyEvent& event);

  // Moves focus forward/backwards in response to TAB.
  bool MoveTabFocus(bool move_backwards);

  // Moves focus to contents or SearchBox and unselects buttons.
  void ResetTabFocus(bool on_contents);

  // Sets voice label for Back button depending on whether a folder is open.
  void SetBackButtonLabel(bool folder);

  // Swaps the google icon with the back button.
  void ShowBackOrGoogleIcon(bool show_back_button);

  // Setting the search box active left aligns the placeholder text, changes
  // the color of the placeholder text, and enables cursor blink. Setting the
  // search box inactive center aligns the placeholder text, sets the color, and
  // disables cursor blink.
  void SetSearchBoxActive(bool active);

  // Shows/hides the virtual keyboard if the search box is active.
  void UpdateKeyboardVisibility();

  // Detects |ET_MOUSE_PRESSED| and |ET_GESTURE_TAP| events on the white
  // background of the search box.
  void HandleSearchBoxEvent(ui::LocatedEvent* located_event);

  // Handles Gesture and Mouse Events sent from |search_box_|.
  bool OnTextfieldEvent();

  // Overridden from views::View:
  bool OnMouseWheel(const ui::MouseWheelEvent& event) override;
  void OnEnabledChanged() override;
  const char* GetClassName() const override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnKeyEvent(ui::KeyEvent* evetn) override;

  // Overridden from views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // Updates the search box's background corner radius and color based on the
  // state of AppListModel.
  void UpdateBackground(double progress,
                        AppListModel::State current_state,
                        AppListModel::State target_state);

  // Updates the search box's layout based on the state of AppListModel.
  void UpdateLayout(double progress,
                    AppListModel::State current_state,
                    AppListModel::State target_state);

  // Called when tablet mode starts and ends.
  void OnTabletModeChanged(bool started);

  // Returns background border corner radius in the given state.
  int GetSearchBoxBorderCornerRadiusForState(AppListModel::State state) const;

  // Returns background color for the given state.
  SkColor GetBackgroundColorForState(AppListModel::State state) const;

  // Updates the opacity of the searchbox.
  void UpdateOpacity();

  // Used only in the tests to get the current search icon.
  views::ImageView* get_search_icon_for_test() { return search_icon_; }

  // Used only in the tests to get the current focused view.
  SearchBoxFocus get_focused_view_for_test() const { return focused_view_; }

  // Whether the search box is active.
  bool is_search_box_active() const { return is_search_box_active_; }

  // Whether the key event is an arrow up/down/left/right.
  // TODO(weidongg): move this function to utility class.
  static bool IsArrowKey(const ui::KeyEvent& event);

  // Returns selected view in contents view.
  views::View* GetSelectedViewInContentsView() const;

  bool selected() { return selected_; }
  void SetSelected(bool selected);

 private:
  // Updates model text and selection model with current Textfield info.
  void UpdateModel();

  // Fires query change notification.
  void NotifyQueryChanged();

  // Updates the search icon.
  void UpdateSearchIcon();

  // Gets the wallpaper prominent colors.
  void GetWallpaperProminentColors(std::vector<SkColor>* colors);

  // Sets the background color.
  void SetBackgroundColor(SkColor light_vibrant);

  // Sets the search box color.
  void SetSearchBoxColor(SkColor color);

  // Updates the search box's background color.
  void UpdateBackgroundColor(SkColor color);

  // Updates the visibility of close button.
  void UpdateCloseButtonVisisbility();

  // Gets the search box background.
  SearchBoxBackground* GetSearchBoxBackground() const;

  // Whether the trimmed query in the search box is empty.
  bool IsSearchBoxTrimmedQueryEmpty() const;

  // Overridden from views::TextfieldController:
  void ContentsChanged(views::Textfield* sender,
                       const base::string16& new_contents) override;
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override;
  bool HandleMouseEvent(views::Textfield* sender,
                        const ui::MouseEvent& mouse_event) override;

  bool HandleGestureEvent(views::Textfield* sender,
                          const ui::GestureEvent& gesture_event) override;

  // Overridden from SearchBoxModelObserver:
  void SpeechRecognitionButtonPropChanged() override;
  void HintTextChanged() override;
  void SelectionModelChanged() override;
  void Update() override;

  // Overridden from SpeechUIModelObserver:
  void OnSpeechRecognitionStateChanged(
      SpeechRecognitionState new_state) override;

  // Overridden from AppListViewDelegateObserver:
  void OnWallpaperColorsChanged() override;

  void SetDefaultBorder();

  SearchBoxViewDelegate* delegate_;     // Not owned.
  AppListViewDelegate* view_delegate_;  // Not owned.
  AppListModel* model_ = nullptr;       // Owned by the profile-keyed service.

  // Owned by views hierarchy.
  views::View* content_container_;
  views::ImageView* search_icon_ = nullptr;
  SearchBoxImageButton* back_button_ = nullptr;
  SearchBoxImageButton* speech_button_ = nullptr;
  SearchBoxImageButton* close_button_ = nullptr;
  views::Textfield* search_box_;
  views::View* search_box_right_space_ = nullptr;
  views::View* contents_view_ = nullptr;
  app_list::AppListView* app_list_view_;

  // Owned by |content_container_|. It is deleted when the view is deleted.
  views::BoxLayout* box_layout_ = nullptr;

  // Whether the fullscreen app list feature is enabled.
  const bool is_fullscreen_app_list_enabled_;

  // Whether the app list focus is enabled.
  const bool is_app_list_focus_enabled_;

  SearchBoxFocus focused_view_;  // Which element has TAB'd focus.

  // Whether the search box is active.
  bool is_search_box_active_ = false;
  // Whether tablet mode is active.
  bool is_tablet_mode_ = false;
  // The current background color.
  SkColor background_color_ = kSearchBoxBackgroundDefault;
  // The current search box color.
  SkColor search_box_color_ = kDefaultSearchboxColor;

  // Whether the search box is selected.
  bool selected_ = false;

  DISALLOW_COPY_AND_ASSIGN(SearchBoxView);
};

}  // namespace app_list

#endif  // UI_APP_LIST_VIEWS_SEARCH_BOX_VIEW_H_
