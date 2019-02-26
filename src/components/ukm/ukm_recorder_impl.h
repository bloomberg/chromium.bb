// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UKM_UKM_RECORDER_IMPL_H_
#define COMPONENTS_UKM_UKM_RECORDER_IMPL_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

#include "base/callback_forward.h"
#include "base/containers/flat_map.h"
#include "base/sequence_checker.h"
#include "base/strings/string_piece.h"
#include "services/metrics/public/cpp/ukm_decode.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/mojom/ukm_interface.mojom.h"

namespace metrics {
class UkmBrowserTestBase;
class UkmEGTestHelper;
}

namespace ukm {
class Report;
class UkmRecorderImplTest;
class UkmSource;
class UkmUtilsForTest;

namespace debug {
class UkmDebugDataExtractor;
}

class UkmRecorderImpl : public UkmRecorder {
  using IsWebstoreExtensionCallback =
      base::RepeatingCallback<bool(base::StringPiece id)>;

 public:
  UkmRecorderImpl();
  ~UkmRecorderImpl() override;

  // Unconditionally attempts to create a field trial to control client side
  // metrics/crash sampling to use as a fallback when one hasn't been
  // provided. This is expected to occur on first-run on platforms that don't
  // have first-run variations support. This should only be called when there is
  // no existing field trial controlling the sampling feature.
  static void CreateFallbackSamplingTrial(bool is_stable_channel,
                                          base::FeatureList* feature_list);

  // Enables/disables recording control if data is allowed to be collected. The
  // |extensions| flag separately controls recording of chrome-extension://
  // URLs; this flag should reflect the "sync extensions" user setting.
  void EnableRecording(bool extensions);
  void DisableRecording();

  // Disables sampling for testing purposes.
  void DisableSamplingForTesting() override;

  // Deletes stored recordings.
  void Purge();

  // Sets a callback for determining if an extension URL can be recorded.
  void SetIsWebstoreExtensionCallback(
      const IsWebstoreExtensionCallback& callback);

 protected:
  // Calculates sampled in/out based on a given |rate|. This is virtual so
  // it can be overriden by tests.
  virtual bool IsSampledIn(int sampling_rate);

  // Cache the list of whitelisted entries from the field trial parameter.
  void StoreWhitelistedEntries();

  // Writes recordings into a report proto, and clears recordings.
  void StoreRecordingsInReport(Report* report);

  const std::map<SourceId, std::unique_ptr<UkmSource>>& sources() const {
    return recordings_.sources;
  }

  const std::vector<mojom::UkmEntryPtr>& entries() const {
    return recordings_.entries;
  }

  // UkmRecorder:
  void AddEntry(mojom::UkmEntryPtr entry) override;
  void UpdateSourceURL(SourceId source_id, const GURL& url) override;
  void UpdateAppURL(SourceId source_id, const GURL& url) override;
  void RecordNavigation(
      SourceId source_id,
      const UkmSource::NavigationData& navigation_data) override;
  using UkmRecorder::RecordOtherURL;

  virtual bool ShouldRestrictToWhitelistedSourceIds() const;

  virtual bool ShouldRestrictToWhitelistedEntries() const;

 private:
  friend ::metrics::UkmBrowserTestBase;
  friend ::metrics::UkmEGTestHelper;
  friend ::ukm::debug::UkmDebugDataExtractor;
  friend ::ukm::UkmRecorderImplTest;
  friend ::ukm::UkmUtilsForTest;

  struct MetricAggregate {
    uint64_t total_count = 0;
    double value_sum = 0;
    double value_square_sum = 0.0;
    uint64_t dropped_due_to_limits = 0;
    uint64_t dropped_due_to_sampling = 0;
    uint64_t dropped_due_to_whitelist = 0;
  };

  struct EventAggregate {
    EventAggregate();
    ~EventAggregate();

    base::flat_map<uint64_t, MetricAggregate> metrics;
    uint64_t total_count = 0;
    uint64_t dropped_due_to_limits = 0;
    uint64_t dropped_due_to_sampling = 0;
    uint64_t dropped_due_to_whitelist = 0;
  };

