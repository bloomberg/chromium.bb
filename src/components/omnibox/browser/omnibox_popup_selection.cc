// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/omnibox_popup_selection.h"

#include <algorithm>

#include "build/build_config.h"
#include "components/omnibox/browser/omnibox_field_trial.h"

const size_t OmniboxPopupSelection::kNoMatch = static_cast<size_t>(-1);

bool OmniboxPopupSelection::operator==(const OmniboxPopupSelection& b) const {
  return line == b.line && state == b.state;
}

bool OmniboxPopupSelection::operator!=(const OmniboxPopupSelection& b) const {
  return !operator==(b);
}

bool OmniboxPopupSelection::operator<(const OmniboxPopupSelection& b) const {
  if (line == b.line)
    return state < b.state;

  return line < b.line;
}

bool OmniboxPopupSelection::IsChangeToKeyword(
    OmniboxPopupSelection from) const {
  return state == KEYWORD_MODE && from.state != KEYWORD_MODE;
}

bool OmniboxPopupSelection::IsButtonFocused() const {
  return state != NORMAL && state != KEYWORD_MODE;
}

bool OmniboxPopupSelection::IsControlPresentOnMatch(
    const AutocompleteResult& result,
    PrefService* pref_service) const {
  if (line >= result.size()) {
    return false;
  }
  const auto& match = result.match_at(line);
  // Skip rows that are hidden because their header is collapsed, unless the
  // user is trying to focus the header itself (which is still shown).
  if (state != FOCUSED_BUTTON_HEADER && match.suggestion_group_id.has_value() &&
      pref_service &&
      result.IsSuggestionGroupIdHidden(pref_service,
                                       match.suggestion_group_id.value())) {
    return false;
  }

  switch (state) {
    case FOCUSED_BUTTON_HEADER: {
      // For the first match, if it a suggestion_group_id, then it has a header.
      if (line == 0)
        return match.suggestion_group_id.has_value();

      // Otherwise, we only show headers that are distinct from the previous
      // match's header.
      const auto& previous_match = result.match_at(line - 1);
      return match.suggestion_group_id.has_value() &&
             match.suggestion_group_id != previous_match.suggestion_group_id;
    }
    case NORMAL:
      return true;
    case KEYWORD_MODE:
      return match.associated_keyword != nullptr;
    case FOCUSED_BUTTON_TAB_SWITCH:
      // Buttons are suppressed for matches with an associated keyword, unless
      // dedicated button row is enabled.
      if (OmniboxFieldTrial::IsKeywordSearchButtonEnabled())
        return match.has_tab_match.value_or(false);
      else
        return match.has_tab_match.value_or(false) && !match.associated_keyword;
    case FOCUSED_BUTTON_ACTION:
      return match.action != nullptr;
    case FOCUSED_BUTTON_REMOVE_SUGGESTION:
      // Remove suggestion buttons are suppressed for matches with an associated
      // keyword, unless the feature that moves it to the button row is enabled.
      if (OmniboxFieldTrial::IsKeywordSearchButtonEnabled()) {
        return match.SupportsDeletion();
      } else {
        return !match.associated_keyword && match.SupportsDeletion();
      }
    default:
      break;
  }
  NOTREACHED();
  return false;
}

OmniboxPopupSelection OmniboxPopupSelection::GetNextSelection(
    const AutocompleteResult& result,
    PrefService* pref_service,
    Direction direction,
    Step step) const {
  if (result.empty()) {
    return *this;
  }

  // Implementing this was like a Google Interview Problem. It was always a
  // tough problem to handle all the cases, but has gotten much harder since
  // we can now hide whole rows from view by collapsing sections.
  //
  // The only sane thing to do is to first enumerate all available selections.
  // Other approaches I've tried all end up being a jungle of branching code.
  // It's not necessarily optimal to generate this list for each keypress, but
  // in practice it's only something like ~10 elements long, and makes the code
  // easy to reason about.
  std::vector<OmniboxPopupSelection> all_available_selections =
      GetAllAvailableSelectionsSorted(result, pref_service, direction, step);

  if (all_available_selections.empty()) {
    return *this;
  }

  // Handle the simple case of just getting the first or last element.
  if (step == kAllLines) {
    return direction == kForward ? all_available_selections.back()
                                 : all_available_selections.front();
  }

  if (direction == kForward) {
    // To go forward, we want to change to the first selection that's larger
    // than the current selection, and std::upper_bound() does just
    // that.
    const auto next = std::upper_bound(all_available_selections.begin(),
                                       all_available_selections.end(), *this);

    // If we can't find any selections larger than the current
    // selection, wrap.
    if (next == all_available_selections.end())
      return all_available_selections.front();

    // Normal case where we found the next selection.
    return *next;
  } else if (direction == kBackward) {
    // To go backwards, decrement one from std::lower_bound(), which finds the
    // current selection. I didn't use std::find() here, because
    // std::lower_bound() can gracefully handle the case where
    // selection is no longer within the list of available selections.
    const auto current =
        std::lower_bound(all_available_selections.begin(),
                         all_available_selections.end(), *this);

    // If the current selection is the first one, wrap.
    if (current == all_available_selections.begin()) {
      return all_available_selections.back();
    }

    // Decrement one from the current selection.
    return *(current - 1);
  }

  NOTREACHED();
  return *this;
}

// static
std::vector<OmniboxPopupSelection>
OmniboxPopupSelection::GetAllAvailableSelectionsSorted(
    const AutocompleteResult& result,
    PrefService* pref_service,
    Direction direction,
    Step step) {
  // First enumerate all the accessible states based on |direction| and |step|,
  // as well as enabled feature flags. This doesn't mean each match will have
  // all of these states - just that it's possible to get there, if available.
  std::vector<LineState> all_states;
  if (step == kWholeLine || step == kAllLines) {
    // In the case of whole-line stepping, only the NORMAL state is accessible.
    all_states.push_back(NORMAL);
  } else {
    // Arrow keys should never reach the header controls.
    if (step == kStateOrLine)
      all_states.push_back(FOCUSED_BUTTON_HEADER);

    all_states.push_back(NORMAL);

    // Keyword mode is accessible if the keyword search button is enabled. If
    // not, then keyword mode is only accessible by tabbing forward.
    if (OmniboxFieldTrial::IsKeywordSearchButtonEnabled() ||
        (direction == kForward && step == kStateOrLine)) {
      all_states.push_back(KEYWORD_MODE);
    }

    all_states.push_back(FOCUSED_BUTTON_TAB_SWITCH);
#if !defined(OS_ANDROID) && !defined(OS_IOS)
    all_states.push_back(FOCUSED_BUTTON_ACTION);
#endif
    all_states.push_back(FOCUSED_BUTTON_REMOVE_SUGGESTION);
  }
  DCHECK(std::is_sorted(all_states.begin(), all_states.end()))
      << "This algorithm depends on a sorted list of line states.";

  // Now, for each accessible line, add all the available line states to a list.
  std::vector<OmniboxPopupSelection> available_selections;
  {
    auto add_available_line_states_for_line = [&](size_t line) {
      for (LineState state : all_states) {
        OmniboxPopupSelection selection(line, state);
        if (selection.IsControlPresentOnMatch(result, pref_service)) {
          available_selections.push_back(selection);
        }
      }
    };

    for (size_t line = 0; line < result.size(); ++line) {
      add_available_line_states_for_line(line);
    }
  }
  DCHECK(
      std::is_sorted(available_selections.begin(), available_selections.end()))
      << "This algorithm depends on a sorted list of available selections.";
  return available_selections;
}
