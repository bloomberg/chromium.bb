/*
 * Copyright (C) 2022 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SRC_TRACE_PROCESSOR_IMPORTERS_COMMON_ARGS_TRANSLATION_TABLE_H_
#define SRC_TRACE_PROCESSOR_IMPORTERS_COMMON_ARGS_TRANSLATION_TABLE_H_

#include <cstdint>

#include "perfetto/ext/base/flat_hash_map.h"
#include "perfetto/ext/base/optional.h"
#include "perfetto/ext/base/string_view.h"
#include "src/trace_processor/importers/common/args_tracker.h"
#include "src/trace_processor/storage/trace_storage.h"
#include "src/trace_processor/util/proto_to_args_parser.h"

namespace perfetto {
namespace trace_processor {

// Tracks and stores args translation rules. It allows Trace Processor
// to map for example hashes to their names.
class ArgsTranslationTable {
 public:
  using Key = util::ProtoToArgsParser::Key;

  ArgsTranslationTable(TraceStorage* storage);

  // Returns true if the translation table fully handles the arg, in which case
  // the original arg doesn't need to be processed. This function has not added
  // anything if returning false.
  bool TranslateUnsignedIntegerArg(const Key& key,
                                   uint64_t value,
                                   ArgsTracker::BoundInserter& inserter);

  void AddChromeHistogramTranslationRule(uint64_t hash, base::StringView name) {
    chrome_histogram_hash_to_name_.Insert(hash, name.ToStdString());
  }
  void AddChromeUserEventTranslationRule(uint64_t hash,
                                         base::StringView action) {
    chrome_user_event_hash_to_action_.Insert(hash, action.ToStdString());
  }
  void AddChromePerformanceMarkSiteTranslationRule(uint64_t hash,
                                                   base::StringView name) {
    chrome_performance_mark_site_hash_to_name_.Insert(hash, name.ToStdString());
  }
  void AddChromePerformanceMarkMarkTranslationRule(uint64_t hash,
                                                   base::StringView name) {
    chrome_performance_mark_mark_hash_to_name_.Insert(hash, name.ToStdString());
  }

  base::Optional<base::StringView> TranslateChromeHistogramHashForTesting(
      uint64_t hash) const {
    return TranslateChromeHistogramHash(hash);
  }
  base::Optional<base::StringView> TranslateChromeUserEventHashForTesting(
      uint64_t hash) const {
    return TranslateChromeUserEventHash(hash);
  }
  base::Optional<base::StringView>
  TranslateChromePerformanceMarkSiteHashForTesting(uint64_t hash) const {
    return TranslateChromePerformanceMarkSiteHash(hash);
  }
  base::Optional<base::StringView>
  TranslateChromePerformanceMarkMarkHashForTesting(uint64_t hash) const {
    return TranslateChromePerformanceMarkMarkHash(hash);
  }

 private:
  static constexpr char kChromeHistogramHashKey[] =
      "chrome_histogram_sample.name_hash";
  static constexpr char kChromeHistogramNameKey[] =
      "chrome_histogram_sample.name";

  static constexpr char kChromeUserEventHashKey[] =
      "chrome_user_event.action_hash";
  static constexpr char kChromeUserEventActionKey[] =
      "chrome_user_event.action";

  static constexpr char kChromePerformanceMarkSiteHashKey[] =
      "chrome_hashed_performance_mark.site_hash";
  static constexpr char kChromePerformanceMarkSiteKey[] =
      "chrome_hashed_performance_mark.site";

  static constexpr char kChromePerformanceMarkMarkHashKey[] =
      "chrome_hashed_performance_mark.mark_hash";
  static constexpr char kChromePerformanceMarkMarkKey[] =
      "chrome_hashed_performance_mark.mark";

  TraceStorage* storage_;
  StringId interned_chrome_histogram_hash_key_;
  StringId interned_chrome_histogram_name_key_;
  StringId interned_chrome_user_event_hash_key_;
  StringId interned_chrome_user_event_action_key_;
  StringId interned_chrome_performance_mark_site_hash_key_;
  StringId interned_chrome_performance_mark_site_key_;
  StringId interned_chrome_performance_mark_mark_hash_key_;
  StringId interned_chrome_performance_mark_mark_key_;
  base::FlatHashMap<uint64_t, std::string> chrome_histogram_hash_to_name_;
  base::FlatHashMap<uint64_t, std::string> chrome_user_event_hash_to_action_;
  base::FlatHashMap<uint64_t, std::string>
      chrome_performance_mark_site_hash_to_name_;
  base::FlatHashMap<uint64_t, std::string>
      chrome_performance_mark_mark_hash_to_name_;

  base::Optional<base::StringView> TranslateChromeHistogramHash(
      uint64_t hash) const;
  base::Optional<base::StringView> TranslateChromeUserEventHash(
      uint64_t hash) const;
  base::Optional<base::StringView> TranslateChromePerformanceMarkSiteHash(
      uint64_t hash) const;
  base::Optional<base::StringView> TranslateChromePerformanceMarkMarkHash(
      uint64_t hash) const;
};

}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_IMPORTERS_COMMON_ARGS_TRANSLATION_TABLE_H_
