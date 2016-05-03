// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/search/mixer.h"

#include <stddef.h>

#include <set>
#include <string>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_vector.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_entropy_provider.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/app_list/app_list_model.h"
#include "ui/app_list/search/history_types.h"
#include "ui/app_list/search_provider.h"
#include "ui/app_list/search_result.h"

namespace app_list {
namespace test {

// Maximum number of results to show in each mixer group.
const size_t kMaxAppsGroupResults = 4;
// Ignored unless AppListMixer field trial is "Blended".
const size_t kMaxOmniboxResults = 4;
const size_t kMaxWebstoreResults = 2;

class TestSearchResult : public SearchResult {
 public:
  TestSearchResult(const std::string& id, double relevance)
      : instance_id_(instantiation_count++) {
    set_id(id);
    set_title(base::UTF8ToUTF16(id));
    set_relevance(relevance);
  }
  ~TestSearchResult() override {}

  using SearchResult::set_voice_result;

  // SearchResult overrides:
  void Open(int event_flags) override {}
  void InvokeAction(int action_index, int event_flags) override {}
  std::unique_ptr<SearchResult> Duplicate() const override {
    return base::WrapUnique(new TestSearchResult(id(), relevance()));
  }

  // For reference equality testing. (Addresses cannot be used to test reference
  // equality because it is possible that an object will be allocated at the
  // same address as a previously deleted one.)
  static int GetInstanceId(SearchResult* result) {
    return static_cast<const TestSearchResult*>(result)->instance_id_;
  }

 private:
  static int instantiation_count;

  int instance_id_;

  DISALLOW_COPY_AND_ASSIGN(TestSearchResult);
};
int TestSearchResult::instantiation_count = 0;

class TestSearchProvider : public SearchProvider {
 public:
  explicit TestSearchProvider(const std::string& prefix)
      : prefix_(prefix),
        count_(0),
        bad_relevance_range_(false),
        display_type_(SearchResult::DISPLAY_LIST) {}
  ~TestSearchProvider() override {}

  // SearchProvider overrides:
  void Start(bool is_voice_query, const base::string16& query) override {
    ClearResults();
    for (size_t i = 0; i < count_; ++i) {
      const std::string id =
          base::StringPrintf("%s%d", prefix_.c_str(), static_cast<int>(i));
      double relevance = 1.0 - i / 10.0;
      // If bad_relevance_range_, change the relevances to give results outside
      // of the canonical [0.0, 1.0] range.
      if (bad_relevance_range_)
        relevance = 10.0 - i * 10;
      TestSearchResult* result = new TestSearchResult(id, relevance);
      result->set_display_type(display_type_);
      if (voice_result_indices.find(i) != voice_result_indices.end())
        result->set_voice_result(true);
      Add(std::unique_ptr<SearchResult>(result));
    }
  }
  void Stop() override {}

  void set_prefix(const std::string& prefix) { prefix_ = prefix; }
  void set_display_type(SearchResult::DisplayType display_type) {
    display_type_ = display_type;
  }
  void set_count(size_t count) { count_ = count; }
  void set_as_voice_result(size_t index) { voice_result_indices.insert(index); }
  void set_bad_relevance_range() { bad_relevance_range_ = true; }

 private:
  std::string prefix_;
  size_t count_;
  bool bad_relevance_range_;
  SearchResult::DisplayType display_type_;
  // Indices of results that will have the |voice_result| flag set.
  std::set<size_t> voice_result_indices;

