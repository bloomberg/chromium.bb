/*
 * Copyright (C) 2017 The Android Open Source Project
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

#include "src/traced/probes/ftrace/ftrace_controller.h"

#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <array>
#include <string>
#include <utility>

#include "perfetto/base/build_config.h"
#include "perfetto/base/logging.h"
#include "perfetto/base/time.h"
#include "perfetto/ext/base/file_utils.h"
#include "perfetto/ext/base/metatrace.h"
#include "perfetto/ext/base/string_utils.h"
#include "perfetto/ext/tracing/core/trace_writer.h"
#include "src/kallsyms/kernel_symbol_map.h"
#include "src/kallsyms/lazy_kernel_symbolizer.h"
#include "src/traced/probes/ftrace/atrace_hal_wrapper.h"
#include "src/traced/probes/ftrace/cpu_reader.h"
#include "src/traced/probes/ftrace/cpu_stats_parser.h"
#include "src/traced/probes/ftrace/discover_vendor_tracepoints.h"
#include "src/traced/probes/ftrace/event_info.h"
#include "src/traced/probes/ftrace/ftrace_config_muxer.h"
#include "src/traced/probes/ftrace/ftrace_data_source.h"
#include "src/traced/probes/ftrace/ftrace_metadata.h"
#include "src/traced/probes/ftrace/ftrace_procfs.h"
#include "src/traced/probes/ftrace/ftrace_stats.h"
#include "src/traced/probes/ftrace/proto_translation_table.h"

namespace perfetto {
namespace {

constexpr int kDefaultDrainPeriodMs = 100;
constexpr int kMinDrainPeriodMs = 1;
constexpr int kMaxDrainPeriodMs = 1000 * 60;

// Read at most this many pages of data per cpu per read task. If we hit this
// limit on at least one cpu, we stop and repost the read task, letting other
// tasks get some cpu time before continuing reading.
constexpr size_t kMaxPagesPerCpuPerReadTick = 256;  // 1 MB per cpu

// When reading and parsing data for a particular cpu, we do it in batches of
// this many pages. In other words, we'll read up to
// |kParsingBufferSizePages| into memory, parse them, and then repeat if we
// still haven't caught up to the writer. A working set of 32 pages is 128k of
// data, which should fit in a typical L2D cache. Furthermore, the batching
// limits the memory usage of traced_probes.
//
// TODO(rsavitski): consider making buffering & parsing page counts independent,
// should be a single counter in the cpu_reader, similar to lost_events case.
constexpr size_t kParsingBufferSizePages = 32;

uint32_t ClampDrainPeriodMs(uint32_t drain_period_ms) {
  if (drain_period_ms == 0) {
    return kDefaultDrainPeriodMs;
  }
  if (drain_period_ms < kMinDrainPeriodMs ||
      kMaxDrainPeriodMs < drain_period_ms) {
    PERFETTO_LOG("drain_period_ms was %u should be between %u and %u",
                 drain_period_ms, kMinDrainPeriodMs, kMaxDrainPeriodMs);
    return kDefaultDrainPeriodMs;
  }
  return drain_period_ms;
}

bool WriteToFile(const char* path, const char* str) {
  auto fd = base::OpenFile(path, O_WRONLY);
  if (!fd)
    return false;
  const size_t str_len = strlen(str);
  return base::WriteAll(*fd, str, str_len) == static_cast<ssize_t>(str_len);
}

bool ClearFile(const char* path) {
  auto fd = base::OpenFile(path, O_WRONLY | O_TRUNC);
  return !!fd;
}

base::Optional<int64_t> ReadFtraceNowTs(const base::ScopedFile& cpu_stats_fd) {
  PERFETTO_CHECK(cpu_stats_fd);

  char buf[512];
  ssize_t res = PERFETTO_EINTR(pread(*cpu_stats_fd, buf, sizeof(buf) - 1, 0));
  if (res <= 0)
    return base::nullopt;
  buf[res] = '\0';

  FtraceCpuStats stats{};
  DumpCpuStats(buf, &stats);
  return static_cast<int64_t>(stats.now_ts * 1000 * 1000 * 1000);
}

}  // namespace

// Method of last resort to reset ftrace state.
// We don't know what state the rest of the system and process is so as far
// as possible avoid allocations.
bool HardResetFtraceState() {
  for (const char* const* item = FtraceProcfs::kTracingPaths; *item; ++item) {
    std::string prefix(*item);
    PERFETTO_CHECK(base::EndsWith(prefix, "/"));
    bool res = true;
    res &= WriteToFile((prefix + "tracing_on").c_str(), "0");
    res &= WriteToFile((prefix + "buffer_size_kb").c_str(), "4");
    // We deliberately don't check for this as on some older versions of Android
    // events/enable was not writable by the shell user.
    WriteToFile((prefix + "events/enable").c_str(), "0");
    res &= ClearFile((prefix + "trace").c_str());
    if (res)
      return true;
  }
  return false;
}

// static
std::unique_ptr<FtraceController> FtraceController::Create(
    base::TaskRunner* runner,
    Observer* observer) {
  std::unique_ptr<FtraceProcfs> ftrace_procfs =
      FtraceProcfs::CreateGuessingMountPoint();

  if (!ftrace_procfs)
    return nullptr;

  auto table = ProtoTranslationTable::Create(
      ftrace_procfs.get(), GetStaticEventInfo(), GetStaticCommonFieldsInfo());

  if (!table)
    return nullptr;

  AtraceHalWrapper hal;
  auto vendor_evts =
      vendor_tracepoints::DiscoverVendorTracepoints(&hal, ftrace_procfs.get());

  std::unique_ptr<FtraceConfigMuxer> model = std::unique_ptr<FtraceConfigMuxer>(
      new FtraceConfigMuxer(ftrace_procfs.get(), table.get(), vendor_evts));
  return std::unique_ptr<FtraceController>(
      new FtraceController(std::move(ftrace_procfs), std::move(table),
                           std::move(model), runner, observer));
}

FtraceController::FtraceController(std::unique_ptr<FtraceProcfs> ftrace_procfs,
                                   std::unique_ptr<ProtoTranslationTable> table,
                                   std::unique_ptr<FtraceConfigMuxer> model,
                                   base::TaskRunner* task_runner,
                                   Observer* observer)
    : task_runner_(task_runner),
      observer_(observer),
      symbolizer_(new LazyKernelSymbolizer()),
      ftrace_procfs_(std::move(ftrace_procfs)),
      table_(std::move(table)),
      ftrace_config_muxer_(std::move(model)),
      ftrace_clock_snapshot_(new FtraceClockSnapshot()),
      weak_factory_(this) {}

FtraceController::~FtraceController() {
  for (const auto* data_source : data_sources_)
    ftrace_config_muxer_->RemoveConfig(data_source->config_id());
  data_sources_.clear();
  started_data_sources_.clear();
  StopIfNeeded();
}

uint64_t FtraceController::NowMs() const {
  return static_cast<uint64_t>(base::GetWallTimeMs().count());
}

void FtraceController::StartIfNeeded() {
  using FtraceClock = protos::pbzero::FtraceClock;
  if (started_data_sources_.size() > 1)
    return;
  PERFETTO_DCHECK(!started_data_sources_.empty());
  PERFETTO_DCHECK(per_cpu_.empty());

  // Lazily allocate the memory used for reading & parsing ftrace.
  if (!parsing_mem_.IsValid()) {
    parsing_mem_ =
        base::PagedMemory::Allocate(base::kPageSize * kParsingBufferSizePages);
  }

  // If we're not using the boot clock, snapshot the ftrace clock.
  FtraceClock clock = ftrace_config_muxer_->ftrace_clock();
  if (clock != FtraceClock::FTRACE_CLOCK_UNSPECIFIED) {
    cpu_zero_stats_fd_ = ftrace_procfs_->OpenCpuStats(0 /* cpu */);
    MaybeSnapshotFtraceClock();
  }

  per_cpu_.clear();
  per_cpu_.reserve(ftrace_procfs_->NumberOfCpus());
  size_t period_page_quota = ftrace_config_muxer_->GetPerCpuBufferSizePages();
  for (size_t cpu = 0; cpu < ftrace_procfs_->NumberOfCpus(); cpu++) {
    auto reader = std::unique_ptr<CpuReader>(new CpuReader(
        cpu, table_.get(), symbolizer_.get(), ftrace_clock_snapshot_.get(),
        ftrace_procfs_->OpenPipeForCpu(cpu)));
    per_cpu_.emplace_back(std::move(reader), period_page_quota);
  }

  // Start the repeating read tasks.
  auto generation = ++generation_;
  auto drain_period_ms = GetDrainPeriodMs();
  auto weak_this = weak_factory_.GetWeakPtr();
  task_runner_->PostDelayedTask(
      [weak_this, generation] {
        if (weak_this)
          weak_this->ReadTick(generation);
      },
      drain_period_ms - (NowMs() % drain_period_ms));
}

