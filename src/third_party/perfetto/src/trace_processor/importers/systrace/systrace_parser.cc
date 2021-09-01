/*
 * Copyright (C) 2018 The Android Open Source Project
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

#include "src/trace_processor/importers/systrace/systrace_parser.h"

#include "perfetto/ext/base/optional.h"
#include "src/trace_processor/importers/common/event_tracker.h"
#include "src/trace_processor/importers/common/process_tracker.h"
#include "src/trace_processor/importers/common/slice_tracker.h"
#include "src/trace_processor/importers/common/track_tracker.h"
#include "src/trace_processor/importers/proto/async_track_set_tracker.h"

namespace perfetto {
namespace trace_processor {

SystraceParser::SystraceParser(TraceProcessorContext* ctx)
    : context_(ctx),
      lmk_id_(ctx->storage->InternString("mem.lmk")),
      screen_state_id_(ctx->storage->InternString("ScreenState")),
      cookie_id_(ctx->storage->InternString("cookie")) {}

SystraceParser::~SystraceParser() = default;

void SystraceParser::ParsePrintEvent(int64_t ts,
                                     uint32_t pid,
                                     base::StringView event) {
  systrace_utils::SystraceTracePoint point{};
  switch (ParseSystraceTracePoint(event, &point)) {
    case systrace_utils::SystraceParseResult::kSuccess:
      ParseSystracePoint(ts, pid, point);
      break;
    case systrace_utils::SystraceParseResult::kFailure:
      context_->storage->IncrementStats(stats::systrace_parse_failure);
      break;
    case systrace_utils::SystraceParseResult::kUnsupported:
      // Silently ignore unsupported results.
      break;
  }
}

void SystraceParser::ParseZeroEvent(int64_t ts,
                                    uint32_t pid,
                                    int32_t flag,
                                    base::StringView name,
                                    uint32_t /* tgid */,
                                    int64_t value) {
  systrace_utils::SystraceTracePoint point{};
  point.name = name;
  point.value = static_cast<double>(value);

  // Hardcode the tgid to 0 (i.e. no tgid available) because zero events can
  // come from kernel threads and as we group kernel threads into the kthreadd
  // process, we would want |point.tgid == kKthreaddPid|. However, we don't have
  // acces to the ppid of this process so we have to not associate to any
  // process and leave the resolution of process to other events.
  // TODO(lalitm): remove this hack once we move kernel thread grouping to
  // the UI.
  point.tgid = 0;

  // The value of these constants can be found in the msm-google kernel.
  constexpr int32_t kSystraceEventBegin = 1 << 0;
  constexpr int32_t kSystraceEventEnd = 1 << 1;
  constexpr int32_t kSystraceEventInt64 = 1 << 2;

  if ((flag & kSystraceEventBegin) != 0) {
    point.phase = 'B';
  } else if ((flag & kSystraceEventEnd) != 0) {
    point.phase = 'E';
  } else if ((flag & kSystraceEventInt64) != 0) {
    point.phase = 'C';
  } else {
    context_->storage->IncrementStats(stats::systrace_parse_failure);
    return;
  }
  ParseSystracePoint(ts, pid, point);
}

void SystraceParser::ParseTracingMarkWrite(int64_t ts,
                                           uint32_t pid,
                                           char trace_type,
                                           bool trace_begin,
                                           base::StringView trace_name,
                                           uint32_t /* tgid */,
                                           int64_t value) {
  systrace_utils::SystraceTracePoint point{};
  point.name = trace_name;

  // Hardcode the tgid to 0 (i.e. no tgid available) because
  // sde_tracing_mark_write events can come from kernel threads and because we
  // group kernel threads into the kthreadd process, we would want |point.tgid
  // == kKthreaddPid|. However, we don't have acces to the ppid of this process
  // so we have to not associate to any process and leave the resolution of
  // process to other events.
  // TODO(lalitm): remove this hack once we move kernel thread grouping to
  // the UI.
  point.tgid = 0;

  point.value = static_cast<double>(value);
  // Some versions of this trace point fill trace_type with one of (B/E/C),
  // others use the trace_begin boolean and only support begin/end events:
  if (trace_type == 0) {
    point.phase = trace_begin ? 'B' : 'E';
  } else if (trace_type == 'B' || trace_type == 'E' || trace_type == 'C') {
    point.phase = trace_type;
  } else {
    context_->storage->IncrementStats(stats::systrace_parse_failure);
    return;
  }

  ParseSystracePoint(ts, pid, point);
}