  DISALLOW_COPY_AND_ASSIGN(TestSearchProvider);
};

// Test is parameterized with bool. True enables the "Blended" field trial.
class MixerTest : public testing::Test,
                  public testing::WithParamInterface<bool> {
 public:
  MixerTest()
      : is_voice_query_(false),
        field_trial_list_(new base::MockEntropyProvider()) {}
  ~MixerTest() override {}

  // testing::Test overrides:
  void SetUp() override {
    // If the parameter is true, enable the field trial.
    const char* field_trial_name = GetParam() ? "Blended" : "Control";
    base::FieldTrialList::CreateFieldTrial("AppListMixer", field_trial_name);

    results_.reset(new AppListModel::SearchResults);

    providers_.push_back(new TestSearchProvider("app"));
    providers_.push_back(new TestSearchProvider("omnibox"));
    providers_.push_back(new TestSearchProvider("webstore"));

    is_voice_query_ = false;

    mixer_.reset(new Mixer(results_.get()));

    size_t apps_group_id = mixer_->AddGroup(kMaxAppsGroupResults, 3.0, 1.0);
    size_t omnibox_group_id =
        mixer_->AddOmniboxGroup(kMaxOmniboxResults, 2.0, 1.0);
    size_t webstore_group_id = mixer_->AddGroup(kMaxWebstoreResults, 1.0, 0.5);

    mixer_->AddProviderToGroup(apps_group_id, providers_[0]);
    mixer_->AddProviderToGroup(omnibox_group_id, providers_[1]);
    mixer_->AddProviderToGroup(webstore_group_id, providers_[2]);
  }

  void RunQuery() {
    const base::string16 query;

    for (size_t i = 0; i < providers_.size(); ++i) {
      providers_[i]->Start(is_voice_query_, query);
      providers_[i]->Stop();
    }

    mixer_->MixAndPublish(is_voice_query_, known_results_);
  }

  std::string GetResults() const {
    std::string result;
    for (size_t i = 0; i < results_->item_count(); ++i) {
      if (!result.empty())
        result += ',';

      result += base::UTF16ToUTF8(results_->GetItemAt(i)->title());
    }

    return result;
  }

  Mixer* mixer() { return mixer_.get(); }
  TestSearchProvider* app_provider() { return providers_[0]; }
  TestSearchProvider* omnibox_provider() { return providers_[1]; }
  TestSearchProvider* webstore_provider() { return providers_[2]; }

  // Sets whether test runs should be treated as a voice query.
  void set_is_voice_query(bool is_voice_query) {
    is_voice_query_ = is_voice_query;
  }

  void AddKnownResult(const std::string& id, KnownResultType type) {
    known_results_[id] = type;
  }

 private:
  std::unique_ptr<Mixer> mixer_;
  std::unique_ptr<AppListModel::SearchResults> results_;
  KnownResults known_results_;

  bool is_voice_query_;

  ScopedVector<TestSearchProvider> providers_;

  base::FieldTrialList field_trial_list_;

  DISALLOW_COPY_AND_ASSIGN(MixerTest);
};

TEST_P(MixerTest, Basic) {
  // Note: Some cases in |expected_blended| have vastly more results than
  // others, due to the "at least 6" mechanism. If it gets at least 6 results
  // from all providers, it stops at 6. If not, it fetches potentially many more
  // results from all providers. Not ideal, but currently by design.
  struct TestCase {
    const size_t app_results;
    const size_t omnibox_results;
    const size_t webstore_results;
    const char* expected_default;  // Expected results with trial off.
    const char* expected_blended;  // Expected results with trial on.
  } kTestCases[] = {
      {0, 0, 0, "", ""},
      {10, 0, 0, "app0,app1,app2,app3",
       "app0,app1,app2,app3,app4,app5,app6,app7,app8,app9"},
      {0, 0, 10, "webstore0,webstore1",
       "webstore0,webstore1,webstore2,webstore3,webstore4,webstore5,webstore6,"
       "webstore7,webstore8,webstore9"},
      {4, 6, 0, "app0,app1,app2,app3,omnibox0,omnibox1",
       "app0,omnibox0,app1,omnibox1,app2,omnibox2,app3,omnibox3"},
      {4, 6, 2, "app0,app1,app2,app3,omnibox0,webstore0",
       "app0,omnibox0,app1,omnibox1,app2,omnibox2,app3,omnibox3,webstore0,"
       "webstore1"},
      {10, 10, 10, "app0,app1,app2,app3,omnibox0,webstore0",
       "app0,omnibox0,app1,omnibox1,app2,omnibox2,app3,omnibox3,webstore0,"
       "webstore1"},
      {0, 10, 0, "omnibox0,omnibox1,omnibox2,omnibox3,omnibox4,omnibox5",
       "omnibox0,omnibox1,omnibox2,omnibox3,omnibox4,omnibox5,omnibox6,"
       "omnibox7,omnibox8,omnibox9"},
      {0, 10, 1, "omnibox0,omnibox1,omnibox2,omnibox3,omnibox4,webstore0",
       "omnibox0,omnibox1,omnibox2,omnibox3,webstore0,omnibox4,omnibox5,"
       "omnibox6,omnibox7,omnibox8,omnibox9"},
      {0, 10, 2, "omnibox0,omnibox1,omnibox2,omnibox3,webstore0,webstore1",
       "omnibox0,omnibox1,omnibox2,omnibox3,webstore0,webstore1"},
      {1, 10, 0, "app0,omnibox0,omnibox1,omnibox2,omnibox3,omnibox4",
       "app0,omnibox0,omnibox1,omnibox2,omnibox3,omnibox4,omnibox5,omnibox6,"
       "omnibox7,omnibox8,omnibox9"},
      {2, 10, 0, "app0,app1,omnibox0,omnibox1,omnibox2,omnibox3",
       "app0,omnibox0,app1,omnibox1,omnibox2,omnibox3"},
      {2, 10, 1, "app0,app1,omnibox0,omnibox1,omnibox2,webstore0",
       "app0,omnibox0,app1,omnibox1,omnibox2,omnibox3,webstore0"},
      {2, 10, 2, "app0,app1,omnibox0,omnibox1,webstore0,webstore1",
       "app0,omnibox0,app1,omnibox1,omnibox2,omnibox3,webstore0,webstore1"},
      {2, 0, 2, "app0,app1,webstore0,webstore1",
       "app0,app1,webstore0,webstore1"},
      {0, 0, 0, "", ""},
  };

  for (size_t i = 0; i < arraysize(kTestCases); ++i) {
    app_provider()->set_count(kTestCases[i].app_results);
    omnibox_provider()->set_count(kTestCases[i].omnibox_results);
    webstore_provider()->set_count(kTestCases[i].webstore_results);
    RunQuery();

    const char* expected = GetParam() ? kTestCases[i].expected_blended
                                      : kTestCases[i].expected_default;
    EXPECT_EQ(expected, GetResults()) << "Case " << i;
  }
}

TEST_P(MixerTest, RemoveDuplicates) {
  const std::string dup = "dup";

  // This gives "dup0,dup1,dup2".
  app_provider()->set_prefix(dup);
  app_provider()->set_count(3);

  // This gives "dup0,dup1".
  omnibox_provider()->set_prefix(dup);
  omnibox_provider()->set_count(2);

  // This gives "dup0".
  webstore_provider()->set_prefix(dup);
  webstore_provider()->set_count(1);

  RunQuery();

  // Only three results with unique id are kept.
  EXPECT_EQ("dup0,dup1,dup2", GetResults());
}

// Tests that "known results" have priority over others.
TEST_P(MixerTest, KnownResultsPriority) {
  // This gives omnibox 0 -- 5.
  omnibox_provider()->set_count(6);

  // omnibox 1 -- 4 are "known results".
  AddKnownResult("omnibox1", PREFIX_SECONDARY);
  AddKnownResult("omnibox2", PERFECT_SECONDARY);
  AddKnownResult("omnibox3", PREFIX_PRIMARY);
  AddKnownResult("omnibox4", PERFECT_PRIMARY);

  RunQuery();

  // omnibox 1 -- 4 should be prioritised over the others. They should be
  // ordered 4, 3, 2, 1 (in order of match quality).
  EXPECT_EQ("omnibox4,omnibox3,omnibox2,omnibox1,omnibox0,omnibox5",
            GetResults());
}

// Tests that "known results" are not considered for recommendation results.
TEST_P(MixerTest, KnownResultsIgnoredForRecommendations) {
  // This gives omnibox 0 -- 5.
  omnibox_provider()->set_count(6);
  omnibox_provider()->set_display_type(SearchResult::DISPLAY_RECOMMENDATION);

  // omnibox 1 -- 4 are "known results".
  AddKnownResult("omnibox1", PREFIX_SECONDARY);
  AddKnownResult("omnibox2", PERFECT_SECONDARY);
  AddKnownResult("omnibox3", PREFIX_PRIMARY);
  AddKnownResult("omnibox4", PERFECT_PRIMARY);

  RunQuery();

  // omnibox 1 -- 4 should be unaffected despite being known results.
  EXPECT_EQ("omnibox0,omnibox1,omnibox2,omnibox3,omnibox4,omnibox5",
            GetResults());
}

TEST_P(MixerTest, VoiceQuery) {
  omnibox_provider()->set_count(3);
  RunQuery();
  EXPECT_EQ("omnibox0,omnibox1,omnibox2", GetResults());

  // Set "omnibox1" as a voice result. Do not expect any changes (as this is not
  // a voice query).
  omnibox_provider()->set_as_voice_result(1);
  RunQuery();
  EXPECT_EQ("omnibox0,omnibox1,omnibox2", GetResults());

  // Perform a voice query. Expect voice result first.
  set_is_voice_query(true);
  RunQuery();
  EXPECT_EQ("omnibox1,omnibox0,omnibox2", GetResults());

  // All voice results should appear before non-voice results.
  omnibox_provider()->set_as_voice_result(2);
  RunQuery();
  EXPECT_EQ("omnibox1,omnibox2,omnibox0", GetResults());
}

TEST_P(MixerTest, Publish) {
  std::unique_ptr<SearchResult> result1(new TestSearchResult("app1", 0));
  std::unique_ptr<SearchResult> result2(new TestSearchResult("app2", 0));
  std::unique_ptr<SearchResult> result3(new TestSearchResult("app3", 0));
  std::unique_ptr<SearchResult> result3_copy = result3->Duplicate();
  std::unique_ptr<SearchResult> result4(new TestSearchResult("app4", 0));
  std::unique_ptr<SearchResult> result5(new TestSearchResult("app5", 0));

  AppListModel::SearchResults ui_results;

  // Publish the first three results to |ui_results|.
  Mixer::SortedResults new_results;
  new_results.push_back(Mixer::SortData(result1.get(), 1.0f));
  new_results.push_back(Mixer::SortData(result2.get(), 1.0f));
  new_results.push_back(Mixer::SortData(result3.get(), 1.0f));

  Mixer::Publish(new_results, &ui_results);
  EXPECT_EQ(3u, ui_results.item_count());
  // The objects in |ui_results| should be new copies because the input results
  // are owned and |ui_results| needs to own its results as well.
  EXPECT_NE(TestSearchResult::GetInstanceId(new_results[0].result),
            TestSearchResult::GetInstanceId(ui_results.GetItemAt(0)));
  EXPECT_NE(TestSearchResult::GetInstanceId(new_results[1].result),
            TestSearchResult::GetInstanceId(ui_results.GetItemAt(1)));
  EXPECT_NE(TestSearchResult::GetInstanceId(new_results[2].result),
            TestSearchResult::GetInstanceId(ui_results.GetItemAt(2)));

  // Save the current |ui_results| instance ids for comparison later.
  std::vector<int> old_ui_result_ids;
  for (size_t i = 0; i < ui_results.item_count(); ++i) {
    old_ui_result_ids.push_back(
        TestSearchResult::GetInstanceId(ui_results.GetItemAt(i)));
  }

  // Change the first result to a totally new object (with a new ID).
  new_results[0] = Mixer::SortData(result4.get(), 1.0f);

  // Change the second result's title, but keep the same id. (The result will
  // keep the id "app2" but change its title to "New App 2 Title".)
  const base::string16 kNewAppTitle = base::UTF8ToUTF16("New App 2 Title");
  new_results[1].result->set_title(kNewAppTitle);

  // Change the third result's object address (it points to an object with the
  // same data).
  new_results[2] = Mixer::SortData(result3_copy.get(), 1.0f);

  Mixer::Publish(new_results, &ui_results);
  EXPECT_EQ(3u, ui_results.item_count());

  // The first result will be a new object, as the ID has changed.
  EXPECT_NE(old_ui_result_ids[0],
            TestSearchResult::GetInstanceId(ui_results.GetItemAt(0)));

  // The second result will still use the original object, but have a different
  // title, since the ID did not change.
  EXPECT_EQ(old_ui_result_ids[1],
            TestSearchResult::GetInstanceId(ui_results.GetItemAt(1)));
  EXPECT_EQ(kNewAppTitle, ui_results.GetItemAt(1)->title());

  // The third result will use the original object as the ID did not change.
  EXPECT_EQ(old_ui_result_ids[2],
            TestSearchResult::GetInstanceId(ui_results.GetItemAt(2)));

  // Save the current |ui_results| order which should is app4, app2, app3.
  old_ui_result_ids.clear();
  for (size_t i = 0; i < ui_results.item_count(); ++i) {
    old_ui_result_ids.push_back(
        TestSearchResult::GetInstanceId(ui_results.GetItemAt(i)));
  }

  // Reorder the existing results and add a new one in the second place.
  new_results[0] = Mixer::SortData(result2.get(), 1.0f);
  new_results[1] = Mixer::SortData(result5.get(), 1.0f);
  new_results[2] = Mixer::SortData(result3.get(), 1.0f);
  new_results.push_back(Mixer::SortData(result4.get(), 1.0f));

  Mixer::Publish(new_results, &ui_results);
  EXPECT_EQ(4u, ui_results.item_count());

  // The reordered results should use the original objects.
  EXPECT_EQ(old_ui_result_ids[0],
            TestSearchResult::GetInstanceId(ui_results.GetItemAt(3)));
  EXPECT_EQ(old_ui_result_ids[1],
            TestSearchResult::GetInstanceId(ui_results.GetItemAt(0)));
  EXPECT_EQ(old_ui_result_ids[2],
            TestSearchResult::GetInstanceId(ui_results.GetItemAt(2)));
}

INSTANTIATE_TEST_CASE_P(MixerTestInstance, MixerTest, testing::Bool());

}  // namespace test
}  // namespace app_list
