// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_RECURRENCE_RANKER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_RECURRENCE_RANKER_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_ranker_config.pb.h"

namespace app_list {

class RecurrencePredictor;
class RecurrenceRankerProto;

// |RecurrenceRanker| is the public interface of the ranking system. The methods
// of interest are:
//  - Record, Rename, and Remove, for modifying the targets stored in the
//  ranker.
//  - Rank, for retrieving rankings under the current conditions.
// The class can be configured to use different predictors, via
// |app_list_features|.
class RecurrenceRanker {
 public:
  RecurrenceRanker(const base::FilePath& filepath,
                   const RecurrenceRankerConfigProto& config,
                   bool is_ephemeral_user);

  ~RecurrenceRanker();

  // Record the use of a given target, and train the predictor on it. The one
  // argument version is a shortcut for an empty query string, useful for
  // zero-state scenarios.
  void Record(const std::string& target);
  void Record(const std::string& target, const std::string& query);
  // Rename a target, while keeping learned information on it.
  void Rename(const std::string& target, const std::string& new_target);
  // Remove a target entirely.
  void Remove(const std::string& target);

  // Returns a map of target to score.
  //  - Higher scores are better.
  //  - Score are guaranteed to be in the range [0,1].
  // The zero-argument version is a shortcut for an empty query string.
  base::flat_map<std::string, float> Rank();
  base::flat_map<std::string, float> Rank(const std::string& query);

  // Returns a sorted vector of <target, score> pairs.
  //  - Higher scores are better.
  //  - Score are guaranteed to be in the range [0,1].
  //  - Pairs are sorted in descending order of score.
  //  - At most n results will be returned.
  // The one-argument version is a shortcut for an empty query string.
  std::vector<std::pair<std::string, float>> RankTopN(int n);
  std::vector<std::pair<std::string, float>> RankTopN(int n,
                                                      const std::string& query);

 private:
  FRIEND_TEST_ALL_PREFIXES(RecurrenceRankerTest,
                           EphemeralUsersUseFrecencyPredictor);
  FRIEND_TEST_ALL_PREFIXES(RecurrenceRankerTest, InitializeIfNoFileExists);
  FRIEND_TEST_ALL_PREFIXES(RecurrenceRankerTest,
                           SavedRankerRejectedIfConfigMismatched);
  FRIEND_TEST_ALL_PREFIXES(RecurrenceRankerTest, LoadFromDisk);
  FRIEND_TEST_ALL_PREFIXES(RecurrenceRankerTest, SaveToDisk);

  // Finishes initialisation by populating |this| with data from the given
  // proto.
  void OnLoadProtoFromDiskComplete(
      std::unique_ptr<RecurrenceRankerProto> proto);

  void MaybeSave();
  void ToProto(RecurrenceRankerProto* proto);
  void FromProto(const RecurrenceRankerProto& proto);

  const char* GetPredictorNameForTesting() const;
  void ForceSaveOnNextUpdateForTesting();

  // Internal predictor that drives ranking.
  std::unique_ptr<RecurrencePredictor> predictor_;

  // Where to save the ranker.
  const base::FilePath proto_filepath_;
  // Hash of client-supplied config, used for associating a serialised ranker
  // with a particular config.
  unsigned int config_hash_;

  // Flag indicating the model has finished being loaded and is ready for use.
  bool load_from_disk_completed_ = false;
  // Whether the current user will have their home directory wiped at the end of
  // the session. We adjust the behaviour of the ranker based on this.
  bool is_ephemeral_user_;

  base::TimeDelta min_seconds_between_saves_;
  base::Time time_of_last_save_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::WeakPtrFactory<RecurrenceRanker> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(RecurrenceRanker);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_RECURRENCE_RANKER_H_
