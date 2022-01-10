// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_UI_CANDIDATE_WINDOW_VIEW_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_UI_CANDIDATE_WINDOW_VIEW_H_

#include <memory>

#include "ui/base/ime/candidate_window.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/chromeos/ui_chromeos_export.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/metadata/view_factory.h"

namespace ui {
namespace ime {

class CandidateView;
class InformationTextArea;

// CandidateWindowView is the main container of the candidate window UI.
class UI_CHROMEOS_EXPORT CandidateWindowView
    : public views::BubbleDialogDelegateView {
 public:
  METADATA_HEADER(CandidateWindowView);
  // The object can be monitored by the observer.
  class Observer {
   public:
    virtual ~Observer() {}
    // The function is called when a candidate is committed.
    virtual void OnCandidateCommitted(int index) = 0;
  };

  explicit CandidateWindowView(gfx::NativeView parent);
  CandidateWindowView(const CandidateWindowView&) = delete;
  CandidateWindowView& operator=(const CandidateWindowView&) = delete;
  ~CandidateWindowView() override;
  views::Widget* InitWidget();

  // views::BubbleDialogDelegateView:
  void OnThemeChanged() override;

  // Adds the given observer. The ownership is not transferred.
  void AddObserver(Observer* observer) { observers_.AddObserver(observer); }

  // Removes the given observer.
  void RemoveObserver(Observer* observer) {
    observers_.RemoveObserver(observer);
  }

  // Hides the lookup table.
  void HideLookupTable();

  // Hides the auxiliary text.
  void HideAuxiliaryText();

  // Hides the preedit text.
  void HidePreeditText();

  // Shows the lookup table.
  void ShowLookupTable();

  // Shows the auxiliary text.
  void ShowAuxiliaryText();

  // Shows the preedit text.
  void ShowPreeditText();

  // Updates the preedit text.
  void UpdatePreeditText(const std::u16string& text);

  // Updates candidates of the candidate window from |candidate_window|.
  // Candidates are arranged per |orientation|.
  void UpdateCandidates(const ui::CandidateWindow& candidate_window);

  void SetCursorBounds(const gfx::Rect& cursor_bounds,
                       const gfx::Rect& composition_head);

 private:
  friend class CandidateWindowViewTest;

  void SelectCandidateAt(int index_in_page);
  void UpdateVisibility();

  // Initializes the candidate views if needed.
  void MaybeInitializeCandidateViews(
      const ui::CandidateWindow& candidate_window);

  void CandidateViewPressed(int index);

  // The candidate window data model.
  ui::CandidateWindow candidate_window_;

  // The index in the current page of the candidate currently being selected.
  int selected_candidate_index_in_page_;

  // The observers of the object.
  base::ObserverList<Observer>::Unchecked observers_;

  // Views created in the class will be part of tree of |this|, so these
  // child views will be deleted when |this| is deleted.
  InformationTextArea* auxiliary_text_;
  InformationTextArea* preedit_;
  views::View* candidate_area_;

  // The candidate views are used for rendering candidates.
  std::vector<CandidateView*> candidate_views_;

  // Current columns size in |candidate_area_|.
  gfx::Size previous_shortcut_column_size_;
  gfx::Size previous_candidate_column_size_;
  gfx::Size previous_annotation_column_size_;

  // The last cursor bounds.
  gfx::Rect cursor_bounds_;

  // The last composition head bounds.
  gfx::Rect composition_head_bounds_;

  // True if the candidate window should be shown with aligning with composition
  // text as opposed to the cursor.
  bool should_show_at_composition_head_;

  // True if the candidate window should be shown on the upper side of
  // composition text.
  bool should_show_upper_side_;

  // True if the candidate window was open.  This is used to determine when to
  // send OnCandidateWindowOpened and OnCandidateWindowClosed events.
  bool was_candidate_window_open_;
};

BEGIN_VIEW_BUILDER(UI_CHROMEOS_EXPORT,
                   CandidateWindowView,
                   views::BubbleDialogDelegateView)
END_VIEW_BUILDER

}  // namespace ime
}  // namespace ui

DEFINE_VIEW_BUILDER(UI_CHROMEOS_EXPORT, ui::ime::CandidateWindowView)

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_UI_CANDIDATE_WINDOW_VIEW_H_
