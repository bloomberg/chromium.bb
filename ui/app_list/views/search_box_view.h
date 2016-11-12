// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_VIEWS_SEARCH_BOX_VIEW_H_
#define UI_APP_LIST_VIEWS_SEARCH_BOX_VIEW_H_

#include <string>

#include "base/macros.h"
#include "ui/app_list/search_box_model_observer.h"
#include "ui/app_list/speech_ui_model_observer.h"
#include "ui/gfx/shadow_value.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/view.h"

namespace views {
class Textfield;
}  // namespace views

namespace app_list {

// Possible locations for partial keyboard focus (but note that the search
// box always handles typing).
enum SearchBoxFocus {
  FOCUS_BACK_BUTTON,    // Back button, only responds to ENTER
  FOCUS_SEARCH_BOX,     // Nothing else has partial focus
  FOCUS_MIC_BUTTON,     // Mic button, only responds to ENTER
  FOCUS_CONTENTS_VIEW,  // Something outside the SearchBox is selected
};

class AppListModel;
class AppListViewDelegate;
class SearchBoxModel;
class SearchBoxViewDelegate;
class SearchBoxImageButton;

// SearchBoxView consists of an icon and a Textfield. SearchBoxModel is its data
// model that controls what icon to display, what placeholder text to use for
// Textfield. The text and selection model part could be set to change the
// contents and selection model of the Textfield.
class APP_LIST_EXPORT SearchBoxView : public views::View,
                                      public views::TextfieldController,
                                      public views::ButtonListener,
                                      public SearchBoxModelObserver,
                                      public SpeechUIModelObserver {
 public:
  SearchBoxView(SearchBoxViewDelegate* delegate,
                AppListViewDelegate* view_delegate);
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
  views::Textfield* search_box() { return search_box_; }

  void set_contents_view(views::View* contents_view) {
    contents_view_ = contents_view;
  }

  // Moves focus forward/backwards in response to TAB.
  bool MoveTabFocus(bool move_backwards);

  // Moves focus to contents or SearchBox and unselects buttons.
  void ResetTabFocus(bool on_contents);

  // Sets voice label for Back button depending on whether a folder is open.
  void SetBackButtonLabel(bool folder);

  // Overridden from views::View:
  gfx::Size GetPreferredSize() const override;
  bool OnMouseWheel(const ui::MouseWheelEvent& event) override;
  void OnEnabledChanged() override;

 private:
  // Updates model text and selection model with current Textfield info.
  void UpdateModel();

  // Fires query change notification.
  void NotifyQueryChanged();

  // Overridden from views::TextfieldController:
  void ContentsChanged(views::Textfield* sender,
                       const base::string16& new_contents) override;
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override;

  // Overridden from views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // Overridden from SearchBoxModelObserver:
  void SpeechRecognitionButtonPropChanged() override;
  void HintTextChanged() override;
  void SelectionModelChanged() override;
  void TextChanged() override;

  // Overridden from SpeechUIModelObserver:
  void OnSpeechRecognitionStateChanged(
      SpeechRecognitionState new_state) override;

  SearchBoxViewDelegate* delegate_;  // Not owned.
  AppListViewDelegate* view_delegate_;  // Not owned.
  AppListModel* model_;  // Owned by the profile-keyed service.

  views::View* content_container_;     // Owned by views hierarchy.
  SearchBoxImageButton* back_button_;    // Owned by views hierarchy.
  SearchBoxImageButton* speech_button_;  // Owned by views hierarchy.
  views::Textfield* search_box_;  // Owned by views hierarchy.
  views::View* contents_view_;  // Owned by views hierarchy.

  SearchBoxFocus focused_view_;  // Which element has TAB'd focus.

  DISALLOW_COPY_AND_ASSIGN(SearchBoxView);
};

}  // namespace app_list

#endif  // UI_APP_LIST_VIEWS_SEARCH_BOX_VIEW_H_
