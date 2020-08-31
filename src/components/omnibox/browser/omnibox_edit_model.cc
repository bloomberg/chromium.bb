// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/omnibox_edit_model.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/auto_reset.h"
#include "base/feature_list.h"
#include "base/format_macros.h"
#include "base/macros.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/dom_distiller/core/url_constants.h"
#include "components/dom_distiller/core/url_utils.h"
#include "components/navigation_metrics/navigation_metrics.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/history_url_provider.h"
#include "components/omnibox/browser/keyword_provider.h"
#include "components/omnibox/browser/location_bar_model.h"
#include "components/omnibox/browser/omnibox_client.h"
#include "components/omnibox/browser/omnibox_edit_controller.h"
#include "components/omnibox/browser/omnibox_event_global_tracker.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/omnibox_log.h"
#include "components/omnibox/browser/omnibox_navigation_observer.h"
#include "components/omnibox/browser/omnibox_pedal.h"
#include "components/omnibox/browser/omnibox_popup_model.h"
#include "components/omnibox/browser/omnibox_popup_view.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "components/omnibox/browser/search_provider.h"
#include "components/omnibox/browser/suggestion_answer.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/search_engines/template_url_service.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "ui/gfx/image/image.h"
#include "url/url_util.h"

using bookmarks::BookmarkModel;
using metrics::OmniboxEventProto;


// Helpers --------------------------------------------------------------------

namespace {

// Histogram name which counts the number of times that the user text is
// cleared.  IME users are sometimes in the situation that IME was
// unintentionally turned on and failed to input latin alphabets (ASCII
// characters) or the opposite case.  In that case, users may delete all
// the text and the user text gets cleared.  We'd like to measure how often
// this scenario happens.
//
// Note that since we don't currently correlate "text cleared" events with
// IME usage, this also captures many other cases where users clear the text;
// though it explicitly doesn't log deleting all the permanent text as
// the first action of an editing sequence (see comments in
// OnAfterPossibleChange()).
const char kOmniboxUserTextClearedHistogram[] = "Omnibox.UserTextCleared";

enum UserTextClearedType {
  OMNIBOX_USER_TEXT_CLEARED_BY_EDITING = 0,
  OMNIBOX_USER_TEXT_CLEARED_WITH_ESCAPE = 1,
  OMNIBOX_USER_TEXT_CLEARED_NUM_OF_ITEMS,
};

// Histogram name which counts the number of times the user enters
// keyword hint mode and via what method.  The possible values are listed
// in the metrics OmniboxEnteredKeywordMode2 enum which is defined in metrics
// enum XML file.
const char kEnteredKeywordModeHistogram[] = "Omnibox.EnteredKeywordMode2";

// Histogram name which counts the number of milliseconds a user takes
// between focusing and editing the omnibox.
const char kFocusToEditTimeHistogram[] = "Omnibox.FocusToEditTime";

// Histogram name which counts the number of milliseconds a user takes
// between focusing and opening an omnibox match.
const char kFocusToOpenTimeHistogram[] =
    "Omnibox.FocusToOpenTimeAnyPopupState3";

void EmitKeywordHistogram(
    OmniboxEventProto::KeywordModeEntryMethod entry_method) {
  UMA_HISTOGRAM_ENUMERATION(
      kEnteredKeywordModeHistogram, static_cast<int>(entry_method),
      static_cast<int>(OmniboxEventProto::KeywordModeEntryMethod_MAX + 1));
}

}  // namespace


// OmniboxEditModel::State ----------------------------------------------------

OmniboxEditModel::State::State(
    bool user_input_in_progress,
    const base::string16& user_text,
    const base::string16& keyword,
    bool is_keyword_hint,
    OmniboxEventProto::KeywordModeEntryMethod keyword_mode_entry_method,
    OmniboxFocusState focus_state,
    OmniboxFocusSource focus_source,
    const AutocompleteInput& autocomplete_input)
    : user_input_in_progress(user_input_in_progress),
      user_text(user_text),
      keyword(keyword),
      is_keyword_hint(is_keyword_hint),
      keyword_mode_entry_method(keyword_mode_entry_method),
      focus_state(focus_state),
      focus_source(focus_source),
      autocomplete_input(autocomplete_input) {}

OmniboxEditModel::State::State(const State& other) = default;

OmniboxEditModel::State::~State() {
}


// OmniboxEditModel -----------------------------------------------------------

OmniboxEditModel::OmniboxEditModel(OmniboxView* view,
                                   OmniboxEditController* controller,
                                   std::unique_ptr<OmniboxClient> client)
    : client_(std::move(client)),
      view_(view),
      controller_(controller),
      focus_state_(OMNIBOX_FOCUS_NONE),
      user_input_in_progress_(false),
      user_input_since_focus_(true),
      just_deleted_text_(false),
      has_temporary_text_(false),
      paste_state_(NONE),
      control_key_state_(UP),
      is_keyword_hint_(false),
      keyword_mode_entry_method_(OmniboxEventProto::INVALID),
      in_revert_(false),
      allow_exact_keyword_match_(false) {
  omnibox_controller_.reset(new OmniboxController(this, client_.get()));
}

OmniboxEditModel::~OmniboxEditModel() {
}

metrics::OmniboxEventProto::PageClassification
OmniboxEditModel::GetPageClassification() const {
  return controller()->GetLocationBarModel()->GetPageClassification(
      focus_source_);
}

OmniboxEditModel::State OmniboxEditModel::GetStateForTabSwitch() const {
  // NOTE: it's important this doesn't attempt to access any state that
  // may come from the active WebContents. At the time this is called, the
  // active WebContents has already changed.

  // Like typing, switching tabs "accepts" the temporary text as the user
  // text, because it makes little sense to have temporary text when the
  // popup is closed.
  base::string16 user_text;
  if (user_input_in_progress_) {
    const base::string16 display_text = view_->GetText();
    if (!MaybePrependKeyword(display_text).empty())
      user_text = display_text;
    // Else case is user deleted all the text. The expectation (which matches
    // other browsers) is when the user restores the state a revert happens as
    // well as a select all. The revert shouldn't be done here, as at the time
    // this is called a revert would revert to the url of the newly activated
    // tab (because at the time this is called, the WebContents has already
    // changed). By leaving the |user_text| empty downstream code is able to
    // detect this and select all.
  } else {
    user_text = user_text_;
  }
  return State(user_input_in_progress_, user_text, keyword_, is_keyword_hint_,
               keyword_mode_entry_method_, focus_state_, focus_source_, input_);
}

void OmniboxEditModel::RestoreState(const State* state) {
  // We need to update the permanent display texts correctly and revert the
  // view regardless of whether there is saved state.
  ResetDisplayTexts();

  view_->RevertAll();
  // Restore the autocomplete controller's input, or clear it if this is a new
  // tab.
  input_ = state ? state->autocomplete_input : AutocompleteInput();
  if (!state)
    return;

  // The tab-management system saves the last-focused control for each tab and
  // restores it. That operation also updates this edit model's focus_state_
  // if necessary. This occurs before we reach this point in the code.
  //
  // The only reason we need to separately save and restore our focus state is
  // to preserve our special "invisible focus" state used for the fakebox.
  //
  // However, in some circumstances (if the last-focused control was destroyed),
  // the Omnibox will be focused by default, and the edit model's saved state
  // may be invalid. We make a check to guard against that.
  bool saved_focus_state_invalid = focus_state_ == OMNIBOX_FOCUS_VISIBLE &&
                                   state->focus_state == OMNIBOX_FOCUS_NONE;
  if (!saved_focus_state_invalid) {
    SetFocusState(state->focus_state, OMNIBOX_FOCUS_CHANGE_TAB_SWITCH);
    focus_source_ = state->focus_source;
  }

  // Restore any user editing.
  if (state->user_input_in_progress) {
    // NOTE: Be sure to set keyword-related state AFTER invoking
    // SetUserText(), as SetUserText() clears the keyword state.
    if (!state->user_text.empty() || !state->keyword.empty())
      view_->SetUserText(state->user_text, false);
    keyword_ = state->keyword;
    is_keyword_hint_ = state->is_keyword_hint;
    keyword_mode_entry_method_ = state->keyword_mode_entry_method;
  } else if (!state->user_text.empty()) {
    // If the |user_input_in_progress| is false but we have |user_text|,
    // restore the |user_text| to the model and the view. It's likely unelided
    // text that the user has not made any modifications to.
    InternalSetUserText(state->user_text);

    // We let the View manage restoring the cursor position afterwards.
    view_->SetWindowTextAndCaretPos(state->user_text, 0, false, false);
  }
}

AutocompleteMatch OmniboxEditModel::CurrentMatch(
    GURL* alternate_nav_url) const {
  // If we have a valid match use it. Otherwise get one for the current text.
  AutocompleteMatch match = omnibox_controller_->current_match();
  if (!match.destination_url.is_valid()) {
    GetInfoForCurrentText(&match, alternate_nav_url);
  } else if (alternate_nav_url) {
    *alternate_nav_url = AutocompleteResult::ComputeAlternateNavUrl(
        input_, match);
  }
  return match;
}

