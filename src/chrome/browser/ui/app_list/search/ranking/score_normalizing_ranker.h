// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_SCORE_NORMALIZING_RANKER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_SCORE_NORMALIZING_RANKER_H_

#include "base/containers/flat_map.h"
#include "chrome/browser/ui/app_list/search/ranking/ranker.h"
#include "chrome/browser/ui/app_list/search/util/persistent_proto.h"

namespace app_list {

class ScoreNormalizerProto;

// A ranker that transforms the result scores of search providers into something
// close to a uniform distribution. This is done:
//
// - Per-provider. An independent transformation is learned for each provider.
// - In aggregate. The overall long-running transformed score distribution for a
//   provider will be close to uniform, but any given batch of search results -
//   eg. for one query - may not be.
//
// Some providers don't have any transformation applied, see
// ShouldIgnoreProvider in the implementation for details.
//
// TODO(crbug.com/1199206): This was made a no-op after stability concerns, but
// will be re-added soon.
class ScoreNormalizingRanker : public Ranker {
 public:
  explicit ScoreNormalizingRanker(PersistentProto<ScoreNormalizerProto> proto);
  ~ScoreNormalizingRanker() override;

  ScoreNormalizingRanker(const ScoreNormalizingRanker&) = delete;
  ScoreNormalizingRanker& operator=(const ScoreNormalizingRanker&) = delete;

  // Ranker:
  void UpdateResultRanks(ResultsMap& results, ProviderType provider) override;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_SCORE_NORMALIZING_RANKER_H_
