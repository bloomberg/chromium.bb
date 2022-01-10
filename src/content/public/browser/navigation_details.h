// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_NAVIGATION_DETAILS_H_
#define CONTENT_PUBLIC_BROWSER_NAVIGATION_DETAILS_H_

#include "base/memory/raw_ptr.h"
#include "content/common/content_export.h"
#include "content/public/browser/navigation_type.h"
#include "url/gurl.h"

namespace content {

class NavigationEntry;

// Provides the details of a committed navigation entry for the
// WebContentsObserver::NavigationEntryCommitted() notification.
struct CONTENT_EXPORT LoadCommittedDetails {
  // By default, the entry will be filled according to a new main frame
  // navigation.
  LoadCommittedDetails();
  LoadCommittedDetails(const LoadCommittedDetails&);
  LoadCommittedDetails& operator=(const LoadCommittedDetails&);

  // The committed entry. This will be the active entry in the controller.
  raw_ptr<NavigationEntry> entry = nullptr;

  // The type of navigation that just occurred. Note that not all types of
  // navigations in the enum are valid here, since some of them don't actually
  // cause a "commit" and won't generate this notification.
  content::NavigationType type = content::NAVIGATION_TYPE_UNKNOWN;

  // The index of the previously committed navigation entry. This will be -1
  // if there are no previous entries.
  int previous_entry_index = -1;

  // The previous main frame URL that the user was on. This may be empty if
  // there was no last committed entry.
  GURL previous_main_frame_url;

  // True if the committed entry has replaced the existing one. Note that in
  // case of subrames, the NavigationEntry and FrameNavigationEntry objects
  // don't actually get replaced - they're reused, but with updated attributes.
  bool did_replace_entry = false;

  // Whether the navigation happened without changing document. Examples of
  // same document navigations are:
  // * reference fragment navigations
  // * pushState/replaceState
  // * same page history navigation
  bool is_same_document = false;

  // True when the main frame was navigated. False means the navigation was a
  // sub-frame. Note that the main frame here means any main frame (including
  // fenced frames, main frames in BFCache or prerendering).
  bool is_main_frame = true;

  // True when the navigation triggered a prerender activation.
  bool is_prerender_activation = false;

  // Returns whether the main frame navigated to a different page (e.g., not
  // scrolling to a fragment inside the current page). We often need this logic
  // for showing or hiding something.
  bool is_navigation_to_different_page() const {
    return is_main_frame && !is_same_document;
  }

  // The HTTP status code for this entry..
  int http_status_code = 0;

  // True if the navigation used to get "ignored" (committed but won't create a
  // new NavigationEntry or modify an existing one) because there are no
  // NavigationEntries yet. Now that we will always have at least the initial
  // NavigationEntry, these navigations won't get ignored anymore, but Android
  //  WebView still needs this info to ignore the NavigationStateChanged() call
  // in some cases to avoid firing onPageFinished etc in more cases than it
  // previously did.
  // See also https://crbug.com/1277414.
  bool used_to_be_ignored_due_to_null_navigation_entry = false;
};

// Provides the details for a NOTIFICATION_NAV_ENTRY_CHANGED notification.
struct EntryChangedDetails {
  // The changed navigation entry after it has been updated.
  raw_ptr<NavigationEntry> changed_entry;

  // Indicates the current index in the back/forward list of the entry.
  int index;
};

// Details sent for NOTIFY_NAV_LIST_PRUNED.
struct PrunedDetails {
  // Index starting which |count| entries were removed.
  int index;

  // Number of items removed.
  int count;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_NAVIGATION_DETAILS_H_
