// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/find_bar/find_bar_controller.h"

#include <algorithm>

#include "base/check_op.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "chrome/browser/ui/find_bar/find_bar.h"
#include "chrome/browser/ui/find_bar/find_bar_state.h"
#include "chrome/browser/ui/find_bar/find_bar_state_factory.h"
#include "components/find_in_page/find_tab_helper.h"
#include "components/find_in_page/find_types.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/range/range.h"

using content::NavigationController;
using content::WebContents;

FindBarController::FindBarController(std::unique_ptr<FindBar> find_bar)
    : find_bar_(std::move(find_bar)),
      find_bar_platform_helper_(FindBarPlatformHelper::Create(this)) {}

FindBarController::~FindBarController() {
  DCHECK(!web_contents_);
}

void FindBarController::Show(bool find_next, bool forward_direction) {
  find_in_page::FindTabHelper* find_tab_helper =
      find_in_page::FindTabHelper::FromWebContents(web_contents_);

  // Only show the animation if we're not already showing a find bar for the
  // selected WebContents.
  if (!find_tab_helper->find_ui_active()) {
    has_user_modified_text_ = false;
    MaybeSetPrepopulateText();

    find_tab_helper->set_find_ui_active(true);
    find_bar_->Show(true);
  }
  find_bar_->SetFocusAndSelection();

  base::string16 find_text;

#if defined(OS_MACOSX)
  if (find_next) {
    // For macOS, we always want to search for the current contents of the find
    // bar on OS X, rather than the behavior we'd get with empty find_text
    // (see FindBarState::GetSearchPrepopulateText).
    find_text = find_bar_->GetFindText();
  }
#endif

  if (!find_next && !has_user_modified_text_) {
    base::string16 selected_text = GetSelectedText();
    auto selected_length = selected_text.length();
    if (selected_length > 0 && selected_length <= 250) {
      find_text = selected_text;
      find_bar_->SetFindTextAndSelectedRange(find_text,
                                             gfx::Range(0, find_text.length()));
      if (web_contents_) {
        // Collapse the selection to its start, so we can run a find_next and
        // make it find the selection. This is a no-op in terms of what ends
        // up selected, but initializes the rest of the find machinery (like
        // showing how many matches there are in the document).
        web_contents_->AdjustSelectionByCharacterOffset(0, -selected_length,
                                                        false);
        find_next = true;
      }
    }
  }

  if (find_next)
    find_tab_helper->StartFinding(find_text, forward_direction, false);
}

void FindBarController::EndFindSession(
    find_in_page::SelectionAction selection_action,
    find_in_page::ResultAction result_action) {
  find_bar_->Hide(true);

  // |web_contents_| can be NULL for a number of reasons, for example when the
  // tab is closing. We must guard against that case. See issue 8030.
  if (web_contents_) {
    find_in_page::FindTabHelper* find_tab_helper =
        find_in_page::FindTabHelper::FromWebContents(web_contents_);

    // When we hide the window, we need to notify the renderer that we are done
    // for now, so that we can abort the scoping effort and clear all the
    // tickmarks and highlighting.
    find_tab_helper->StopFinding(selection_action);

    if (result_action == find_in_page::ResultAction::kClear)
      find_bar_->ClearResults(find_tab_helper->find_result());

    // When we get dismissed we restore the focus to where it belongs.
    find_bar_->RestoreSavedFocus();
  }
}

void FindBarController::ChangeWebContents(WebContents* contents) {
  if (web_contents_) {
    registrar_.RemoveAll();
    find_bar_->StopAnimation();

    find_in_page::FindTabHelper* find_tab_helper =
        find_in_page::FindTabHelper::FromWebContents(web_contents_);
    if (find_tab_helper) {
      find_tab_helper->set_selected_range(find_bar_->GetSelectedRange());
      find_tab_observer_.Remove(find_tab_helper);
    }
  }

  web_contents_ = contents;
  find_in_page::FindTabHelper* find_tab_helper =
      web_contents_
          ? find_in_page::FindTabHelper::FromWebContents(web_contents_)
          : nullptr;
  if (find_tab_helper)
    find_tab_observer_.Add(find_tab_helper);

  // Hide any visible find window from the previous tab if a NULL tab contents
  // is passed in or if the find UI is not active in the new tab.
  if (find_bar_->IsFindBarVisible() &&
      (!find_tab_helper || !find_tab_helper->find_ui_active())) {
    find_bar_->Hide(false);
  }

  if (!web_contents_)
    return;

  registrar_.Add(
      this,
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::Source<NavigationController>(&web_contents_->GetController()));

  MaybeSetPrepopulateText();

  if (find_tab_helper && find_tab_helper->find_ui_active()) {
    // A tab with a visible find bar just got selected and we need to show the
    // find bar but without animation since it was already animated into its
    // visible state. We also want to reset the window location so that
    // we don't surprise the user by popping up to the left for no apparent
    // reason.
    find_bar_->Show(false);
  }

  UpdateFindBarForCurrentResult();
  find_bar_->UpdateFindBarForChangedWebContents();
}

void FindBarController::SetText(base::string16 text) {
  find_bar_->SetFindTextAndSelectedRange(text, find_bar_->GetSelectedRange());
}

