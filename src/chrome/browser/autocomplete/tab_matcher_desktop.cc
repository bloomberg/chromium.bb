// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/tab_matcher_desktop.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/web_contents_user_data.h"

namespace {
class AutocompleteClientWebContentsUserData
    : public content::WebContentsUserData<
          AutocompleteClientWebContentsUserData> {
 public:
  ~AutocompleteClientWebContentsUserData() override = default;

  int GetLastCommittedEntryIndex() { return last_committed_entry_index_; }
  const GURL& GetLastCommittedStrippedURL() {
    return last_committed_stripped_url_;
  }
  void UpdateLastCommittedStrippedURL(
      int last_committed_index,
      const GURL& last_committed_url,
      const TemplateURLService* template_url_service) {
    if (last_committed_url.is_valid()) {
      last_committed_entry_index_ = last_committed_index;
      // Use blank input since we will re-use this stripped URL with other
      // inputs.
      last_committed_stripped_url_ = AutocompleteMatch::GURLToStrippedGURL(
          last_committed_url, AutocompleteInput(), template_url_service,
          std::u16string());
    }
  }

 private:
  explicit AutocompleteClientWebContentsUserData(
      content::WebContents* contents);
  friend class content::WebContentsUserData<
      AutocompleteClientWebContentsUserData>;

  int last_committed_entry_index_ = -1;
  GURL last_committed_stripped_url_;
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

AutocompleteClientWebContentsUserData::AutocompleteClientWebContentsUserData(
    content::WebContents* web_contents)
    : content::WebContentsUserData<AutocompleteClientWebContentsUserData>(
          *web_contents) {}

WEB_CONTENTS_USER_DATA_KEY_IMPL(AutocompleteClientWebContentsUserData);
}  // namespace

bool TabMatcherDesktop::IsTabOpenWithURL(const GURL& url,
                                         const AutocompleteInput* input) const {
  const AutocompleteInput empty_input;
  if (!input)
    input = &empty_input;
  const GURL stripped_url = AutocompleteMatch::GURLToStrippedGURL(
      url, *input, client_.GetTemplateURLService(), std::u16string());
  Browser* active_browser = BrowserList::GetInstance()->GetLastActive();
  content::WebContents* active_tab = nullptr;
  if (active_browser)
    active_tab = active_browser->tab_strip_model()->GetActiveWebContents();
  for (auto* browser : *BrowserList::GetInstance()) {
    // Only look at same profile (and anonymity level).
    if (profile_ == browser->profile()) {
      for (int i = 0; i < browser->tab_strip_model()->count(); ++i) {
        content::WebContents* web_contents =
            browser->tab_strip_model()->GetWebContentsAt(i);
        if (web_contents != active_tab &&
            IsStrippedURLEqualToWebContentsURL(stripped_url, web_contents))
          return true;
      }
    }
  }

  return false;
}

bool TabMatcherDesktop::IsStrippedURLEqualToWebContentsURL(
    const GURL& stripped_url,
    content::WebContents* web_contents) const {
  AutocompleteClientWebContentsUserData::CreateForWebContents(web_contents);
  AutocompleteClientWebContentsUserData* user_data =
      AutocompleteClientWebContentsUserData::FromWebContents(web_contents);
  DCHECK(user_data);
  if (user_data->GetLastCommittedEntryIndex() !=
      web_contents->GetController().GetLastCommittedEntryIndex()) {
    user_data->UpdateLastCommittedStrippedURL(
        web_contents->GetController().GetLastCommittedEntryIndex(),
        web_contents->GetLastCommittedURL(), client_.GetTemplateURLService());
  }
  return stripped_url == user_data->GetLastCommittedStrippedURL();
}
