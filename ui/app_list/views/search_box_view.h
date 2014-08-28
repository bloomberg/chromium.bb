// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_VIEWS_SEARCH_BOX_VIEW_H_
#define UI_APP_LIST_VIEWS_SEARCH_BOX_VIEW_H_

#include <string>

#include "ui/app_list/search_box_model_observer.h"
#include "ui/app_list/speech_ui_model_observer.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/menu_button_listener.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/view.h"

namespace views {
class ImageView;
class MenuButton;
class Textfield;
}  // namespace views

namespace app_list {

class AppListMenuViews;
class AppListModel;
class AppListViewDelegate;
class SearchBoxModel;
class SearchBoxViewDelegate;

// SearchBoxView consists of an icon and a Textfield. SearchBoxModel is its data
// model that controls what icon to display, what placeholder text to use for
// Textfield. The text and selection model part could be set to change the
// contents and selection model of the Textfield.
class APP_LIST_EXPORT SearchBoxView : public views::View,
                                      public views::TextfieldController,
                                      public views::ButtonListener,
                                      public views::MenuButtonListener,
                                      public SearchBoxModelObserver,
                                      public SpeechUIModelObserver {
 public:
  SearchBoxView(SearchBoxViewDelegate* delegate,
                AppListViewDelegate* view_delegate);
  virtual ~SearchBoxView();

  void ModelChanged();
  bool HasSearch() const;
  void ClearSearch();
  void InvalidateMenu();

  views::Textfield* search_box() { return search_box_; }

  void set_contents_view(views::View* contents_view) {
    contents_view_ = contents_view;
  }

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize() const OVERRIDE;
  virtual bool OnMouseWheel(const ui::MouseWheelEvent& event) OVERRIDE;

 private:
  // Updates model text and selection model with current Textfield info.
  void UpdateModel();

  // Fires query change notification.
  void NotifyQueryChanged();

  // Overridden from views::TextfieldController:
  virtual void ContentsChanged(views::Textfield* sender,
                               const base::string16& new_contents) OVERRIDE;
  virtual bool HandleKeyEvent(views::Textfield* sender,
                              const ui::KeyEvent& key_event) OVERRIDE;

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  // Overridden from views::MenuButtonListener:
  virtual void OnMenuButtonClicked(View* source,
                                   const gfx::Point& point) OVERRIDE;

  // Overridden from SearchBoxModelObserver:
  virtual void IconChanged() OVERRIDE;
  virtual void SpeechRecognitionButtonPropChanged() OVERRIDE;
  virtual void HintTextChanged() OVERRIDE;
  virtual void SelectionModelChanged() OVERRIDE;
  virtual void TextChanged() OVERRIDE;

  // Overridden from SpeechUIModelObserver:
  virtual void OnSpeechRecognitionStateChanged(
      SpeechRecognitionState new_state) OVERRIDE;

  SearchBoxViewDelegate* delegate_;  // Not owned.
  AppListViewDelegate* view_delegate_;  // Not owned.
  AppListModel* model_;  // Owned by the profile-keyed service.

  scoped_ptr<AppListMenuViews> menu_;

  views::ImageView* icon_view_;  // Owned by views hierarchy.
  views::ImageButton* speech_button_;  // Owned by views hierarchy.
  views::MenuButton* menu_button_;  // Owned by views hierarchy.
  views::Textfield* search_box_;  // Owned by views hierarchy.
  views::View* contents_view_;  // Owned by views hierarchy.

  DISALLOW_COPY_AND_ASSIGN(SearchBoxView);
};

}  // namespace app_list

#endif  // UI_APP_LIST_VIEWS_SEARCH_BOX_VIEW_H_
