// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_PERF_HEAP_COLLECTOR_H_
#define CHROME_BROWSER_METRICS_PERF_HEAP_COLLECTOR_H_

#include <map>
#include <memory>
#include <string>

#include "base/feature_list.h"
#include "base/optional.h"
#include "chrome/browser/metrics/perf/metric_collector.h"
#include "chrome/browser/metrics/perf/perf_output.h"
#include "chrome/browser/ui/browser_list_observer.h"

class Browser;

namespace base {
class CommandLine;
class FilePath;
}  // namespace base

namespace metrics {

// Feature for controlling the heap collection parameters.
extern const base::Feature kCWPHeapCollection;

// Enables collection of heap profiles using the tcmalloc heap sampling
// profiler.
class HeapCollector : public MetricCollector, public BrowserListObserver {
 public:
  HeapCollector();
  ~HeapCollector() override;

  // MetricCollector:
  void Init() override;

 protected:
  // MetricCollector:
  bool ShouldCollect() const override;
  void CollectProfile(std::unique_ptr<SampledProfile> sampled_profile) override;

  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override;
  void OnBrowserRemoved(Browser* browser) override;

  // Fetches a heap profile from tcmalloc, dumps it to a temp file, and returns
  // the path.
  base::Optional<base::FilePath> DumpProfileToTempFile();

  // Generates a quipper command to parse the given profile file.
  base::CommandLine MakeQuipperCommand(const base::FilePath& profile_path);

  // Executes the given command line to parse a profile stored at the given
  // path and saves it in the given sampled profile. The given temporary profile
  // file is removed after parsing.
  void ParseAndSaveProfile(const base::CommandLine& parser,
                           const base::FilePath& profile_path,
                           std::unique_ptr<SampledProfile> sampled_profile);

 private:
  // Change the values in |collection_params_| based on the values of field
  // trial parameters.
  void SetCollectionParamsFromFeatureParams();

  // Heap sampling period.
  size_t sampling_period_bytes_;

  DISALLOW_COPY_AND_ASSIGN(HeapCollector);
};

}  // namespace metrics

#endif  // CHROME_BROWSER_METRICS_PERF_HEAP_COLLECTOR_H_
