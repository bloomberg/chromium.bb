// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREVIEWS_PREVIEWS_CONTENT_UTIL_H_
#define CHROME_BROWSER_PREVIEWS_PREVIEWS_CONTENT_UTIL_H_

#include "components/previews/core/previews_decider.h"
#include "content/public/common/previews_state.h"

namespace content {
class NavigationHandle;
}

namespace previews {

// Returns whether |previews_state| has any enabled previews.
bool HasEnabledPreviews(content::PreviewsState previews_state);

// Returns the bitmask of enabled client-side previews for |url| and the
// current effective network connection given |previews_decider|.
// This handles the mapping of previews::PreviewsType enum values to bitmask
// definitions for content::PreviewsState.
// |is_reload| is used to eliminate certain preview types, and |previews_data|
// is populated with relevant information.
// TODO(ryansturm): |navigation_handle| has all of the other information, so
// remove extra arguments. https://crbug.com/934400
content::PreviewsState DetermineAllowedClientPreviewsState(
    previews::PreviewsUserData* previews_data,
    const GURL& url,
    bool is_reload,
    bool is_redirect,
    bool is_data_saver_user,
    previews::PreviewsDecider* previews_decider,
    content::NavigationHandle* navigation_handle);

// Returns an updated PreviewsState given |previews_state| that has already
// been updated wrt server previews. This should be called at Navigation Commit
// time. It will defer to any server preview set, otherwise it chooses which
// client preview bits to retain for processing the main frame response.
content::PreviewsState DetermineCommittedClientPreviewsState(
    previews::PreviewsUserData* previews_data,
    const GURL& url,
    content::PreviewsState previews_state,
    const previews::PreviewsDecider* previews_decider,
    content::NavigationHandle* navigation_handle);

// Returns the effective PreviewsType known on a main frame basis given the
// |previews_state| bitmask for the committed main frame. This uses the same
// previews precendence consideration as |DetermineCommittedClientPreviewsState|
// in case it is called on a PreviewsState value that has not been filtered
// through that method.
previews::PreviewsType GetMainFramePreviewsType(
    content::PreviewsState previews_state);

}  // namespace previews

#endif  // CHROME_BROWSER_PREVIEWS_PREVIEWS_CONTENT_UTIL_H_