void SystraceParser::ParseSystracePoint(
    int64_t ts,
    uint32_t pid,
    systrace_utils::SystraceTracePoint point) {
  switch (point.phase) {
    case 'B': {
      StringId name_id = context_->storage->InternString(point.name);
      UniqueTid utid;
      if (point.tgid == 0) {
        utid = context_->process_tracker->GetOrCreateThread(pid);
      } else {
        utid = context_->process_tracker->UpdateThread(pid, point.tgid);
      }
      TrackId track_id = context_->track_tracker->InternThreadTrack(utid);
      context_->slice_tracker->Begin(ts, track_id, kNullStringId /* cat */,
                                     name_id);
      break;
    }

    case 'E': {
      // |point.tgid| can be 0 in older android versions where the end event
      // would not contain the value.
      UniqueTid utid;
      if (point.tgid == 0) {
        // If we haven't seen this thread before, there can't have been a Begin
        // event for it so just ignore the event.
        auto opt_utid = context_->process_tracker->GetThreadOrNull(pid);
        if (!opt_utid)
          break;
        utid = *opt_utid;
      } else {
        utid = context_->process_tracker->UpdateThread(pid, point.tgid);
      }
      TrackId track_id = context_->track_tracker->InternThreadTrack(utid);
      context_->slice_tracker->End(ts, track_id);
      break;
    }

    case 'S':
    case 'F': {
      StringId name_id = context_->storage->InternString(point.name);
      int64_t cookie = static_cast<int64_t>(point.value);
      UniquePid upid =
          context_->process_tracker->GetOrCreateProcess(point.tgid);

      auto track_set_id =
          context_->async_track_set_tracker->InternAndroidSet(upid, name_id);

      if (point.phase == 'S') {
        // Historically, async slices on Android did not support nesting async
        // slices (i.e. you could not have a stack of async slices). If clients
        // were implemented correctly, we would simply be able to use the normal
        // Begin method and we could rely on the traced code to never emit two
        // 'S' events back to back on the same track.
        // However, there exists buggy code in Android (in Wakelock class of
        // PowerManager) which emits an arbitrary number of 'S' events and
        // expects only the first one to be tracked. Moreover, this issue is
        // compounded by an unfortunate implementation of async slices in
        // Catapult (the legacy trace viewer) which simply tracks the details of
        // the *most recent* emitted 'S' event which leads even more inaccurate
        // behaviour. To support these quirks, we have the special 'unnestable'
        // slice concept which implements workarounds for these very specific
        // issues. No other code should ever use this method.
        tables::SliceTable::Row row;
        row.ts = ts;
        row.track_id =
            context_->async_track_set_tracker->Begin(track_set_id, cookie);
        row.name = name_id;
        context_->slice_tracker->BeginLegacyUnnestable(
            row, [this, cookie](ArgsTracker::BoundInserter* inserter) {
              inserter->AddArg(cookie_id_, Variadic::Integer(cookie));
            });
      } else {
        TrackId track_id =
            context_->async_track_set_tracker->End(track_set_id, cookie);
        context_->slice_tracker->End(ts, track_id);
      }
      break;
    }

    case 'C': {
      // LMK events from userspace are hacked as counter events with the "value"
      // of the counter representing the pid of the killed process which is
      // reset to 0 once the kill is complete.
      // Homogenise this with kernel LMK events as an instant event, ignoring
      // the resets to 0.
      if (point.name == "kill_one_process") {
        auto killed_pid = static_cast<uint32_t>(point.value);
        if (killed_pid != 0) {
          UniquePid killed_upid =
              context_->process_tracker->GetOrCreateProcess(killed_pid);
          context_->event_tracker->PushInstant(ts, lmk_id_, killed_upid,
                                               RefType::kRefUpid);
        }
        // TODO(lalitm): we should not add LMK events to the counters table
        // once the UI has support for displaying instants.
      } else if (point.name == "ScreenState") {
        // Promote ScreenState to its own top level counter.
        TrackId track =
            context_->track_tracker->InternGlobalCounterTrack(screen_state_id_);
        context_->event_tracker->PushCounter(ts, point.value, track);
        return;
      }

      StringId name_id = context_->storage->InternString(point.name);
      TrackId track_id;
      if (point.tgid == 0) {
        // If tgid is 0 (likely because this is a kernel thread), we can do no
        // better than using a thread track with the pid of the process.
        UniqueTid utid = context_->process_tracker->GetOrCreateThread(pid);
        track_id =
            context_->track_tracker->InternThreadCounterTrack(name_id, utid);
      } else {
        // This is per upid on purpose. Some counters are pushed from arbitrary
        // threads but are really per process.
        UniquePid upid =
            context_->process_tracker->GetOrCreateProcess(point.tgid);
        track_id =
            context_->track_tracker->InternProcessCounterTrack(name_id, upid);
      }
      context_->event_tracker->PushCounter(ts, point.value, track_id);
    }
  }
}

}  // namespace trace_processor
}  // namespace perfetto
