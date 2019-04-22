// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/omnibox_edit_controller.h"

#include "components/omnibox/browser/location_bar_model.h"

void OmniboxEditController::OnAutocompleteAccept(
    const GURL& destination_url,
    TemplateURLRef::PostContent* post_content,
    WindowOpenDisposition disposition,
    ui::PageTransition transition,
    AutocompleteMatchType::Type type,
    base::TimeTicks match_selection_timestamp) {
  destination_url_ = destination_url;
  post_content_ = post_content;
  disposition_ = disposition;
  transition_ = transition;
  match_selection_timestamp_ = match_selection_timestamp;
}

void OmniboxEditController::OnInputInProgress(bool in_progress) {}

void OmniboxEditController::OnChanged() {}

void OmniboxEditController::OnPopupVisibilityChanged() {}

OmniboxEditController::OmniboxEditController()
    : disposition_(WindowOpenDisposition::CURRENT_TAB),
      transition_(ui::PageTransitionFromInt(
          ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR)) {}

OmniboxEditController::~OmniboxEditController() {}