bool OmniboxEditModel::ResetDisplayTexts() {
  const base::string16 old_display_text = GetPermanentDisplayText();

  LocationBarModel* location_bar_model = controller()->GetLocationBarModel();
  url_for_editing_ = location_bar_model->GetFormattedFullURL();

  if (location_bar_model->GetDisplaySearchTerms(&display_text_)) {
    // The search query has been inserted into |display_text_|.
    DCHECK(!display_text_.empty());
  } else {
#if defined(OS_IOS)
    // iOS is unusual in that it uses a separate LocationView to show the
    // LocationBarModel's display-only URL. The actual OmniboxViewIOS widget is
    // hidden in the defocused state, and always contains the URL for editing.
    display_text_ = url_for_editing_;
#else
    display_text_ = location_bar_model->GetURLForDisplay();
#endif
  }

  // When there's new permanent text, and the user isn't interacting with the
  // omnibox, we want to revert the edit to show the new text.  We could simply
  // define "interacting" as "the omnibox has focus", but we still allow updates
  // when the omnibox has focus as long as the user hasn't begun editing, and
  // isn't seeing zerosuggestions (because changing this text would require
  // changing or hiding those suggestions).  When the omnibox doesn't have
  // focus, we assume the user may have abandoned their interaction and it's
  // always safe to change the text; this also prevents someone toggling "Show
  // URL" (which sounds as if it might be persistent) from seeing just that URL
  // forever afterwards.
  return (GetPermanentDisplayText() != old_display_text) &&
         (!has_focus() || (!user_input_in_progress_ && !PopupIsOpen()));
}

base::string16 OmniboxEditModel::GetPermanentDisplayText() const {
  return display_text_;
}

void OmniboxEditModel::SetUserText(const base::string16& text) {
  SetInputInProgress(true);
  keyword_.clear();
  is_keyword_hint_ = false;
  keyword_mode_entry_method_ = OmniboxEventProto::INVALID;
  InternalSetUserText(text);
  omnibox_controller_->InvalidateCurrentMatch();
  paste_state_ = NONE;
  has_temporary_text_ = false;
}

bool OmniboxEditModel::Unelide(bool exit_query_in_omnibox) {
  // Unelision should not occur if the user has already inputted text.
  if (user_input_in_progress())
    return false;

  // No need to unelide if we are already displaying the full URL.
  LocationBarModel* location_bar_model = controller()->GetLocationBarModel();
  if (view_->GetText() == location_bar_model->GetFormattedFullURL())
    return false;

  // Early exit if we don't want to exit Query in Omnibox mode, and the omnibox
  // is displaying a query.
  if (!exit_query_in_omnibox &&
      location_bar_model->GetDisplaySearchTerms(nullptr))
    return false;

  // Set the user text to the unelided URL, but don't change
  // |user_input_in_progress_|. This is to save the unelided URL on tab switch.
  InternalSetUserText(url_for_editing_);

  view_->SetWindowTextAndCaretPos(url_for_editing_, 0, false, false);

  // Select all in reverse to ensure the beginning of the URL is shown.
  view_->SelectAll(true /* reversed */);

  return true;
}

void OmniboxEditModel::OnChanged() {
  // Don't call CurrentMatch() when there's no editing, as in this case we'll
  // never actually use it.  This avoids running the autocomplete providers (and
  // any systems they then spin up) during startup.
  const AutocompleteMatch& current_match = user_input_in_progress_ ?
      CurrentMatch(nullptr) : AutocompleteMatch();

  client_->OnTextChanged(current_match, user_input_in_progress_, user_text_,
                         result(), has_focus());
  controller_->OnChanged();
}

void OmniboxEditModel::GetDataForURLExport(GURL* url,
                                           base::string16* title,
                                           gfx::Image* favicon) {
  *url = CurrentMatch(nullptr).destination_url;
  if (*url == client_->GetURL()) {
    *title = client_->GetTitle();
    *favicon = client_->GetFavicon();
  }
}

bool OmniboxEditModel::CurrentTextIsURL() const {
  // If !user_input_in_progress_, we can determine if the text is a URL without
  // starting the autocomplete system. This speeds browser startup.
  if (!user_input_in_progress_) {
    // If we are displaying Query in Omnibox, and the user has not clicked
    // "Show URL", then the text must be search terms, and not a URL.
    if (controller()->GetLocationBarModel()->GetDisplaySearchTerms(nullptr) &&
        view_->GetText() == display_text_) {
      return false;
    }

    // In all other cases, the text must be a URL.
    return true;
  }

  return !AutocompleteMatch::IsSearchType(CurrentMatch(nullptr).type);
}

void OmniboxEditModel::AdjustTextForCopy(int sel_min,
                                         base::string16* text,
                                         GURL* url_from_text,
                                         bool* write_url) {
  DCHECK(text);
  DCHECK(url_from_text);
  DCHECK(write_url);

  *write_url = false;

  // Do not adjust if selection did not start at the beginning of the field.
  if (sel_min != 0)
    return;

  // If the user has not modified the display text and is copying the whole URL
  // text (whether it's in the elided or unelided form), copy the omnibox
  // contents as a hyperlink to the current page.
  if (!user_input_in_progress_ &&
      (*text == display_text_ || *text == url_for_editing_)) {
    *url_from_text = controller()->GetLocationBarModel()->GetURL();
    *write_url = true;

    // If the omnibox is displaying a URL, set the hyperlink text to the URL's
    // spec. This undoes any URL elisions.
    if (!controller()->GetLocationBarModel()->GetDisplaySearchTerms(nullptr)) {
      // Don't let users copy Reader Mode page URLs.
      // We display the original article's URL in the omnibox, so users will
      // expect that to be what is copied to the clipboard.
      if (dom_distiller::url_utils::IsDistilledPage(*url_from_text)) {
        *url_from_text =
            dom_distiller::url_utils::GetOriginalUrlFromDistillerUrl(
                *url_from_text);
      }
      *text = base::UTF8ToUTF16(url_from_text->spec());
    }

    return;
  }

  // This code early exits if the copied text looks like a search query. It's
  // not at the very top of this method, as it would interpret the intranet URL
  // "printer/path" as a search query instead of a URL.
  //
  // We can't use CurrentTextIsURL() or GetDataForURLExport() because right now
  // the user is probably holding down control to cause the copy, which will
  // screw up our calculation of the desired_tld.
  AutocompleteMatch match_from_text;
  client_->GetAutocompleteClassifier()->Classify(*text, is_keyword_selected(),
                                                 true, GetPageClassification(),
                                                 &match_from_text, nullptr);
  if (AutocompleteMatch::IsSearchType(match_from_text.type) ||
      match_from_text.is_navigational_title_match)
    return;

  // Make our best GURL interpretation of |text|.
  *url_from_text = match_from_text.destination_url;

  // Get the current page GURL (or the GURL of the currently selected match).
  GURL current_page_url = controller()->GetLocationBarModel()->GetURL();
  if (PopupIsOpen()) {
    AutocompleteMatch current_match = CurrentMatch(nullptr);
    if (!AutocompleteMatch::IsSearchType(current_match.type) &&
        current_match.destination_url.is_valid()) {
      // If the popup is open and a valid match is selected, treat that as the
      // current page, since the URL in the Omnibox will be from that match.
      current_page_url = current_match.destination_url;
    }
  }

  // If the user has altered the host piece of the omnibox text, then we cannot
  // guess at user intent, so early exit and leave |text| as-is as plain text.
  if (!current_page_url.SchemeIsHTTPOrHTTPS() ||
      !url_from_text->SchemeIsHTTPOrHTTPS() ||
      current_page_url.host_piece() != url_from_text->host_piece()) {
    return;
  }

  // Infer the correct scheme for the copied text, and prepend it if necessary.
  {
    base::string16 http = base::ASCIIToUTF16(url::kHttpScheme) +
                          base::ASCIIToUTF16(url::kStandardSchemeSeparator);
    base::string16 https = base::ASCIIToUTF16(url::kHttpsScheme) +
                           base::ASCIIToUTF16(url::kStandardSchemeSeparator);
    const base::string16& current_page_url_prefix =
        current_page_url.SchemeIs(url::kHttpScheme) ? http : https;

    // Only prepend a scheme if the text doesn't already have a scheme.
    if (!base::StartsWith(*text, http, base::CompareCase::INSENSITIVE_ASCII) &&
        !base::StartsWith(*text, https, base::CompareCase::INSENSITIVE_ASCII)) {
      *text = current_page_url_prefix + *text;

      // Amend the copied URL to match the prefixed string.
      GURL::Replacements replace_scheme;
      replace_scheme.SetSchemeStr(current_page_url.scheme_piece());
      *url_from_text = url_from_text->ReplaceComponents(replace_scheme);
    }
  }

  // If the URL derived from |text| is valid, mark |write_url| true, and modify
  // |text| to contain the canonical URL spec with non-ASCII characters escaped.
  if (url_from_text->is_valid()) {
    *write_url = true;
    *text = base::UTF8ToUTF16(url_from_text->spec());
  }
}