// We handle the ftrace buffers in a repeating task (ReadTick). On a given tick,
// we iterate over all per-cpu buffers, parse their contents, and then write out
// the serialized packets. This is handled by |CpuReader| instances, which
// attempt to read from their respective per-cpu buffer fd until they catch up
// to the head of the buffer, or hit a transient error.
//
// The readers work in batches of |kParsingBufferSizePages| pages for cache
// locality, and to limit memory usage.
//
// However, the reading happens on the primary thread, shared with the rest of
// the service (including ipc). If there is a lot of ftrace data to read, we
// want to yield to the event loop, re-enqueueing a continuation task at the end
// of the immediate queue (letting other enqueued tasks to run before
// continuing). Therefore we introduce |kMaxPagesPerCpuPerReadTick|.
//
// There is also a possibility that the ftrace bandwidth is particularly high.
// We do not want to continue trying to catch up to the event stream (via
// continuation tasks) without bound, as we want to limit our cpu% usage.  We
// assume that given a config saying "per-cpu kernel ftrace buffer is N pages,
// and drain every T milliseconds", we should not read more than N pages per
// drain period. Therefore we introduce |per_cpu_.period_page_quota|. If the
// consumer wants to handle a high bandwidth of ftrace events, they should set
// the config values appropriately.
void FtraceController::ReadTick(int generation) {
  metatrace::ScopedEvent evt(metatrace::TAG_FTRACE,
                             metatrace::FTRACE_READ_TICK);
  if (started_data_sources_.empty() || generation != generation_) {
    return;
  }

#if PERFETTO_DCHECK_IS_ON()
  // The OnFtraceDataWrittenIntoDataSourceBuffers() below is supposed to clear
  // all metadata, including the |kernel_addrs| map for symbolization.
  for (FtraceDataSource* ds : started_data_sources_) {
    FtraceMetadata* ftrace_metadata = ds->mutable_metadata();
    PERFETTO_DCHECK(ftrace_metadata->kernel_addrs.empty());
    PERFETTO_DCHECK(ftrace_metadata->last_kernel_addr_index_written == 0);
  }
#endif

  // Read all cpu buffers with remaining per-period quota.
  bool all_cpus_done = true;
  uint8_t* parsing_buf = reinterpret_cast<uint8_t*>(parsing_mem_.Get());
  const auto ftrace_clock = ftrace_config_muxer_->ftrace_clock();
  for (size_t i = 0; i < per_cpu_.size(); i++) {
    size_t orig_quota = per_cpu_[i].period_page_quota;
    if (orig_quota == 0)
      continue;

    size_t max_pages = std::min(orig_quota, kMaxPagesPerCpuPerReadTick);
    CpuReader& cpu_reader = *per_cpu_[i].reader;
    cpu_reader.set_ftrace_clock(ftrace_clock);
    size_t pages_read = cpu_reader.ReadCycle(
        parsing_buf, kParsingBufferSizePages, max_pages, started_data_sources_);

    size_t new_quota = (pages_read >= orig_quota) ? 0 : orig_quota - pages_read;
    per_cpu_[i].period_page_quota = new_quota;

    // Reader got stopped by the cap on the number of pages (to not do too much
    // work on the shared thread at once), but can read more in this drain
    // period. Repost the ReadTick (on the immediate queue) to iterate over all
    // cpus again. In other words, we will keep reposting work for all cpus as
    // long as at least one of them hits the read page cap each tick. If all
    // readers catch up to the event stream (pages_read < max_pages), or exceed
    // their quota, we will stop for the given period.
    PERFETTO_DCHECK(pages_read <= max_pages);
    if (pages_read == max_pages && new_quota > 0)
      all_cpus_done = false;
  }
  observer_->OnFtraceDataWrittenIntoDataSourceBuffers();

  // More work to do in this period.
  auto weak_this = weak_factory_.GetWeakPtr();
  if (!all_cpus_done) {
    PERFETTO_DLOG("Reposting immediate ReadTick as there's more work.");
    task_runner_->PostTask([weak_this, generation] {
      if (weak_this)
        weak_this->ReadTick(generation);
    });
  } else {
    // Done until next drain period.
    size_t period_page_quota = ftrace_config_muxer_->GetPerCpuBufferSizePages();
    for (auto& per_cpu : per_cpu_)
      per_cpu.period_page_quota = period_page_quota;

    // Snapshot the clock so the data in the next period will be clock synced as
    // well.
    MaybeSnapshotFtraceClock();

    auto drain_period_ms = GetDrainPeriodMs();
    task_runner_->PostDelayedTask(
        [weak_this, generation] {
          if (weak_this)
            weak_this->ReadTick(generation);
        },
        drain_period_ms - (NowMs() % drain_period_ms));
  }
}