void FindBarController::OnUserChangedFindText(base::string16 text) {
  has_user_modified_text_ = !text.empty();

  if (find_bar_platform_helper_)
    find_bar_platform_helper_->OnUserChangedFindText(text);
}

////////////////////////////////////////////////////////////////////////////////
// FindBarController, content::NotificationObserver implementation:

void FindBarController::Observe(int type,
                                const content::NotificationSource& source,
                                const content::NotificationDetails& details) {
  DCHECK_EQ(content::NOTIFICATION_NAV_ENTRY_COMMITTED, type);
  NavigationController* source_controller =
      content::Source<NavigationController>(source).ptr();
  if (source_controller == &web_contents_->GetController()) {
    content::LoadCommittedDetails* commit_details =
        content::Details<content::LoadCommittedDetails>(details).ptr();
    // Hide the find bar on navigation.
    if (find_bar_->IsFindBarVisible() && commit_details->is_main_frame &&
        commit_details->is_navigation_to_different_page()) {
      EndFindSession(find_in_page::SelectionAction::kKeep,
                     find_in_page::ResultAction::kClear);
    }
  }
}

void FindBarController::OnFindResultAvailable(
    content::WebContents* web_contents) {
  DCHECK_EQ(web_contents, web_contents_);
  UpdateFindBarForCurrentResult();

  find_in_page::FindTabHelper* find_tab_helper =
      find_in_page::FindTabHelper::FromWebContents(web_contents_);

  // Only "final" results may audibly alert the user.
  if (!find_tab_helper->find_result().final_update())
    return;

  const base::string16& current_search = find_tab_helper->find_text();

  // If no results were found, play an audible alert (depending upon platform
  // convention). Alert only once per unique search, and don't alert on
  // backspace.
  if ((find_tab_helper->find_result().number_of_matches() == 0) &&
      (current_search != find_tab_helper->last_completed_find_text() &&
       !base::StartsWith(find_tab_helper->previous_find_text(), current_search,
                         base::CompareCase::SENSITIVE))) {
    find_bar_->AudibleAlert();
  }

  // Record the completion of the search to suppress future alerts, even if the
  // page's contents change.
  find_tab_helper->set_last_completed_find_text(current_search);
}

void FindBarController::UpdateFindBarForCurrentResult() {
  find_in_page::FindTabHelper* find_tab_helper =
      find_in_page::FindTabHelper::FromWebContents(web_contents_);
  const find_in_page::FindNotificationDetails& find_result =
      find_tab_helper->find_result();
  // Avoid bug 894389: When a new search starts (and finds something) it reports
  // an interim match count result of 1 before the scoping effort starts. This
  // is to provide feedback as early as possible that we will find something.
  // As you add letters to the search term, this creates a flashing effect when
  // we briefly show "1 of 1" matches because there is a slight delay until
  // the scoping effort starts updating the match count. We avoid this flash by
  // ignoring interim results of 1 if we already have a positive number.
  if (find_result.number_of_matches() > -1) {
    if (last_reported_matchcount_ > 0 && find_result.number_of_matches() == 1 &&
        !find_result.final_update() &&
        last_reported_ordinal_ == find_result.active_match_ordinal()) {
      return;  // Don't let interim result override match count.
    }
    last_reported_matchcount_ = find_result.number_of_matches();
    last_reported_ordinal_ = find_result.active_match_ordinal();
  }

  find_bar_->UpdateUIForFindResult(find_result, find_tab_helper->find_text());
}

void FindBarController::MaybeSetPrepopulateText() {
  // Having a per-tab find_string is not compatible with a global find
  // pasteboard, so we always have the same find text in all find bars. This is
  // done through the find pasteboard mechanism, so don't set the text here.
  if (find_bar_->HasGlobalFindPasteboard())
    return;

  // Find out what we should show in the find text box. Usually, this will be
  // the last search in this tab, but if no search has been issued in this tab
  // we use the last search string (from any tab).
  find_in_page::FindTabHelper* find_tab_helper =
      find_in_page::FindTabHelper::FromWebContents(web_contents_);
  base::string16 find_string = find_tab_helper->find_text();
  if (find_string.empty())
    find_string = find_tab_helper->GetInitialSearchText();

  // Update the find bar with existing results and search text, regardless of
  // whether or not the find bar is visible, so that if it's subsequently
  // shown it is showing the right state for this tab. We update the find text
  // _first_ since the FindBarView checks its emptiness to see if it should
  // clear the result count display when there's nothing in the box.
  find_bar_->SetFindTextAndSelectedRange(find_string,
                                         find_tab_helper->selected_range());
}

base::string16 FindBarController::GetSelectedText() {
  auto* host_view = web_contents_->GetRenderWidgetHostView();
  if (!host_view)
    return base::string16();

  base::string16 selected_text = host_view->GetSelectedText();
  // This should be kept in sync with what TextfieldModel::Paste() does, since
  // that's what would run if the user explicitly pasted this text into the find
  // bar.
  base::TrimWhitespace(selected_text, base::TRIM_ALL, &selected_text);
  return selected_text;
}
