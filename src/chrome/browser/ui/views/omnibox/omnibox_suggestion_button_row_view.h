// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_SUGGESTION_BUTTON_ROW_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_SUGGESTION_BUTTON_ROW_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/omnibox_popup_selection.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

class OmniboxEditModel;
class OmniboxPopupContentsView;
class OmniboxSuggestionRowButton;

namespace views {
class Button;
}

// A view to contain the button row within a result view.
class OmniboxSuggestionButtonRowView : public views::View {
 public:
  METADATA_HEADER(OmniboxSuggestionButtonRowView);
  explicit OmniboxSuggestionButtonRowView(OmniboxPopupContentsView* view,
                                          OmniboxEditModel* model,
                                          int model_index);
  OmniboxSuggestionButtonRowView(const OmniboxSuggestionButtonRowView&) =
      delete;
  OmniboxSuggestionButtonRowView& operator=(
      const OmniboxSuggestionButtonRowView&) = delete;
  ~OmniboxSuggestionButtonRowView() override;

  // Called when results background color is refreshed.
  void OnOmniboxBackgroundChange(SkColor omnibox_bg_color);

  // Updates the suggestion row buttons based on the model.
  void UpdateFromModel();

  views::Button* GetActiveButton() const;

 private:
  // Digs into the model with index to get the match for owning result view.
  const AutocompleteMatch& match() const;

  void SetPillButtonVisibility(OmniboxSuggestionRowButton* button,
                               OmniboxPopupSelection::LineState state);

  void ButtonPressed(OmniboxPopupSelection::LineState state,
                     const ui::Event& event);

  const raw_ptr<OmniboxPopupContentsView> popup_contents_view_;
  raw_ptr<OmniboxEditModel> model_;
  size_t const model_index_;

  raw_ptr<OmniboxSuggestionRowButton> keyword_button_ = nullptr;
  raw_ptr<OmniboxSuggestionRowButton> pedal_button_ = nullptr;
  raw_ptr<OmniboxSuggestionRowButton> tab_switch_button_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_SUGGESTION_BUTTON_ROW_VIEW_H_