uint32_t FtraceController::GetDrainPeriodMs() {
  if (data_sources_.empty())
    return kDefaultDrainPeriodMs;
  uint32_t min_drain_period_ms = kMaxDrainPeriodMs + 1;
  for (const FtraceDataSource* data_source : data_sources_) {
    if (data_source->config().drain_period_ms() < min_drain_period_ms)
      min_drain_period_ms = data_source->config().drain_period_ms();
  }
  return ClampDrainPeriodMs(min_drain_period_ms);
}

void FtraceController::ClearTrace() {
  ftrace_procfs_->ClearTrace();
}

void FtraceController::DisableAllEvents() {
  ftrace_procfs_->DisableAllEvents();
}

void FtraceController::WriteTraceMarker(const std::string& s) {
  ftrace_procfs_->WriteTraceMarker(s);
}

void FtraceController::Flush(FlushRequestID flush_id) {
  metatrace::ScopedEvent evt(metatrace::TAG_FTRACE,
                             metatrace::FTRACE_CPU_FLUSH);

  // Read all cpus in one go, limiting the per-cpu read amount to make sure we
  // don't get stuck chasing the writer if there's a very high bandwidth of
  // events.
  size_t per_cpu_buf_size_pages =
      ftrace_config_muxer_->GetPerCpuBufferSizePages();
  uint8_t* parsing_buf = reinterpret_cast<uint8_t*>(parsing_mem_.Get());
  for (size_t i = 0; i < per_cpu_.size(); i++) {
    per_cpu_[i].reader->ReadCycle(parsing_buf, kParsingBufferSizePages,
                                  per_cpu_buf_size_pages,
                                  started_data_sources_);
  }
  observer_->OnFtraceDataWrittenIntoDataSourceBuffers();

  for (FtraceDataSource* data_source : started_data_sources_)
    data_source->OnFtraceFlushComplete(flush_id);
}