  // Container for sampling in/out choices for events within a single page
  // load. This is important because some events are emitted multiple times
  // with different metric values that are expected to be grouped together.
  // For example, Blink.UseCounter is emitted for *all* used blink features
  // on a page so its important that this metric either be on or off for
  // the entire page. The sampling of different events is calculated
  // independently (i.e. it can't be assumed that because one type of event
  // is sampled-in that another will be sample-in or sampled-out) but always
  // remembered for the entire page.
  class PageSampling {
   public:
    PageSampling();
    ~PageSampling();

    // Sets the sampled-in flag for a given |event_id|.
    void Set(uint64_t event_id, bool sampled_in);

    // Returns if there is already a flag for a given |event_id|. The value
    // of that flag is stored in |out_sampled_in|;
    bool Find(uint64_t event_id, bool* out_sampled_in) const;

    // Returns if this record has been modified.
    bool modified() const { return modified_; }

    // Clears the |modified_| flag.
    void clear_modified() { modified_ = false; }

   private:
    // Per-event boolean indicating sampled-in for this page, keyed by event_id.
    std::map<uint64_t, bool> event_sampling_;

    // Boolean indicating if this has been modified, used to clear out old
    // entries so they don't continue to use memory. "Modified" means Set()
    // has been called since the last time clear_modified() was called
    // (currently at every upload of UKM data).
    bool modified_ = false;

    DISALLOW_COPY_AND_ASSIGN(PageSampling);
  };

  using MetricAggregateMap = std::map<uint64_t, MetricAggregate>;

  // Returns true if |sanitized_url| should be recorded.
  bool ShouldRecordUrl(SourceId source_id, const GURL& sanitized_url) const;

  void RecordSource(std::unique_ptr<UkmSource> source);

  // Load sampling configurations from field-trial information.
  void LoadExperimentSamplingInfo();

  // Whether recording new data is currently allowed.
  bool recording_enabled_ = false;

  // Indicates whether recording is enabled for extensions.
  bool extensions_enabled_ = false;

  // Indicates whether recording continuity has been broken since last report.
  bool recording_is_continuous_ = true;

  // Indicates if sampling has been enabled.
  bool sampling_enabled_ = true;

  // Callback for checking extension IDs.
  IsWebstoreExtensionCallback is_webstore_extension_callback_;

  // Map from hashes to entry and metric names.
  ukm::builders::DecodeMap decode_map_;

  // Whitelisted Entry hashes, only the ones in this set will be recorded.
  std::set<uint64_t> whitelisted_entry_hashes_;

  // Sampling configurations, loaded from a field-trial.
  int default_sampling_rate_ = 0;
  base::flat_map<uint64_t, int> event_sampling_rates_;

  // Result of sampling calculation per event for a source/page. This is
  // cleared at the start of each page load and ensure that that all events
  // within a page will be included or excluded together.
  std::map<int64_t, PageSampling> source_event_sampling_;

  // Contains data from various recordings which periodically get serialized
  // and cleared by StoreRecordingsInReport() and may be Purged().
  struct Recordings {
    Recordings();
    Recordings& operator=(Recordings&&);
    ~Recordings();

    // Data captured by UpdateSourceUrl().
    std::map<SourceId, std::unique_ptr<UkmSource>> sources;

    // Data captured by AddEntry().
    std::vector<mojom::UkmEntryPtr> entries;

    // URLs of sources that matched a whitelist url, but were not included in
    // the report generated by the last log rotation because we haven't seen any
    // events for that source yet.
    std::unordered_set<std::string> carryover_urls_whitelist;

    // Aggregate information for collected event metrics.
    std::map<uint64_t, EventAggregate> event_aggregations;

    // Aggregated counters about Sources recorded in the current log.
    struct SourceCounts {
      // Count of URLs recorded for all sources.
      size_t observed = 0;
      // Count of URLs recorded for all SourceIdType::NAVIGATION_ID Sources.
      size_t navigation_sources = 0;
      // Sources carried over (not recorded) from a previous logging rotation.
      size_t carryover_sources = 0;

      // Resets all of the data.
      void Reset();
    };
    SourceCounts source_counts;

    // Resets all of the data.
    void Reset();
  };
  Recordings recordings_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace ukm

#endif  // COMPONENTS_UKM_UKM_RECORDER_IMPL_H_
