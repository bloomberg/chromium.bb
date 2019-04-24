// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/perf/heap_collector.h"

#include <memory>
#include <string>
#include <utility>

#include "base/allocator/allocator_extension.h"
#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/metrics/field_trial_params.h"
#include "base/process/process_handle.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "third_party/metrics_proto/sampled_profile.pb.h"

namespace metrics {

// Feature for controlling the heap collection parameters.
const base::Feature kCWPHeapCollection{"CWPHeapCollection",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

namespace {

// Name of the heap collector. It is appended to the UMA metric names for
// reporting collection and upload status.
const char kHeapCollectorName[] = "Heap";

// The approximate gap in bytes between sampling actions. Heap allocations are
// sampled using a geometric distribution with the specified mean.
const size_t kHeapSamplingIntervalBytes = 1 * 1024 * 1024;

// Feature parameters that control the behavior of the heap collector.
constexpr base::FeatureParam<int> kSamplingIntervalBytes{
    &kCWPHeapCollection, "SamplingIntervalBytes", kHeapSamplingIntervalBytes};

constexpr base::FeatureParam<int> kPeriodicCollectionIntervalMs{
    &kCWPHeapCollection, "PeriodicCollectionIntervalMs",
    3 * 3600 * 1000};  // 3h

constexpr base::FeatureParam<int> kResumeFromSuspendSamplingFactor{
    &kCWPHeapCollection, "ResumeFromSuspend::SamplingFactor", 10};

constexpr base::FeatureParam<int> kResumeFromSuspendMaxDelaySec{
    &kCWPHeapCollection, "ResumeFromSuspend::MaxDelaySec", 5};

constexpr base::FeatureParam<int> kRestoreSessionSamplingFactor{
    &kCWPHeapCollection, "RestoreSession::SamplingFactor", 10};

constexpr base::FeatureParam<int> kRestoreSessionMaxDelaySec{
    &kCWPHeapCollection, "RestoreSession::MaxDelaySec", 10};

// Limit the total size of protobufs that can be cached, so they don't take up
// too much memory. If the size of cached protobufs exceeds this value, stop
// collecting further perf data. The current value is 2 MB.
const size_t kCachedHeapDataProtobufSizeThreshold = 2 * 1024 * 1024;

// Location of quipper on ChromeOS.
const char kQuipperLocation[] = "/usr/bin/quipper";

// Quipper arguments for passing in a profile and the process PID.
const char kQuipperHeapProfile[] = "input_heap_profile";
const char kQuipperProcessPid[] = "pid";

// Deletes the temp file when the object goes out of scope.
class FileDeleter {
 public:
  explicit FileDeleter(const base::FilePath& temp_dir) : temp_dir_(temp_dir) {}
  ~FileDeleter();

 private:
  const base::FilePath temp_dir_;

  DISALLOW_COPY_AND_ASSIGN(FileDeleter);
};

FileDeleter::~FileDeleter() {
  base::PostTaskWithTraits(FROM_HERE,
                           {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
                            base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
                           base::BindOnce(base::IgnoreResult(&base::DeleteFile),
                                          std::move(temp_dir_), false));
}

void SetHeapSamplingPeriod(size_t sampling_period) {
  bool res = base::allocator::SetNumericProperty(
      "tcmalloc.sampling_period_bytes", sampling_period);
  DCHECK(res);
}

}  // namespace

HeapCollector::HeapCollector()
    : MetricCollector(kHeapCollectorName),
      sampling_period_bytes_(kHeapSamplingIntervalBytes) {
  BrowserList::AddObserver(this);
}

HeapCollector::~HeapCollector() {
  // Disable heap sampling when the collector exits.
  SetHeapSamplingPeriod(0);
  BrowserList::RemoveObserver(this);
}

void HeapCollector::Init() {
  if (base::FeatureList::IsEnabled(kCWPHeapCollection))
    SetCollectionParamsFromFeatureParams();

  size_t sampling_period = 0;
  // Enable sampling if no incognito session is active.
  if (!BrowserList::IsIncognitoSessionActive()) {
    sampling_period = sampling_period_bytes_;
  }
  SetHeapSamplingPeriod(sampling_period);

  MetricCollector::Init();
}

void HeapCollector::OnBrowserAdded(Browser* browser) {
  // Pause heap sampling when an incognito session is opened.
  if (browser->profile()->IsOffTheRecord()) {
    SetHeapSamplingPeriod(0);
  }
}

void HeapCollector::OnBrowserRemoved(Browser* browser) {
  // Resume heap sampling if no incognito sessions are active.
  if (!BrowserList::IsIncognitoSessionActive()) {
    SetHeapSamplingPeriod(sampling_period_bytes_);
  }
}

void HeapCollector::SetCollectionParamsFromFeatureParams() {
  sampling_period_bytes_ = kSamplingIntervalBytes.Get();
  collection_params_.periodic_interval =
      base::TimeDelta::FromMilliseconds(kPeriodicCollectionIntervalMs.Get());
  collection_params_.resume_from_suspend.sampling_factor =
      kResumeFromSuspendSamplingFactor.Get();
  collection_params_.resume_from_suspend.max_collection_delay =
      base::TimeDelta::FromSeconds(kResumeFromSuspendMaxDelaySec.Get());
  collection_params_.restore_session.sampling_factor =
      kRestoreSessionSamplingFactor.Get();
  collection_params_.restore_session.max_collection_delay =
      base::TimeDelta::FromSeconds(kRestoreSessionMaxDelaySec.Get());
}

bool HeapCollector::ShouldCollect() const {
  // Do not collect further data if we've already collected a substantial amount
  // of data, as indicated by |kCachedHeapDataProtobufSizeThreshold|.
  if (cached_profile_data_size() >= kCachedHeapDataProtobufSizeThreshold) {
    AddToUmaHistogram(CollectionAttemptStatus::NOT_READY_TO_COLLECT);
    return false;
  }
  return true;
}

void HeapCollector::CollectProfile(
    std::unique_ptr<SampledProfile> sampled_profile) {
  base::Optional<base::FilePath> temp_file = DumpProfileToTempFile();
  if (!temp_file)
    return;

  base::CommandLine quipper = MakeQuipperCommand(temp_file.value());
  ParseAndSaveProfile(quipper, temp_file.value(), std::move(sampled_profile));
}

base::Optional<base::FilePath> HeapCollector::DumpProfileToTempFile() {
  base::FilePath temp_path;
  if (!base::CreateTemporaryFile(&temp_path)) {
    AddToUmaHistogram(CollectionAttemptStatus::UNABLE_TO_COLLECT);
    return base::nullopt;
  }
  std::string writer;
  base::allocator::GetHeapSample(&writer);
  base::File temp(temp_path, base::File::FLAG_CREATE_ALWAYS |
                                 base::File::FLAG_READ |
                                 base::File::FLAG_WRITE);
  DCHECK(temp.created());
  DCHECK(temp.IsValid());
  int res = temp.WriteAtCurrentPos(writer.c_str(), writer.length());
  DCHECK_EQ(res, static_cast<int>(writer.length()));
  temp.Close();
  return base::make_optional<base::FilePath>(temp_path);
}

base::CommandLine HeapCollector::MakeQuipperCommand(
    const base::FilePath& profile_path) {
  base::CommandLine quipper{base::FilePath(kQuipperLocation)};
  quipper.AppendSwitchPath(kQuipperHeapProfile, profile_path);
  quipper.AppendSwitchASCII(kQuipperProcessPid,
                            std::to_string(base::GetCurrentProcId()));
  return quipper;
}

void HeapCollector::ParseAndSaveProfile(
    const base::CommandLine& parser,
    const base::FilePath& profile_path,
    std::unique_ptr<SampledProfile> sampled_profile) {
  // We may exit due to parsing errors, so use a FileDeleter to remove the
  // temporary profile data on all paths.
  FileDeleter file_deleter(profile_path);

  // Run the parser command on the profile file.
  std::string output;
  if (!base::GetAppOutput(parser, &output)) {
    AddToUmaHistogram(CollectionAttemptStatus::ILLEGAL_DATA_RETURNED);
    return;
  }

  SaveSerializedPerfProto(std::move(sampled_profile),
                          PerfProtoType::PERF_TYPE_DATA, output);
}

}  // namespace metrics