void FtraceController::StopIfNeeded() {
  if (!started_data_sources_.empty())
    return;

  // We are not implicitly flushing on Stop. The tracing service is supposed to
  // ask for an explicit flush before stopping, unless it needs to perform a
  // non-graceful stop.

  per_cpu_.clear();
  symbolizer_->Destroy();
  cpu_zero_stats_fd_.reset();

  if (parsing_mem_.IsValid()) {
    parsing_mem_.AdviseDontNeed(parsing_mem_.Get(), parsing_mem_.size());
  }
}

bool FtraceController::AddDataSource(FtraceDataSource* data_source) {
  if (!ValidConfig(data_source->config()))
    return false;

  auto config_id = ftrace_config_muxer_->SetupConfig(
      data_source->config(), data_source->mutable_setup_errors());
  if (!config_id)
    return false;

  const FtraceDataSourceConfig* ds_config =
      ftrace_config_muxer_->GetDataSourceConfig(config_id);
  auto it_and_inserted = data_sources_.insert(data_source);
  PERFETTO_DCHECK(it_and_inserted.second);
  data_source->Initialize(config_id, ds_config);
  return true;
}

bool FtraceController::StartDataSource(FtraceDataSource* data_source) {
  PERFETTO_DCHECK(data_sources_.count(data_source) > 0);

  FtraceConfigId config_id = data_source->config_id();
  PERFETTO_CHECK(config_id);

  if (!ftrace_config_muxer_->ActivateConfig(config_id))
    return false;

  started_data_sources_.insert(data_source);
  StartIfNeeded();

  // If the config is requesting to symbolize kernel addresses, create the
  // symbolizer and parse /proc/kallsyms (it will take 200-300 ms). This is not
  // strictly required here but is to avoid hitting the parsing cost while
  // processing the first ftrace event batch in CpuReader.
  if (data_source->config().symbolize_ksyms()) {
    if (data_source->config().initialize_ksyms_synchronously_for_testing()) {
      symbolizer_->GetOrCreateKernelSymbolMap();
    } else {
      auto weak_this = weak_factory_.GetWeakPtr();
      task_runner_->PostTask([weak_this] {
        if (weak_this)
          weak_this->symbolizer_->GetOrCreateKernelSymbolMap();
      });
    }
  }

  return true;
}