bool OmniboxEditModel::ShouldShowCurrentPageIcon() const {
  // If the popup is open, don't show the current page's icon. The caller is
  // instead expected to show the current match's icon.
  if (PopupIsOpen())
    return false;

  // On the New Tab Page, the omnibox textfield is empty. We want to display
  // the default search provider favicon instead of the NTP security icon.
  if (view_->GetText().empty())
    return false;

  // If user input is not in progress, always show the current page's icon.
  if (!user_input_in_progress())
    return true;

  // If user input is in progress, keep showing the current page's icon so long
  // as the text matches the current page's URL, elided or unelided. This logic
  // also works for Query in Omnibox, since the query is in |display_text_|.
  return view_->GetText() == display_text_ ||
         view_->GetText() == url_for_editing_;
}

void OmniboxEditModel::UpdateInput(bool has_selected_text,
                                   bool prevent_inline_autocomplete) {
  bool changed = SetInputInProgressNoNotify(true);
  if (!has_focus()) {
    if (changed)
      NotifyObserversInputInProgress(true);
    return;
  }
  StartAutocomplete(has_selected_text, prevent_inline_autocomplete);
  if (changed)
    NotifyObserversInputInProgress(true);
}

void OmniboxEditModel::SetInputInProgress(bool in_progress) {
  if (SetInputInProgressNoNotify(in_progress))
    NotifyObserversInputInProgress(in_progress);
}

void OmniboxEditModel::Revert() {
  SetInputInProgress(false);
  input_.Clear();
  paste_state_ = NONE;
  InternalSetUserText(base::string16());
  keyword_.clear();
  is_keyword_hint_ = false;
  keyword_mode_entry_method_ = OmniboxEventProto::INVALID;
  has_temporary_text_ = false;
  size_t start, end;
  view_->GetSelectionBounds(&start, &end);
  // First home the cursor, so view of text is scrolled to left, then correct
  // it. |SetCaretPos()| doesn't scroll the text, so doing that first wouldn't
  // accomplish anything.
  base::string16 current_permanent_url = GetPermanentDisplayText();
  view_->SetWindowTextAndCaretPos(current_permanent_url, 0, false, true);
  view_->SetCaretPos(std::min(current_permanent_url.length(), start));
  client_->OnRevert();
}

void OmniboxEditModel::StartAutocomplete(bool has_selected_text,
                                         bool prevent_inline_autocomplete) {
  const base::string16 input_text = MaybePrependKeyword(user_text_);

  size_t start, cursor_position;
  view_->GetSelectionBounds(&start, &cursor_position);

  // For keyword searches, the text that AutocompleteInput expects is
  // of the form "<keyword> <query>", where our query is |user_text_|.
  // So we need to adjust the cursor position forward by the length of
  // any keyword added by MaybePrependKeyword() above.
  if (is_keyword_selected()) {
    // If there is user text, the cursor is past the keyword and doesn't
    // account for its size.  Add the keyword's size to the position passed
    // to autocomplete.
    if (!user_text_.empty()) {
      cursor_position += input_text.length() - user_text_.length();
    } else {
      // Otherwise, cursor may point into keyword or otherwise not account
      // for the keyword's size (depending on how this code is reached).
      // Pass a cursor at end of input to autocomplete.  This is safe in all
      // conditions.
      cursor_position = input_text.length();
    }
  }
  input_ =
      AutocompleteInput(input_text, cursor_position, GetPageClassification(),
                        client_->GetSchemeClassifier());
  input_.set_current_url(client_->GetURL());
  input_.set_current_title(client_->GetTitle());
  input_.set_prevent_inline_autocomplete(
      prevent_inline_autocomplete || just_deleted_text_ ||
      (has_selected_text && inline_autocomplete_text_.empty()) ||
      (paste_state_ != NONE));
  input_.set_prefer_keyword(is_keyword_selected());
  input_.set_allow_exact_keyword_match(is_keyword_selected() ||
                                       allow_exact_keyword_match_);
  input_.set_keyword_mode_entry_method(keyword_mode_entry_method_);

  omnibox_controller_->StartAutocomplete(input_);
}

void OmniboxEditModel::StopAutocomplete() {
  autocomplete_controller()->Stop(true);
}

bool OmniboxEditModel::CanPasteAndGo(const base::string16& text) const {
  if (!client_->IsPasteAndGoEnabled())
    return false;

  AutocompleteMatch match;
  ClassifyString(text, &match, nullptr);
  return match.destination_url.is_valid();
}

void OmniboxEditModel::PasteAndGo(const base::string16& text,
                                  base::TimeTicks match_selection_timestamp) {
  DCHECK(CanPasteAndGo(text));
  UMA_HISTOGRAM_COUNTS_1M("Omnibox.PasteAndGo", 1);

  view_->RevertAll();
  AutocompleteMatch match;
  GURL alternate_nav_url;
  ClassifyString(text, &match, &alternate_nav_url);
  view_->OpenMatch(match, WindowOpenDisposition::CURRENT_TAB, alternate_nav_url,
                   text, OmniboxPopupModel::kNoMatch,
                   match_selection_timestamp);
}

void OmniboxEditModel::AcceptInput(WindowOpenDisposition disposition,
                                   base::TimeTicks match_selection_timestamp) {
  // Get the URL and transition type for the selected entry.
  GURL alternate_nav_url;
  AutocompleteMatch match = CurrentMatch(&alternate_nav_url);

  // If CTRL is down it means the user wants to append ".com" to the text they
  // typed. If we can successfully generate a URL_WHAT_YOU_TYPED match doing
  // that, then we use this. These matches are marked as generated by the
  // HistoryURLProvider so we only generate them if this provider is present.
  if (control_key_state_ == DOWN && !is_keyword_selected() &&
      autocomplete_controller()->history_url_provider()) {
    // For generating the hostname of the URL, we use the most recent
    // input instead of the currently visible text. This means we'll ignore any
    // visible inline autocompletion: if a user types "foo" and is autocompleted
    // to "foodnetwork.com", ctrl-enter will navigate to "foo.com", not
    // "foodnetwork.com".  At the time of writing, this behavior matches
    // Internet Explorer, but not Firefox. Two exceptions to our own rule:
    //  1. If the user has selected a suggestion, use the suggestion text.
    //  2. If the user has never edited the text, use the current page's full
    //     URL instead of the elided URL to avoid HTTPS downgrading.
    base::string16 text_for_desired_tld_navigation = input_.text();
    if (has_temporary_text_)
      text_for_desired_tld_navigation = view_->GetText();
    else if (!user_input_in_progress())
      text_for_desired_tld_navigation = url_for_editing_;

    // Generate a new AutocompleteInput, copying the latest one but using "com"
    // as the desired TLD. Then use this autocomplete input to generate a
    // URL_WHAT_YOU_TYPED AutocompleteMatch.
    AutocompleteInput input(
        text_for_desired_tld_navigation, input_.cursor_position(), "com",
        input_.current_page_classification(), client_->GetSchemeClassifier());
    input.set_prevent_inline_autocomplete(input_.prevent_inline_autocomplete());
    input.set_prefer_keyword(input_.prefer_keyword());
    input.set_keyword_mode_entry_method(input_.keyword_mode_entry_method());
    input.set_allow_exact_keyword_match(input_.allow_exact_keyword_match());
    input.set_want_asynchronous_matches(input_.want_asynchronous_matches());
    input.set_from_omnibox_focus(input_.from_omnibox_focus());
    input_ = input;
    AutocompleteMatch url_match(
        autocomplete_controller()->history_url_provider()->SuggestExactInput(
            input_, input_.canonicalized_url(), false));

    if (url_match.destination_url.is_valid()) {
      // We have a valid URL, we use this newly generated AutocompleteMatch.
      match = url_match;
      alternate_nav_url = GURL();
    }
  }

  if (!match.destination_url.is_valid())
    return;

  if (ui::PageTransitionCoreTypeIs(match.transition,
                                   ui::PAGE_TRANSITION_TYPED) &&
      (match.destination_url ==
       controller()->GetLocationBarModel()->GetURL())) {
    // When the user hit enter on the existing permanent URL, treat it like a
    // reload for scoring purposes.  We could detect this by just checking
    // user_input_in_progress_, but it seems better to treat "edits" that end
    // up leaving the URL unchanged (e.g. deleting the last character and then
    // retyping it) as reloads too.  We exclude non-TYPED transitions because if
    // the transition is GENERATED, the user input something that looked
    // different from the current URL, even if it wound up at the same place
    // (e.g. manually retyping the same search query), and it seems wrong to
    // treat this as a reload.
    match.transition = ui::PAGE_TRANSITION_RELOAD;
  } else if (ui::PageTransitionCoreTypeIs(match.transition,
                                          ui::PAGE_TRANSITION_GENERATED)) {
    // When the omnibox is displaying the default search provider search terms,
    // the user focuses the omnibox, and hits Enter without refining the search
    // terms, we should classify this transition as a RELOAD.
    base::string16 search_terms;
    if (controller()->GetLocationBarModel()->GetDisplaySearchTerms(
            &search_terms) &&
        match.fill_into_edit == search_terms &&
        match
            .GetSubstitutingExplicitlyInvokedKeyword(
                client_->GetTemplateURLService())
            .empty()) {
      match.transition = ui::PAGE_TRANSITION_RELOAD;
    }
  } else if (paste_state_ != NONE &&
             match.type == AutocompleteMatchType::URL_WHAT_YOU_TYPED) {
    // When the user pasted in a URL and hit enter, score it like a link click
    // rather than a normal typed URL, so it doesn't get inline autocompleted
    // as aggressively later.
    match.transition = ui::PAGE_TRANSITION_LINK;
  }

  client_->OnInputAccepted(match);

  // popup_model() could be nullptr during unit tests.
  if (popup_model()) {
    view_->OpenMatch(match, disposition, alternate_nav_url, base::string16(),
                     popup_model()->selected_line(), match_selection_timestamp);
  }
}

