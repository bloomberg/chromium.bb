// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_CONSTANTS_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_CONSTANTS_H_

namespace app_list {

// The maximum number of omnibox results to display if we have more results than
// can fit in the UI.
constexpr int kMaxOmniboxResults = 3;

// The maximum number of best matches to show.
constexpr size_t kNumBestMatches = 3u;

// The number of top-ranked best match results to stabilize during the
// post-burn-in period. Stabilized results retain their rank and are not
// displaced by later-arriving results
constexpr size_t kNumBestMatchesToStabilize = 1u;

// The score threshold before we consider a result a best match.
constexpr double kBestMatchThreshold = 0.8;

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_CONSTANTS_H_