void FtraceController::RemoveDataSource(FtraceDataSource* data_source) {
  started_data_sources_.erase(data_source);
  size_t removed = data_sources_.erase(data_source);
  if (!removed)
    return;  // Can happen if AddDataSource failed (e.g. too many sessions).
  ftrace_config_muxer_->RemoveConfig(data_source->config_id());
  StopIfNeeded();
}

void FtraceController::DumpFtraceStats(FtraceStats* stats) {
  DumpAllCpuStats(ftrace_procfs_.get(), stats);
  if (symbolizer_ && symbolizer_->is_valid()) {
    auto* symbol_map = symbolizer_->GetOrCreateKernelSymbolMap();
    stats->kernel_symbols_parsed =
        static_cast<uint32_t>(symbol_map->num_syms());
    stats->kernel_symbols_mem_kb =
        static_cast<uint32_t>(symbol_map->size_bytes() / 1024);
  }
}

void FtraceController::MaybeSnapshotFtraceClock() {
  if (!cpu_zero_stats_fd_)
    return;

  auto ftrace_clock = ftrace_config_muxer_->ftrace_clock();
  PERFETTO_DCHECK(ftrace_clock != protos::pbzero::FTRACE_CLOCK_UNSPECIFIED);

  // Snapshot the boot clock *before* reading CPU stats so that
  // two clocks are as close togher as possible (i.e. if it was the
  // other way round, we'd skew by the const of string parsing).
  ftrace_clock_snapshot_->boot_clock_ts = base::GetBootTimeNs().count();

  // A value of zero will cause this snapshot to be skipped.
  ftrace_clock_snapshot_->ftrace_clock_ts =
      ReadFtraceNowTs(cpu_zero_stats_fd_).value_or(0);
}

FtraceController::Observer::~Observer() = default;

}  // namespace perfetto