void OmniboxEditModel::EnterKeywordModeForDefaultSearchProvider(
    OmniboxEventProto::KeywordModeEntryMethod entry_method) {
  if (!client_->IsDefaultSearchProviderEnabled())
    return;

  autocomplete_controller()->Stop(false);

  keyword_ =
      client_->GetTemplateURLService()->GetDefaultSearchProvider()->keyword();
  is_keyword_hint_ = false;
  keyword_mode_entry_method_ = entry_method;

  base::string16 display_text =
      user_input_in_progress_ ? view_->GetText() : base::string16();
  size_t caret_pos = display_text.length();
  if (entry_method == OmniboxEventProto::QUESTION_MARK) {
    display_text.erase(0, 1);
    caret_pos = 0;
  }

  InternalSetUserText(display_text);
  view_->SetWindowTextAndCaretPos(display_text, caret_pos, true, false);
  if (entry_method == OmniboxEventProto::KEYBOARD_SHORTCUT)
    view_->SelectAll(false);

  EmitKeywordHistogram(entry_method);
}

void OmniboxEditModel::ExecutePedal(const AutocompleteMatch& match,
                                    base::TimeTicks match_selection_timestamp) {
  CHECK(match.pedal);
  {
    // This block resets omnibox to unedited state and closes popup, which
    // may not seem necessary in cases of navigation but makes sense for
    // taking Pedal actions in general.
    base::AutoReset<bool> tmp(&in_revert_, true);
    view_->RevertAll();
  }
  OmniboxPedal::ExecutionContext context(*client_, *controller_,
                                         match_selection_timestamp);
  match.pedal->Execute(context);
}

