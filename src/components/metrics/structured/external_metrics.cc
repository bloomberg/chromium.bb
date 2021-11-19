// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/external_metrics.h"

#include <sys/file.h>

#include "base/containers/fixed_flat_set.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/task/post_task.h"
#include "base/task/task_runner_util.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/metrics/structured/storage.pb.h"
#include "components/metrics/structured/structured_metrics_features.h"

namespace metrics {
namespace structured {
namespace {

// TODO(b/181724341): Remove this once the bluetooth metrics are fully enabled.
void MaybeFilterBluetoothEvents(
    google::protobuf::RepeatedPtrField<metrics::StructuredEventProto>* events) {
  // Event name hashes of all bluetooth events listed in
  // src/platform2/metrics/structured/structured.xml.
  static constexpr auto kBluetoothEventHashes = base::MakeFixedFlatSet<uint64_t>({
      // BluetoothAdapterStateChanged
      UINT64_C(959829856916771459),
      // BluetoothPairingStateChanged
      UINT64_C(11839023048095184048),
      // BluetoothAclConnectionStateChanged
      UINT64_C(1880220404408566268),
      // BluetoothProfileConnectionStateChanged
      UINT64_C(7217682640379679663),
      // BluetoothDeviceInfoReport
      UINT64_C(1506471670382892394)
  });

  if (base::FeatureList::IsEnabled(kBluetoothSessionizedMetrics))
    return;

  // Remove all bluetooth events.
  auto it = events->begin();
  while (it != events->end()) {
    if (kBluetoothEventHashes.contains(it->event_name_hash())) {
      it = events->erase(it);
    } else {
      ++it;
    }
  }
}

EventsProto ReadAndDeleteEvents(const base::FilePath& directory) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  EventsProto result;
  if (!base::DirectoryExists(directory))
    return result;

  base::FileEnumerator enumerator(directory, false,
                                  base::FileEnumerator::FILES);
  for (base::FilePath path = enumerator.Next(); !path.empty();
       path = enumerator.Next()) {
    std::string proto_str;
    EventsProto proto;

    bool read_ok = base::ReadFileToString(path, &proto_str) &&
                   proto.ParseFromString(proto_str);
    base::DeleteFile(path);

    if (!read_ok)
      continue;

    // MergeFrom performs a copy that could be a move if done manually. But all
    // the protos here are expected to be small, so let's keep it simple.
    result.mutable_uma_events()->MergeFrom(proto.uma_events());
    result.mutable_non_uma_events()->MergeFrom(proto.non_uma_events());
  }

  MaybeFilterBluetoothEvents(result.mutable_uma_events());
  MaybeFilterBluetoothEvents(result.mutable_non_uma_events());
  return result;
}

}  // namespace

ExternalMetrics::ExternalMetrics(const base::FilePath& events_directory,
                                 const base::TimeDelta& collection_interval,
                                 MetricsCollectedCallback callback)
    : events_directory_(events_directory),
      collection_interval_(collection_interval),
      callback_(std::move(callback)),
      task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::BEST_EFFORT, base::MayBlock(),
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})) {
  ScheduleCollector();
}

ExternalMetrics::~ExternalMetrics() = default;

void ExternalMetrics::CollectEventsAndReschedule() {
  CollectEvents();
  ScheduleCollector();
}

void ExternalMetrics::ScheduleCollector() {
  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ExternalMetrics::CollectEventsAndReschedule,
                     weak_factory_.GetWeakPtr()),
      collection_interval_);
}

void ExternalMetrics::CollectEvents() {
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&ReadAndDeleteEvents, events_directory_),
      base::BindOnce(callback_));
}

}  // namespace structured
}  // namespace metrics
