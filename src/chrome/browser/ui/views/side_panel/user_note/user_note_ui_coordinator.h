// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_USER_NOTE_USER_NOTE_UI_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_USER_NOTE_USER_NOTE_UI_COORDINATOR_H_

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/user_notes/interfaces/user_notes_ui.h"
#include "ui/views/view.h"

class UserNoteUICoordinator : public user_notes::UserNotesUI,
                              public TabStripModelObserver {
 public:
  explicit UserNoteUICoordinator(Browser* browser);

  UserNoteUICoordinator(const UserNoteUICoordinator&) = delete;
  UserNoteUICoordinator& operator=(const UserNoteUICoordinator&) = delete;
  ~UserNoteUICoordinator() override;

  // UserNoteUI overrides
  void FocusNote(const std::string& guid) override;
  void StartNoteCreation(const std::string& guid, gfx::Rect bounds) override;
  void Invalidate() override;
  void Show() override;

  // TabStripModelObserver overrides
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  std::unique_ptr<views::View> CreateUserNotesView();

 private:
  Browser* browser_;
  views::View* scroll_contents_view_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_USER_NOTE_USER_NOTE_UI_COORDINATOR_H_