void OmniboxEditModel::OpenMatch(AutocompleteMatch match,
                                 WindowOpenDisposition disposition,
                                 const GURL& alternate_nav_url,
                                 const base::string16& pasted_text,
                                 size_t index,
                                 base::TimeTicks match_selection_timestamp) {
  const base::TimeTicks& now(base::TimeTicks::Now());
  base::TimeDelta elapsed_time_since_user_first_modified_omnibox(
      now - time_user_first_modified_omnibox_);
  autocomplete_controller()->UpdateMatchDestinationURLWithQueryFormulationTime(
      elapsed_time_since_user_first_modified_omnibox, &match);

  // Matches with |pedal| may be opened normally or executed, but when a match
  // is a dedicated Pedal suggestion, it should always be executed. This only
  // happens when the button row feature is disabled.
  if (match.pedal && !OmniboxFieldTrial::IsSuggestionButtonRowEnabled()) {
    ExecutePedal(match, match_selection_timestamp);
    return;
  }

  base::string16 input_text(pasted_text);
  if (input_text.empty())
    input_text = user_input_in_progress_ ? user_text_ : url_for_editing_;
  // Create a dummy AutocompleteInput for use in calling SuggestExactInput()
  // to create an alternate navigational match.
  AutocompleteInput alternate_input(input_text, GetPageClassification(),
                                    client_->GetSchemeClassifier());
  // Somehow we can occasionally get here with no active tab.  It's not
  // clear why this happens.
  alternate_input.set_current_url(client_->GetURL());
  alternate_input.set_current_title(client_->GetTitle());
  std::unique_ptr<OmniboxNavigationObserver> observer(
      client_->CreateOmniboxNavigationObserver(
          input_text, match,
          autocomplete_controller()->history_url_provider()->SuggestExactInput(
              alternate_input, alternate_nav_url, false)));

  base::TimeDelta elapsed_time_since_last_change_to_default_match(
      now - autocomplete_controller()->last_time_default_match_changed());
  DCHECK(match.provider);
  // These elapsed times don't really make sense for matches that come from
  // omnibox focus (because the user did not modify the omnibox), so for those
  // we set the elapsed times to something that will be ignored by
  // metrics_log.cc.  They also don't necessarily make sense if the omnibox
  // dropdown is closed or the user used a paste-and-go action.  (In most
  // cases when this happens, the user never modified the omnibox.)
  const bool popup_open = PopupIsOpen();
  if (input_.from_omnibox_focus() || !popup_open || !pasted_text.empty()) {
    const base::TimeDelta default_time_delta =
        base::TimeDelta::FromMilliseconds(-1);
    elapsed_time_since_user_first_modified_omnibox = default_time_delta;
    elapsed_time_since_last_change_to_default_match = default_time_delta;
  }

  // In some unusual cases, we ignore result() and instead log a fake result set
  // with a single element (|match|) and selected_index of 0. For these cases:
  //  1. If the popup is closed (there is no result set).
  //  2. If the index is out of bounds. This should only happen if |index| is
  //     kNoMatch, which can happen if the default search provider is disabled.
  //  3. If this is a paste-and-go action (meaning the contents of the dropdown
  //     are ignored regardless).
  const bool dropdown_ignored =
      !popup_open || index >= result().size() || !pasted_text.empty();
  ACMatches fake_single_entry_matches;
  fake_single_entry_matches.push_back(match);
  AutocompleteResult fake_single_entry_result;
  fake_single_entry_result.AppendMatches(input_, fake_single_entry_matches);

  OmniboxLog log(
      input_.from_omnibox_focus() ? base::string16() : input_text,
      just_deleted_text_, input_.type(), is_keyword_selected(),
      keyword_mode_entry_method_, popup_open, dropdown_ignored ? 0 : index,
      disposition, !pasted_text.empty(),
      SessionID::InvalidValue(),  // don't know tab ID; set later if appropriate
      GetPageClassification(), elapsed_time_since_user_first_modified_omnibox,
      match.allowed_to_be_default_match ? match.inline_autocompletion.length()
                                        : base::string16::npos,
      elapsed_time_since_last_change_to_default_match,
      dropdown_ignored ? fake_single_entry_result : result());
  DCHECK(dropdown_ignored ||
         (log.elapsed_time_since_user_first_modified_omnibox >=
          log.elapsed_time_since_last_change_to_default_match))
      << "We should've got the notification that the user modified the "
      << "omnibox text at same time or before the most recent time the "
      << "default match changed.";

  if ((disposition == WindowOpenDisposition::CURRENT_TAB) &&
      client_->CurrentPageExists()) {
    // If we know the destination is being opened in the current tab,
    // we can easily get the tab ID.  (If it's being opened in a new
    // tab, we don't know the tab ID yet.)
    log.tab_id = client_->GetSessionID();
  }
  autocomplete_controller()->AddProvidersInfo(&log.providers_info);
  client_->OnURLOpenedFromOmnibox(&log);
  OmniboxEventGlobalTracker::GetInstance()->OnURLOpened(&log);
  LOCAL_HISTOGRAM_BOOLEAN("Omnibox.EventCount", true);
  SuggestionAnswer::LogAnswerUsed(match.answer);
  if (!last_omnibox_focus_.is_null()) {
    // Only record focus to open time when a focus actually happened (as
    // opposed to, say, dragging a link onto the omnibox).
    UMA_HISTOGRAM_MEDIUM_TIMES(kFocusToOpenTimeHistogram,
                               now - last_omnibox_focus_);
  }

  TemplateURLService* service = client_->GetTemplateURLService();
  TemplateURL* template_url = match.GetTemplateURL(service, false);
  if (template_url) {
    if (ui::PageTransitionTypeIncludingQualifiersIs(
            match.transition, ui::PAGE_TRANSITION_KEYWORD)) {
      // The user is using a non-substituting keyword or is explicitly in
      // keyword mode.

      // Don't increment usage count for extension keywords.
      if (client_->ProcessExtensionKeyword(template_url, match, disposition,
                                           observer.get())) {
        if (disposition != WindowOpenDisposition::NEW_BACKGROUND_TAB)
          view_->RevertAll();
        return;
      }

      base::RecordAction(base::UserMetricsAction("AcceptedKeyword"));
      client_->GetTemplateURLService()->IncrementUsageCount(template_url);
    } else {
      DCHECK(ui::PageTransitionTypeIncludingQualifiersIs(
                 match.transition, ui::PAGE_TRANSITION_GENERATED) ||
             ui::PageTransitionTypeIncludingQualifiersIs(
                 match.transition, ui::PAGE_TRANSITION_RELOAD));
      // NOTE: We purposefully don't increment the usage count of the default
      // search engine here like we do for explicit keywords above; see comments
      // in template_url.h.
    }

    AutocompleteMatch::LogSearchEngineUsed(match, service);
  } else {
    // |match| is a URL navigation, not a search.
    // For logging the below histogram, only record uses that depend on the
    // omnibox suggestion system, i.e., TYPED navigations.  That is, exclude
    // omnibox URL interactions that are treated as reloads or link-following
    // (i.e., cut-and-paste of URLs).
    if (ui::PageTransitionTypeIncludingQualifiersIs(match.transition,
                                                    ui::PAGE_TRANSITION_TYPED))
      navigation_metrics::RecordOmniboxURLNavigation(match.destination_url);
  }

  if (disposition != WindowOpenDisposition::NEW_BACKGROUND_TAB) {
    base::AutoReset<bool> tmp(&in_revert_, true);
    view_->RevertAll();  // Revert the box to its unedited state.
  }

  // Track whether the destination URL sends us to a search results page
  // using the default search provider.
  TemplateURLService* template_url_service = client_->GetTemplateURLService();
  if (template_url_service &&
      template_url_service->IsSearchResultsPageFromDefaultSearchProvider(
          match.destination_url)) {
    base::RecordAction(
        base::UserMetricsAction("OmniboxDestinationURLIsSearchOnDSP"));
    base::UmaHistogramBoolean(
        "Omnibox.Search.OffTheRecord",
        client_->CreateAutocompleteProviderClient()->IsOffTheRecord());
  }

  if (match.destination_url.is_valid()) {
    // This calls RevertAll again.
    base::AutoReset<bool> tmp(&in_revert_, true);

    controller_->OnAutocompleteAccept(
        match.destination_url, match.post_content.get(), disposition,
        ui::PageTransitionFromInt(match.transition |
                                  ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
        match.type, match_selection_timestamp);

    // The observer should have been synchronously notified of a pending load.
    if (observer && observer->HasSeenPendingLoad())
      ignore_result(observer.release());  // The observer will delete itself.
  }

  BookmarkModel* bookmark_model = client_->GetBookmarkModel();
  if (bookmark_model && bookmark_model->IsBookmarked(match.destination_url))
    client_->OnBookmarkLaunched();
}

bool OmniboxEditModel::InExplicitExperimentalKeywordMode() {
  return AutocompleteProvider::InExplicitExperimentalKeywordMode(input_,
                                                                 keyword_);
}

bool OmniboxEditModel::AcceptKeyword(
    OmniboxEventProto::KeywordModeEntryMethod entry_method) {
  DCHECK(is_keyword_hint_ && !keyword_.empty());

  autocomplete_controller()->Stop(false);

  is_keyword_hint_ = false;
  keyword_mode_entry_method_ = entry_method;
  if (original_user_text_with_keyword_.empty())
    original_user_text_with_keyword_ = user_text_;
  user_text_ = MaybeStripKeyword(user_text_);

  if (PopupIsOpen())
    popup_model()->SetSelectedLineState(OmniboxPopupModel::KEYWORD);
  else
    StartAutocomplete(false, true);

  // When entering keyword mode via tab, the new text to show is whatever the
  // newly-selected match in the dropdown is.  When entering via space, however,
  // we should make sure to use the actual |user_text_| as the basis for the new
  // text.  This ensures that if the user types "<keyword><space>" and the
  // default match would have inline autocompleted a further string (e.g.
  // because there's a past multi-word search beginning with this keyword), the
  // inline autocompletion doesn't get filled in as the keyword search query
  // text.
  //
  // We also treat tabbing into keyword mode like tabbing through the popup in
  // that we set |has_temporary_text_|, whereas pressing space is treated like
  // a new keystroke that changes the current match instead of overlaying it
  // with a temporary one.  This is important because rerunning autocomplete
  // after the user pressed space, which will have happened just before reaching
  // here, may have generated a new match, which the user won't actually see and
  // which we don't want to switch back to when exiting keyword mode; see
  // comments in ClearKeyword().
  const AutocompleteMatch& match = CurrentMatch(nullptr);
  if (entry_method == OmniboxEventProto::TAB) {
    // Ensure the current selection is saved before showing keyword mode
    // so that moving to another line and then reverting the text will restore
    // the current state properly.
    view_->OnTemporaryTextMaybeChanged(MaybeStripKeyword(match.fill_into_edit),
                                       match, !has_temporary_text_, true);
  } else {
    view_->OnTemporaryTextMaybeChanged(user_text_, match, !has_temporary_text_,
                                       true);
  }

  base::RecordAction(base::UserMetricsAction("AcceptedKeywordHint"));
  EmitKeywordHistogram(entry_method);

  return true;
}

void OmniboxEditModel::AcceptTemporaryTextAsUserText() {
  InternalSetUserText(view_->GetText());
  has_temporary_text_ = false;

  if (user_input_in_progress_ || !in_revert_)
    client_->OnInputStateChanged();
}

void OmniboxEditModel::ClearKeyword() {
  if (!is_keyword_selected())
    return;

  autocomplete_controller()->Stop(false);

  // While we're always in keyword mode upon reaching here, sometimes we've just
  // toggled in via space or tab, and sometimes we're on a non-toggled line
  // (usually because the user has typed a search string).  Keep track of the
  // difference, as we'll need it below. popup_model() may be nullptr in tests.
  bool was_toggled_into_keyword_mode =
      popup_model() &&
      popup_model()->selected_line_state() == OmniboxPopupModel::KEYWORD;

  bool entry_by_tab = keyword_mode_entry_method_ == OmniboxEventProto::TAB;

  omnibox_controller_->ClearPopupKeywordMode();

  // There are several possible states we could have been in before the user hit
  // backspace or shift-tab to enter this function:
  // (1) was_toggled_into_keyword_mode == false, entry_by_tab == false
  //     The user typed a further key after being in keyword mode already, e.g.
  //     "google.com f".
  // (2) was_toggled_into_keyword_mode == false, entry_by_tab == true
  //     The user tabbed away from a dropdown entry in keyword mode, then tabbed
  //     back to it, e.g. "google.com f<tab><shift-tab>".
  // (3) was_toggled_into_keyword_mode == true, entry_by_tab == false
  //     The user had just typed space to enter keyword mode, e.g.
  //     "google.com ".
  // (4) was_toggled_into_keyword_mode == true, entry_by_tab == true
  //     The user had just typed tab to enter keyword mode, e.g.
  //     "google.com<tab>".
  //
  // For states 1-3, we can safely handle the exit from keyword mode by using
  // OnBefore/AfterPossibleChange() to do a complete state update of all
  // objects.  However, with state 4, if we do this, and if the user had tabbed
  // into keyword mode on a line in the middle of the dropdown instead of the
  // first line, then the state update will rerun autocompletion and reset the
  // whole dropdown, and end up with the first line selected instead, instead of
  // just "undoing" the keyword mode entry on the non-first line.  So in this
  // case we simply reset |is_keyword_hint_| to true and update the window text.
  //
  // You might wonder why we don't simply do this in all cases.  In states 1-2,
  // getting out of keyword mode likely shouldn't put us in keyword hint mode;
  // if the user typed "google.com f" and then put the cursor before 'f' and hit
  // backspace, the resulting text would be "google.comf", which is unlikely to
  // be a keyword.  Unconditionally putting things back in keyword hint mode is
  // going to lead to internally inconsistent state, and possible future
  // crashes.  State 3 is more subtle; here we need to do the full state update
  // because before entering keyword mode to begin with, we will have re-run
  // autocomplete in ways that can produce surprising results if we just switch
  // back out of keyword mode.  For example, if a user has a keyword named "x",
  // an inline-autocompletable history site "xyz.com", and a lower-ranked
  // inline-autocompletable search "x y", then typing "x" will inline-
  // autocomplete to "xyz.com", hitting space will toggle into keyword mode, but
  // then hitting backspace could wind up with the default match as the "x y"
  // search, which feels bizarre.
  if (was_toggled_into_keyword_mode && entry_by_tab) {
    // State 4 above.
    is_keyword_hint_ = true;
    keyword_mode_entry_method_ = OmniboxEventProto::INVALID;
    const base::string16 window_text = keyword_ + view_->GetText();
    view_->SetWindowTextAndCaretPos(window_text, keyword_.length(), false,
                                    true);
  } else {
    // States 1-3 above.
    view_->OnBeforePossibleChange();
    // Add a space after the keyword to allow the user to continue typing
    // without re-enabling keyword mode.  The common case is state 3, where
    // the user entered keyword mode unintentionally, so backspacing
    // immediately out of keyword mode should keep the space.  In states 1 and
    // 2, having the space is "safer" behavior.  For instance, if the user types
    // "google.com f" or "google.com<tab>f" in the omnibox, moves the cursor to
    // the left, and presses backspace to leave keyword mode (state 1), it's
    // better to have the space because it may be what the user wanted.  The
    // user can easily delete it.  On the other hand, if there is no space and
    // the user wants it, it's more work to add because typing the space will
    // enter keyword mode, which then the user would have to leave again.

    // If we entered keyword mode in a special way like using a keyboard
    // shortcut or typing a question mark in a blank omnibox, don't restore the
    // keyword.  Instead, restore the question mark iff the user originally
    // typed one.
    base::string16 prefix;
    if (keyword_mode_entry_method_ == OmniboxEventProto::QUESTION_MARK)
      prefix = base::ASCIIToUTF16("?");
    else if (keyword_mode_entry_method_ != OmniboxEventProto::KEYBOARD_SHORTCUT)
      prefix = keyword_ + base::ASCIIToUTF16(" ");

    keyword_.clear();
    is_keyword_hint_ = false;
    keyword_mode_entry_method_ = OmniboxEventProto::INVALID;

    view_->SetWindowTextAndCaretPos(prefix + view_->GetText(), prefix.length(),
                                    false, false);

    view_->OnAfterPossibleChange(false);
  }
}

void OmniboxEditModel::OnSetFocus(bool control_down) {
  last_omnibox_focus_ = base::TimeTicks::Now();
  user_input_since_focus_ = false;

  // If the omnibox lost focus while the caret was hidden and then regained
  // focus, OnSetFocus() is called and should restore visibility. Note that
  // focus can be regained without an accompanying call to
  // OmniboxView::SetFocus(), e.g. by tabbing in.
  SetFocusState(OMNIBOX_FOCUS_VISIBLE, OMNIBOX_FOCUS_CHANGE_EXPLICIT);
  // On focusing the omnibox, if the ctrl key is pressed, we don't want to
  // trigger ctrl-enter behavior unless it is released and re-pressed. For
  // example, if the user presses ctrl-l to focus the omnibox.
  control_key_state_ = control_down ? DOWN_AND_CONSUMED : UP;

  if (user_input_in_progress_ || !in_revert_)
    client_->OnInputStateChanged();
}

void OmniboxEditModel::ShowOnFocusSuggestionsIfAutocompleteIdle() {
  // Early exit if a query is already in progress or the popup is already open.
  // This is what allows this method to be called multiple times in multiple
  // code locations without harm.
  if (query_in_progress() || PopupIsOpen())
    return;

  // Early exit if the page has not loaded yet, so we don't annoy users.
  if (!client_->CurrentPageExists())
    return;

  // Early exit if the user already has a navigation or search query in mind.
  if (user_input_in_progress_)
    return;

  // Send the textfield contents exactly as-is, as otherwise the verbatim
  // match can be wrong. The full page URL is anyways in set_current_url().
  input_ = AutocompleteInput(view_->GetText(), GetPageClassification(),
                             client_->GetSchemeClassifier());
  input_.set_current_url(client_->GetURL());
  input_.set_current_title(client_->GetTitle());
  input_.set_from_omnibox_focus(true);
  autocomplete_controller()->Start(input_);
}

void OmniboxEditModel::SetCaretVisibility(bool visible) {
  // Caret visibility only matters if the omnibox has focus.
  if (focus_state_ != OMNIBOX_FOCUS_NONE) {
    SetFocusState(visible ? OMNIBOX_FOCUS_VISIBLE : OMNIBOX_FOCUS_INVISIBLE,
                  OMNIBOX_FOCUS_CHANGE_EXPLICIT);
  }
}

void OmniboxEditModel::ConsumeCtrlKey() {
  if (control_key_state_ == DOWN)
    control_key_state_ = DOWN_AND_CONSUMED;
}

void OmniboxEditModel::OnWillKillFocus() {
  if (user_input_in_progress_ || !in_revert_)
    client_->OnInputStateChanged();
}

void OmniboxEditModel::OnKillFocus() {
  SetFocusState(OMNIBOX_FOCUS_NONE, OMNIBOX_FOCUS_CHANGE_EXPLICIT);
  focus_source_ = OmniboxFocusSource::INVALID;
  last_omnibox_focus_ = base::TimeTicks();
  paste_state_ = NONE;
  control_key_state_ = UP;
#if defined(OS_WIN)
  view_->HideImeIfNeeded();
#endif
}

bool OmniboxEditModel::WillHandleEscapeKey() const {
  return user_input_in_progress_ || has_temporary_text_;
}

bool OmniboxEditModel::OnEscapeKeyPressed() {
  if (has_temporary_text_) {
    RevertTemporaryTextAndPopup();
    return true;
  }

  // We do not clear the pending entry from the omnibox when a load is first
  // stopped.  If the user presses Escape while stopped, whether editing or not,
  // we clear it.
  if (client_->CurrentPageExists() && !client_->IsLoading()) {
    client_->DiscardNonCommittedNavigations();
    view_->Update();
  }

  if (!user_text_.empty()) {
    UMA_HISTOGRAM_ENUMERATION(kOmniboxUserTextClearedHistogram,
                              OMNIBOX_USER_TEXT_CLEARED_WITH_ESCAPE,
                              OMNIBOX_USER_TEXT_CLEARED_NUM_OF_ITEMS);
  }

  // Unconditionally revert/select all.  This ensures any popup, whether due to
  // normal editing or ZeroSuggest, is closed, and the full text is selected.
  // This in turn allows the user to use escape to quickly select all the text
  // for ease of replacement, and matches other browsers.
  bool user_input_was_in_progress = user_input_in_progress_;
  view_->RevertAll();
  view_->SelectAll(true);

  // If the user was in the midst of editing, don't cancel any underlying page
  // load.  This doesn't match IE or Firefox, but seems more correct.  Note that
  // we do allow the page load to be stopped in the case where ZeroSuggest was
  // visible; this is so that it's still possible to focus the address bar and
  // hit escape once to stop a load even if the address being loaded triggers
  // the ZeroSuggest popup.
  return user_input_was_in_progress;
}

void OmniboxEditModel::OnControlKeyChanged(bool pressed) {
  if (pressed == (control_key_state_ == UP))
    control_key_state_ = pressed ? DOWN : UP;
}

void OmniboxEditModel::OnPaste() {
  UMA_HISTOGRAM_COUNTS_1M("Omnibox.Paste", 1);
  paste_state_ = PASTING;
}

void OmniboxEditModel::OnUpOrDownKeyPressed(int count) {
  DCHECK(count == -1 || count == 1);

  // NOTE: This purposefully doesn't trigger any code that resets paste_state_.
  if (PopupIsOpen()) {
    // The popup is open, so the user should be able to interact with it
    // normally.

    // If, as a result of the key press, we would select the first result, then
    // we should revert the temporary text same as what pressing escape would
    // have done.
    //
    // Reverting, however, does not make sense for on-focus suggestions
    // (user_input_in_progress_ is false) unless the first result is a
    // verbatim match of the omnibox input (on-focus query refinements on SERP).
    const auto next_selection = popup_model()->GetNextSelection(
        count > 0 ? OmniboxPopupModel::kForward : OmniboxPopupModel::kBackward,
        OmniboxPopupModel::kWholeLine);
    if (result().default_match() && has_temporary_text_ &&
        next_selection.line == 0 &&
        (user_input_in_progress_ ||
         result().default_match()->IsVerbatimType())) {
      RevertTemporaryTextAndPopup();
    } else {
      popup_model()->SetSelection(next_selection);
    }
    return;
  }

  MaybeStartQueryForPopup();

  // TODO(pkasting): Here, the popup could be working on a query but is not
  // open. In that case, we should force it to open immediately.
}

bool OmniboxEditModel::MaybeStartQueryForPopup() {
  if (PopupIsOpen()) {
    return false;
  }
  if (!query_in_progress()) {
    // The popup is neither open nor working on a query already.  So, start an
    // autocomplete query for the current text.  This also sets
    // user_input_in_progress_ to true, which we want: if the user has started
    // to interact with the popup, changing the url_for_editing_ shouldn't
    // change the displayed text.
    // Note: This does not force the popup to open immediately.
    // TODO(pkasting): We should, in fact, force this particular query to open
    // the popup immediately.
    if (!user_input_in_progress_)
      InternalSetUserText(url_for_editing_);
    view_->UpdatePopup();
    return true;
  }
  return false;
}

void OmniboxEditModel::OnPopupDataChanged(const base::string16& text,
                                          bool is_temporary_text,
                                          const base::string16& keyword,
                                          bool is_keyword_hint) {
  if (!original_user_text_with_keyword_.empty() && !is_temporary_text &&
      (keyword.empty() || is_keyword_hint)) {
    user_text_ = original_user_text_with_keyword_;
    original_user_text_with_keyword_.clear();
  }

  // The popup changed its data, the match in the controller is no longer valid.
  omnibox_controller_->InvalidateCurrentMatch();

  // Update keyword/hint-related local state.
  bool keyword_state_changed = (keyword_ != keyword) ||
      ((is_keyword_hint_ != is_keyword_hint) && !keyword.empty());
  if (keyword_state_changed) {
    bool keyword_was_selected = is_keyword_selected();
    keyword_ = keyword;
    is_keyword_hint_ = is_keyword_hint;
    if (!keyword_was_selected && is_keyword_selected()) {
      // Since we entered keyword mode, record the reason. Note that we
      // don't do this simply because the keyword changes, since the user
      // never left keyword mode.
      keyword_mode_entry_method_ = OmniboxEventProto::SELECT_SUGGESTION;
    } else if (!is_keyword_selected()) {
      // We've left keyword mode, so align the entry method field with that.
      keyword_mode_entry_method_ = OmniboxEventProto::INVALID;
    }

    // |is_keyword_hint_| should always be false if |keyword_| is empty.
    DCHECK(!keyword_.empty() || !is_keyword_hint_);
  }

  // Handle changes to temporary text.
  if (is_temporary_text) {
    const bool save_original_selection = !has_temporary_text_;
    if (save_original_selection) {
      // Save the original selection and URL so it can be reverted later.
      has_temporary_text_ = true;
      inline_autocomplete_text_.clear();
      view_->OnInlineAutocompleteTextCleared();
    }
    // Arrowing around the popup cancels control-enter.
    ConsumeCtrlKey();
    // Now things are a bit screwy: the desired_tld has changed, but if we
    // update the popup, the new order of entries won't match the old, so the
    // user's selection gets screwy; and if we don't update the popup, and the
    // user reverts, then the selected item will be as if control is still
    // pressed, even though maybe it isn't any more.  There is no obvious
    // right answer here :(

    const AutocompleteMatch& match = CurrentMatch(nullptr);
    view_->OnTemporaryTextMaybeChanged(
        MaybeStripKeyword(text), match,
        save_original_selection && original_user_text_with_keyword_.empty(),
        true);
    return;
  }

  bool call_controller_onchanged = true;
  inline_autocomplete_text_ = text;
  if (inline_autocomplete_text_.empty())
    view_->OnInlineAutocompleteTextCleared();

  const base::string16& user_text =
      user_input_in_progress_ ? user_text_ : input_.text();
  if (keyword_state_changed && is_keyword_selected() &&
      inline_autocomplete_text_.empty()) {
    // If we reach here, the user most likely entered keyword mode by inserting
    // a space between a keyword name and a search string (as pressing space or
    // tab after the keyword name alone would have been be handled in
    // MaybeAcceptKeywordBySpace() by calling AcceptKeyword(), which won't reach
    // here).  In this case, we don't want to call
    // OnInlineAutocompleteTextMaybeChanged() as normal, because that will
    // correctly change the text (to the search string alone) but move the caret
    // to the end of the string; instead we want the caret at the start of the
    // search string since that's where it was in the original input.  So we set
    // the text and caret position directly.
    //
    // It may also be possible to reach here if we're reverting from having
    // temporary text back to a default match that's a keyword search, but in
    // that case the RevertTemporaryTextAndPopup() call below will reset the
    // caret or selection correctly so the caret positioning we do here won't
    // matter.
    view_->SetWindowTextAndCaretPos(user_text, 0, false, true);
  } else if (view_->OnInlineAutocompleteTextMaybeChanged(
                 user_text + inline_autocomplete_text_, user_text.length())) {
    call_controller_onchanged = false;
  }

  // We need to invoke OnChanged in case the destination url changed (as could
  // happen when control is toggled).
  if (call_controller_onchanged)
    OnChanged();
}

bool OmniboxEditModel::OnAfterPossibleChange(
    const OmniboxView::StateChanges& state_changes,
    bool allow_keyword_ui_change) {
  // Update the paste state as appropriate: if we're just finishing a paste
  // that replaced all the text, preserve that information; otherwise, if we've
  // made some other edit, clear paste tracking.
  if (paste_state_ == PASTING)
    paste_state_ = PASTED;
  else if (state_changes.text_differs)
    paste_state_ = NONE;

  if (state_changes.text_differs || state_changes.selection_differs) {
    // Record current focus state for this input if we haven't already.
    if (focus_source_ == OmniboxFocusSource::INVALID) {
      // We should generally expect the omnibox to have focus at this point, but
      // it doesn't always on Linux. This is because, unlike other platforms,
      // right clicking in the omnibox on Linux doesn't focus it. So pasting via
      // right-click can change the contents without focusing the omnibox.
      // TODO(samarth): fix Linux focus behavior and add a DCHECK here to
      // check that the omnibox does have focus.
      focus_source_ = (focus_state_ == OMNIBOX_FOCUS_INVISIBLE)
                          ? OmniboxFocusSource::FAKEBOX
                          : OmniboxFocusSource::OMNIBOX;
    }

    // Restore caret visibility whenever the user changes text or selection in
    // the omnibox.
    SetFocusState(OMNIBOX_FOCUS_VISIBLE, OMNIBOX_FOCUS_CHANGE_TYPING);
  }

  // When the user performs an action with the ctrl key pressed down, we assume
  // the ctrl key was intended for that action. If they then press enter without
  // releasing the ctrl key, we prevent "ctrl-enter" behavior.
  ConsumeCtrlKey();

  // If the user text does not need to be changed, return now, so we don't
  // change any other state, lest arrowing around the omnibox do something like
  // reset |just_deleted_text_|.  Note that modifying the selection accepts any
  // inline autocompletion, which results in a user text change.
  if (!state_changes.text_differs &&
      (!state_changes.selection_differs || inline_autocomplete_text_.empty())) {
    if (state_changes.keyword_differs) {
      // We won't need the below logic for creating a keyword by a space at the
      // end or in the middle, or by typing a '?', but we do need to update the
      // popup view because the keyword can change without the text changing,
      // for example when the keyword is "youtube.com" and the user presses
      // ctrl-k to change it to "google.com", or if the user text is empty and
      // the user presses backspace.
      view_->UpdatePopup();
    }
    return state_changes.keyword_differs;
  }

  InternalSetUserText(*state_changes.new_text);
  has_temporary_text_ = false;
  just_deleted_text_ = state_changes.just_deleted_text;

  if (user_input_in_progress_ && user_text_.empty()) {
    // Log cases where the user started editing and then subsequently cleared
    // all the text.  Note that this explicitly doesn't catch cases like
    // "hit ctrl-l to select whole edit contents, then hit backspace", because
    // in such cases, |user_input_in_progress| won't be true here.
    UMA_HISTOGRAM_ENUMERATION(kOmniboxUserTextClearedHistogram,
                              OMNIBOX_USER_TEXT_CLEARED_BY_EDITING,
                              OMNIBOX_USER_TEXT_CLEARED_NUM_OF_ITEMS);
  }

  const bool no_selection =
      state_changes.new_sel_start == state_changes.new_sel_end;

  // Update the popup for the change, in the process changing to keyword mode
  // if the user hit space in mid-string after a keyword.
  // |allow_exact_keyword_match_| will be used by StartAutocomplete() method,
  // which will be called by |view_->UpdatePopup()|; so after that returns we
  // can safely reset this flag.
  allow_exact_keyword_match_ =
      state_changes.text_differs && allow_keyword_ui_change &&
      !state_changes.just_deleted_text && no_selection &&
      CreatedKeywordSearchByInsertingSpaceInMiddle(
          *state_changes.old_text, user_text_, state_changes.new_sel_start);
  view_->UpdatePopup();
  if (allow_exact_keyword_match_) {
    keyword_mode_entry_method_ = OmniboxEventProto::SPACE_IN_MIDDLE;
    EmitKeywordHistogram(OmniboxEventProto::SPACE_IN_MIDDLE);
    allow_exact_keyword_match_ = false;
  }

  if (!state_changes.text_differs || !allow_keyword_ui_change ||
      (state_changes.just_deleted_text && no_selection) ||
      is_keyword_selected() || (paste_state_ != NONE))
    return true;

  // If the user input a "?" at the beginning of the text, put them into
  // keyword mode for their default search provider.
  if ((state_changes.new_sel_start == 1) && (user_text_[0] == '?')) {
    EnterKeywordModeForDefaultSearchProvider(OmniboxEventProto::QUESTION_MARK);
    return false;
  }

  // Change to keyword mode if the user is now pressing space after a keyword
  // name.  Note that if this is the case, then even if there was no keyword
  // hint when we entered this function (e.g. if the user has used space to
  // replace some selected text that was adjoined to this keyword), there will
  // be one now because of the call to UpdatePopup() above; so it's safe for
  // MaybeAcceptKeywordBySpace() to look at |keyword_| and |is_keyword_hint_|
  // to determine what keyword, if any, is applicable.
  //
  // If MaybeAcceptKeywordBySpace() accepts the keyword and returns true, that
  // will have updated our state already, so in that case we don't also return
  // true from this function.
  return (state_changes.new_sel_start != user_text_.length()) ||
         !MaybeAcceptKeywordBySpace(user_text_);
}

// TODO(beaudoin): Merge OnPopupDataChanged with this method once the popup
// handling has completely migrated to omnibox_controller.
void OmniboxEditModel::OnCurrentMatchChanged() {
  has_temporary_text_ = false;

  const AutocompleteMatch& match = omnibox_controller_->current_match();

  client_->OnCurrentMatchChanged(match);

  // We store |keyword| and |is_keyword_hint| in temporary variables since
  // OnPopupDataChanged use their previous state to detect changes.
  base::string16 keyword;
  bool is_keyword_hint;
  TemplateURLService* service = client_->GetTemplateURLService();
  match.GetKeywordUIState(service, &keyword, &is_keyword_hint);
  if (popup_model())
    popup_model()->OnResultChanged();

  if (!is_keyword_selected() && !is_keyword_hint && !keyword.empty()) {
    // We just entered keyword mode, so remove the keyword from the input.
    // We don't call MaybeStripKeyword, as we haven't yet updated our internal
    // state (keyword_ and is_keyword_hint_), and MaybeStripKeyword checks this.
    user_text_ =
        KeywordProvider::SplitReplacementStringFromInput(user_text_, false);
    original_user_text_with_keyword_.clear();
  }

  // OnPopupDataChanged() resets OmniboxController's |current_match_| early
  // on.  Therefore, copy match.inline_autocompletion to a temp to preserve
  // its value across the entire call.
  const base::string16 inline_autocompletion(match.inline_autocompletion);
  OnPopupDataChanged(inline_autocompletion,
                     /*is_temporary_text=*/false, keyword, is_keyword_hint);
}

// static
const char OmniboxEditModel::kCutOrCopyAllTextHistogram[] =
    "Omnibox.CutOrCopyAllText";

void OmniboxEditModel::SetAccessibilityLabel(const AutocompleteMatch& match) {
  view_->SetAccessibilityLabel(view_->GetText(), match);
}

bool OmniboxEditModel::PopupIsOpen() const {
  return popup_model() && popup_model()->IsOpen();
}

void OmniboxEditModel::InternalSetUserText(const base::string16& text) {
  user_text_ = text;
  original_user_text_with_keyword_.clear();
  just_deleted_text_ = false;
  inline_autocomplete_text_.clear();
  view_->OnInlineAutocompleteTextCleared();
}

base::string16 OmniboxEditModel::MaybeStripKeyword(
    const base::string16& text) const {
  return is_keyword_selected() ?
      KeywordProvider::SplitReplacementStringFromInput(text, false) : text;
}

base::string16 OmniboxEditModel::MaybePrependKeyword(
    const base::string16& text) const {
  return is_keyword_selected() ? (keyword_ + base::char16(' ') + text) : text;
}

void OmniboxEditModel::GetInfoForCurrentText(AutocompleteMatch* match,
                                             GURL* alternate_nav_url) const {
  DCHECK(match);

  // If there's a query in progress or the popup is open, pick out the default
  // match or selected match, if there is one.
  bool found_match_for_text = false;
  if (query_in_progress() || PopupIsOpen()) {
    if (query_in_progress() && result().default_match()) {
      // The user cannot have manually selected a match, or the query would have
      // stopped. So the default match must be the desired selection.
      *match = *result().default_match();
      found_match_for_text = true;
    } else if (popup_model()->selected_line() != OmniboxPopupModel::kNoMatch) {
      const AutocompleteMatch& selected_match =
          result().match_at(popup_model()->selected_line());
      *match =
          (popup_model()->selected_line_state() == OmniboxPopupModel::KEYWORD) ?
              *selected_match.associated_keyword : selected_match;
      found_match_for_text = true;
    }
    if (found_match_for_text && alternate_nav_url &&
        (!popup_model() || !popup_model()->has_selected_match())) {
      *alternate_nav_url =
          AutocompleteResult::ComputeAlternateNavUrl(input_, *match);
    }
  }

  if (!found_match_for_text) {
    // For match generation, we use the unelided |url_for_editing_|, unless the
    // user input is in progress or Query in Omnibox is active.
    LocationBarModel* location_bar_model = controller()->GetLocationBarModel();
    base::string16 text_for_match_generation = url_for_editing_;
    if (user_input_in_progress() ||
        location_bar_model->GetDisplaySearchTerms(nullptr)) {
      text_for_match_generation = view_->GetText();
    }

    client_->GetAutocompleteClassifier()->Classify(
        MaybePrependKeyword(text_for_match_generation), is_keyword_selected(),
        true, GetPageClassification(), match, alternate_nav_url);
  }
}

void OmniboxEditModel::RevertTemporaryTextAndPopup() {
  // The user typed something, then selected a different item.  Restore the
  // text they typed and change back to the default item.
  // NOTE: This purposefully does not reset paste_state_.
  just_deleted_text_ = false;
  has_temporary_text_ = false;

  if (popup_model())
    popup_model()->ResetToInitialState();

  // There are two cases in which resetting to the default match doesn't restore
  // the proper original text:
  //  1. If user input is not in progress, we are reverting an on-focus
  //     suggestion. These may be unrelated to the original input.
  //  2. If there's no default match at all.
  //
  // The original selection will be restored in OnRevertTemporaryText() below.
  if (!user_input_in_progress_ || !result().default_match()) {
    view_->SetWindowTextAndCaretPos(input_.text(), /*caret_pos=*/0,
                                    /*update_popup=*/false,
                                    /*notify_text_changed=*/true);
  }

  const AutocompleteMatch& match = CurrentMatch(nullptr);
  view_->OnRevertTemporaryText(match.fill_into_edit, match);
}

bool OmniboxEditModel::MaybeAcceptKeywordBySpace(
    const base::string16& new_text) {
  size_t keyword_length = new_text.length() - 1;
  return is_keyword_hint_ && (keyword_.length() == keyword_length) &&
         IsSpaceCharForAcceptingKeyword(new_text[keyword_length]) &&
         !new_text.compare(0, keyword_length, keyword_, 0, keyword_length) &&
         AcceptKeyword(OmniboxEventProto::SPACE_AT_END);
}

bool OmniboxEditModel::CreatedKeywordSearchByInsertingSpaceInMiddle(
    const base::string16& old_text,
    const base::string16& new_text,
    size_t caret_position) const {
  DCHECK_GE(new_text.length(), caret_position);

  // Check simple conditions first.
  if ((paste_state_ != NONE) || (caret_position < 2) ||
      (old_text.length() < caret_position) ||
      (new_text.length() == caret_position))
    return false;
  size_t space_position = caret_position - 1;
  if (!IsSpaceCharForAcceptingKeyword(new_text[space_position]) ||
      base::IsUnicodeWhitespace(new_text[space_position - 1]) ||
      new_text.compare(0, space_position, old_text, 0, space_position) ||
      !new_text.compare(space_position, new_text.length() - space_position,
                        old_text, space_position,
                        old_text.length() - space_position)) {
    return false;
  }

  // Then check if the text before the inserted space matches a keyword.
  base::string16 keyword;
  base::TrimWhitespace(new_text.substr(0, space_position), base::TRIM_LEADING,
                       &keyword);
  return !keyword.empty() && !autocomplete_controller()->keyword_provider()->
      GetKeywordForText(keyword).empty();
}

//  static
bool OmniboxEditModel::IsSpaceCharForAcceptingKeyword(wchar_t c) {
  switch (c) {
    case 0x0020:  // Space
    case 0x3000:  // Ideographic Space
      return true;
    default:
      return false;
  }
}

void OmniboxEditModel::ClassifyString(const base::string16& text,
                                      AutocompleteMatch* match,
                                      GURL* alternate_nav_url) const {
  DCHECK(match);
  client_->GetAutocompleteClassifier()->Classify(
      text, false, false, GetPageClassification(), match, alternate_nav_url);
}

bool OmniboxEditModel::SetInputInProgressNoNotify(bool in_progress) {
  if (in_progress && !user_input_since_focus_) {
    base::TimeTicks now = base::TimeTicks::Now();
    DCHECK(last_omnibox_focus_ <= now);
    UMA_HISTOGRAM_TIMES(kFocusToEditTimeHistogram, now - last_omnibox_focus_);
    user_input_since_focus_ = true;
  }

  if (user_input_in_progress_ == in_progress)
    return false;

  user_input_in_progress_ = in_progress;
  if (user_input_in_progress_) {
    time_user_first_modified_omnibox_ = base::TimeTicks::Now();
    base::RecordAction(base::UserMetricsAction("OmniboxInputInProgress"));
    autocomplete_controller()->ResetSession();
  }
  return true;
}

void OmniboxEditModel::NotifyObserversInputInProgress(bool in_progress) {
  controller_->OnInputInProgress(in_progress);

  if (user_input_in_progress_ || !in_revert_)
    client_->OnInputStateChanged();
}

void OmniboxEditModel::SetFocusState(OmniboxFocusState state,
                                     OmniboxFocusChangeReason reason) {
  if (state == focus_state_)
    return;

  // Update state and notify view if the omnibox has focus and the caret
  // visibility changed.
  const bool was_caret_visible = is_caret_visible();
  focus_state_ = state;
  if (focus_state_ != OMNIBOX_FOCUS_NONE &&
      is_caret_visible() != was_caret_visible)
    view_->ApplyCaretVisibility();

  client_->OnFocusChanged(focus_state_, reason);
}
