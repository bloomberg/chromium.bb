// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_SEARCH_HISTORY_H_
#define UI_APP_LIST_SEARCH_HISTORY_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "ui/app_list/app_list_export.h"
#include "ui/app_list/search/history_data_observer.h"
#include "ui/app_list/search/history_types.h"

namespace app_list {

class HistoryData;
class HistoryDataStore;

namespace test {
class SearchHistoryTest;
}

// History tracks the launch events of the search results and uses HistoryData
// to build user learning data out of these events. The learning data is stored
// as associations between user typed query and launched result id. There are
// primary and secondary associations. See HistoryData comments to see how
// they are built. The learning data is sent to the mixer to boost results that
// have been launched before.
class APP_LIST_EXPORT History : public KeyedService,
                                public HistoryDataObserver {
 public:
  explicit History(scoped_refptr<HistoryDataStore> store);
  ~History() override;

  // Returns true if the service is ready.
  bool IsReady() const;

  // Adds a launch event to the learning data.
  void AddLaunchEvent(const std::string& query, const std::string& result_id);

  // Gets all known search results that were launched using the given |query|
  // or using queries that |query| is a prefix of.
  scoped_ptr<KnownResults> GetKnownResults(const std::string& query) const;

 private:
  friend class app_list::test::SearchHistoryTest;

  // HistoryDataObserver overrides:
  void OnHistoryDataLoadedFromStore() override;

  scoped_ptr<HistoryData> data_;
  scoped_refptr<HistoryDataStore> store_;
  bool data_loaded_;

  DISALLOW_COPY_AND_ASSIGN(History);
};

}  // namespace app_list

#endif  // UI_APP_LIST_SEARCH_HISTORY_H_
