// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/heap_profiling/multi_process/test_driver.h"

#include <algorithm>
#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/process/process_handle.h"
#include "base/run_loop.h"
#include "base/sampling_heap_profiler/poisson_allocation_sampler.h"
#include "base/stl_util.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "base/threading/platform_thread.h"
#include "base/trace_event/heap_profiler_event_filter.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/heap_profiling/multi_process/supervisor.h"
#include "components/services/heap_profiling/public/cpp/controller.h"
#include "components/services/heap_profiling/public/cpp/profiling_client.h"
#include "components/services/heap_profiling/public/cpp/settings.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/tracing_controller.h"

namespace heap_profiling {

namespace {

constexpr const char kTestCategory[] = "kTestCategory";
const char kMallocEvent[] = "kMallocEvent";
const char kMallocTypeTag[] = "kMallocTypeTag";
const char kMallocVariadicTypeTag[] = "kMallocVariadicTypeTag";
const char kPAEvent[] = "kPAEvent";
const char kVariadicEvent[] = "kVariadicEvent";
const char kThreadName[] = "kThreadName";

// Make some specific allocations in Browser to do a deeper test of the
// allocation tracking.
constexpr int kMallocAllocSize = 700;
constexpr int kMallocAllocCount = 1570;

constexpr int kVariadicAllocCount = 1000;

// The sample rate should not affect the sampled allocations. Intentionally
// choose an odd number.
constexpr int kSampleRate = 777;

// Test fixed-size partition alloc. The size must be aligned to system pointer
// size.
constexpr int kPartitionAllocSize = 8 * 25;
constexpr int kPartitionAllocCount = 2000;
static const char* kPartitionAllocTypeName = "kPartitionAllocTypeName";

// Ideally, we'd check to see that at least one renderer exists, and all
// renderers are being profiled, but something odd seems to be happening with
// warm-up/spare renderers.
//
// Whether at least 1 renderer exists, and at least 1 renderer is being
// profiled.
bool RenderersAreBeingProfiled(
    const std::vector<base::ProcessId>& profiled_pids) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  for (auto iter = content::RenderProcessHost::AllHostsIterator();
       !iter.IsAtEnd(); iter.Advance()) {
    if (iter.GetCurrentValue()->GetProcess().Handle() ==
        base::kNullProcessHandle)
      continue;
    base::ProcessId pid = iter.GetCurrentValue()->GetProcess().Pid();
    if (base::Contains(profiled_pids, pid)) {
      return true;
    }
  }

  return false;
}

// On success, populates |pid|.
int NumProcessesWithName(base::Value* dump_json,
                         std::string name,
                         std::vector<int>* pids) {
  int num_processes = 0;
  base::Value* events = dump_json->FindKey("traceEvents");
  for (const base::Value& event : events->GetList()) {
    const base::Value* found_name =
        event.FindKeyOfType("name", base::Value::Type::STRING);
    if (!found_name)
      continue;
    if (found_name->GetString() != "process_name")
      continue;
    const base::Value* found_args =
        event.FindKeyOfType("args", base::Value::Type::DICTIONARY);
    if (!found_args)
      continue;
    const base::Value* found_process_name =
        found_args->FindKeyOfType("name", base::Value::Type::STRING);
    if (!found_process_name)
      continue;
    if (found_process_name->GetString() != name)
      continue;

    if (pids) {
      const base::Value* found_pid =
          event.FindKeyOfType("pid", base::Value::Type::INTEGER);
      if (!found_pid) {
        LOG(ERROR) << "Process missing pid.";
        return 0;
      }
      pids->push_back(found_pid->GetInt());
    }

    ++num_processes;
  }
  return num_processes;
}

base::Value* FindArgDump(base::ProcessId pid,
                         base::Value* dump_json,
                         const char* arg) {
  base::Value* events = dump_json->FindKey("traceEvents");
  base::Value* dumps = nullptr;
  base::Value* heaps_v2 = nullptr;
  for (base::Value& event : events->GetList()) {
    const base::Value* found_name =
        event.FindKeyOfType("name", base::Value::Type::STRING);
    if (!found_name)
      continue;
    if (found_name->GetString() != "periodic_interval")
      continue;
    const base::Value* found_pid =
        event.FindKeyOfType("pid", base::Value::Type::INTEGER);
    if (!found_pid)
      continue;
    if (static_cast<base::ProcessId>(found_pid->GetInt()) != pid)
      continue;
    dumps = &event;
    heaps_v2 = dumps->FindPath({"args", "dumps", arg});
    if (heaps_v2)
      return heaps_v2;
  }
  return nullptr;
}

constexpr uint64_t kNullParent = std::numeric_limits<int>::max();
struct Node {
  int name_id;
  std::string name;
  int parent_id = kNullParent;
};
using NodeMap = std::unordered_map<uint64_t, Node>;

// Parses maps.types and maps.strings. Returns |true| on success.
bool ParseTypes(base::Value* heaps_v2, NodeMap* output) {
  base::Value* types = heaps_v2->FindPath({"maps", "types"});
  for (const base::Value& type_value : types->GetList()) {
    const base::Value* id = type_value.FindKey("id");
    const base::Value* name_sid = type_value.FindKey("name_sid");
    if (!id || !name_sid) {
      LOG(ERROR) << "Node missing id or name_sid field";
      return false;
    }

    Node node;
    node.name_id = name_sid->GetInt();
    (*output)[id->GetInt()] = node;
  }

  base::Value* strings = heaps_v2->FindPath({"maps", "strings"});
  for (const base::Value& string_value : strings->GetList()) {
    const base::Value* id = string_value.FindKey("id");
    const base::Value* string = string_value.FindKey("string");
    if (!id || !string) {
      LOG(ERROR) << "String struct missing id or string field";
      return false;
    }
    for (auto& pair : *output) {
      if (pair.second.name_id == id->GetInt()) {
        pair.second.name = string->GetString();
        break;
      }
    }
  }

  return true;
}

// |expected_size| of 0 means no expectation.
bool GetAllocatorSubarray(base::Value* heaps_v2,
                          const char* allocator_name,
                          const char* subarray_name,
                          size_t expected_size,
                          base::Value::ConstListView* output) {
  base::Value* subarray =
      heaps_v2->FindPath({"allocators", allocator_name, subarray_name});
  if (!subarray) {
    LOG(ERROR) << "Failed to find path: 'allocators." << allocator_name << "."
               << subarray_name << "' in heaps v2";
    return false;
  }

  base::Value::ConstListView subarray_list = subarray->GetList();
  if (expected_size && subarray_list.size() != expected_size) {
    LOG(ERROR) << subarray_name << " has wrong size";
    return false;
  }

  *output = subarray_list;
  return true;
}

bool ValidateSamplingAllocations(base::Value* heaps_v2,
                                 const char* allocator_name,
                                 int approximate_size,
                                 int approximate_count,
                                 const char* type_name) {
  // Maps type ids to strings.
  NodeMap type_map;
  if (!ParseTypes(heaps_v2, &type_map))
    return false;

  bool found = false;
  int id_of_type = 0;
  for (auto& pair : type_map) {
    if (pair.second.name == type_name) {
      id_of_type = pair.first;
      found = true;
    }
  }

  if (!found) {
    LOG(ERROR) << "Failed to find type with name: " << type_name;
    return false;
  }

  // Find the type with the appropriate id.
  base::Value::ConstListView types_list;
  if (!GetAllocatorSubarray(heaps_v2, allocator_name, "types", 0,
                            &types_list)) {
    return false;
  }

  // Look up the size.
  base::Value::ConstListView sizes;
  if (!GetAllocatorSubarray(heaps_v2, allocator_name, "sizes",
                            types_list.size(), &sizes)) {
    return false;
  }

  // Look up the count.
  base::Value::ConstListView counts;
  if (!GetAllocatorSubarray(heaps_v2, allocator_name, "counts",
                            types_list.size(), &counts)) {
    return false;
  }

  int allocations_with_matching_type = 0;
  size_t index = 0;
  for (size_t i = 0; i < types_list.size(); ++i) {
    if (types_list[i].GetInt() == id_of_type) {
      index = i;
      ++allocations_with_matching_type;
    }
  }

  if (allocations_with_matching_type != 1) {
    LOG(ERROR) << "Expected 1 but found " << allocations_with_matching_type
               << " allocations with matching type";
    return false;
  }

  if (sizes[index].GetInt() < approximate_size / 2 ||
      sizes[index].GetInt() > approximate_size * 2) {
    LOG(ERROR) << "sampling size " << sizes[index].GetInt()
               << " was not within a factor of 2 of expected size "
               << approximate_size;
    return false;
  }

  if (counts[index].GetInt() < approximate_count / 2 ||
      counts[index].GetInt() > approximate_count * 2) {
    LOG(ERROR) << "sampling size " << counts[index].GetInt()
               << " was not within a factor of 2 of expected count "
               << approximate_count;
    return false;
  }
  return true;
}

bool ValidateProcessMmaps(base::Value* process_mmaps,
                          bool should_have_contents) {
  base::Value* vm_regions = nullptr;
  size_t count = 0;
  if (process_mmaps) {
    vm_regions = process_mmaps->FindKey("vm_regions");
    count = vm_regions->GetList().size();
  }
  if (!should_have_contents) {
    if (count != 0) {
      LOG(ERROR) << "vm_regions should be empty, but has contents";
      return false;
    }
    return true;
  }

  if (count == 0) {
    LOG(ERROR) << "vm_regions should have contents, but doesn't";
    return false;
  }

  // File paths may contain PII. Make sure that "mf" entries only contain the
  // basename, rather than a full path.
  for (const base::Value& vm_region : vm_regions->GetList()) {
    const base::Value* file_path_value = vm_region.FindKey("mf");
    if (file_path_value) {
      std::string file_path = file_path_value->GetString();

      base::FilePath::StringType path(file_path.begin(), file_path.end());
      if (base::FilePath(path).BaseName().AsUTF8Unsafe() != file_path) {
        LOG(ERROR) << "vm_region should not contain file path: " << file_path;
        return false;
      }
    }
  }

  return true;
}

}  // namespace

TestDriver::TestDriver()
    : wait_for_ui_thread_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                          base::WaitableEvent::InitialState::NOT_SIGNALED) {
  partition_allocator_.init();
}
TestDriver::~TestDriver() = default;

bool TestDriver::RunTest(const Options& options) {
  options_ = options;

  running_on_ui_thread_ =
      content::BrowserThread::CurrentlyOn(content::BrowserThread::UI);

  // The only thing to test for Mode::kNone is that profiling hasn't started.
  if (options_.mode == Mode::kNone) {
    if (running_on_ui_thread_) {
      has_started_ = Supervisor::GetInstance()->HasStarted();
    } else {
      base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                     base::BindOnce(&TestDriver::GetHasStartedOnUIThread,
                                    base::Unretained(this)));
      wait_for_ui_thread_.Wait();
    }
    if (has_started_) {
      LOG(ERROR) << "Profiling should not have started";
      return false;
    }
    return true;
  }

  if (running_on_ui_thread_) {
    if (!CheckOrStartProfilingOnUIThreadWithNestedRunLoops())
      return false;
    if (ShouldProfileRenderer())
      WaitForProfilingToStartForAllRenderersUIThread();
    if (ShouldProfileBrowser())
      MakeTestAllocations();
    CollectResults(true);
  } else {
    base::PostTask(
        FROM_HERE, {content::BrowserThread::UI},
        base::BindOnce(&TestDriver::CheckOrStartProfilingOnUIThreadAndSignal,
                       base::Unretained(this)));
    wait_for_ui_thread_.Wait();
    if (!initialization_success_)
      return false;
    if (ShouldProfileRenderer()) {
      base::PostTask(
          FROM_HERE, {content::BrowserThread::UI},
          base::BindOnce(
              &TestDriver::
                  WaitForProfilingToStartForAllRenderersUIThreadAndSignal,
              base::Unretained(this)));
      wait_for_ui_thread_.Wait();
    }
    if (ShouldProfileBrowser()) {
      base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                     base::BindOnce(&TestDriver::MakeTestAllocations,
                                    base::Unretained(this)));
    }
    base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                   base::BindOnce(&TestDriver::CollectResults,
                                  base::Unretained(this), false));
    wait_for_ui_thread_.Wait();
  }

  std::unique_ptr<base::Value> dump_json =
      base::JSONReader::ReadDeprecated(serialized_trace_);
  if (!dump_json) {
    LOG(ERROR) << "Failed to deserialize trace.";
    return false;
  }

  if (!ValidateBrowserAllocations(dump_json.get())) {
    LOG(ERROR) << "Failed to validate browser allocations";
    return false;
  }

  if (!ValidateRendererAllocations(dump_json.get())) {
    LOG(ERROR) << "Failed to validate renderer allocations";
    return false;
  }

  return true;
}

void TestDriver::GetHasStartedOnUIThread() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  has_started_ = Supervisor::GetInstance()->HasStarted();
  wait_for_ui_thread_.Signal();
}

void TestDriver::CheckOrStartProfilingOnUIThreadAndSignal() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  initialization_success_ =
      CheckOrStartProfilingOnUIThreadWithAsyncSignalling();

  // If the flag is true, then the WaitableEvent will be signaled after
  // profiling has started.
  if (!wait_for_profiling_to_start_)
    wait_for_ui_thread_.Signal();
}

bool TestDriver::CheckOrStartProfilingOnUIThreadWithAsyncSignalling() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  if (options_.profiling_already_started) {
    if (!Supervisor::GetInstance()->HasStarted()) {
      LOG(ERROR) << "Profiling should have been started, but wasn't";
      return false;
    }

    // Even if profiling has started, it's possible that the allocator shim
    // has not yet been initialized. Wait for it.
    if (ShouldProfileBrowser()) {
      bool already_initialized = SetOnInitAllocatorShimCallbackForTesting(
          base::BindOnce(&base::WaitableEvent::Signal,
                         base::Unretained(&wait_for_ui_thread_)),
          base::ThreadTaskRunnerHandle::Get());
      if (!already_initialized) {
        wait_for_profiling_to_start_ = true;
      }
    }
    return true;
  }

  wait_for_profiling_to_start_ = true;
  base::OnceClosure start_callback;

  // If we're going to profile the browser, then wait for the allocator shim to
  // start. Otherwise, wait for the Supervisor to start.
  if (ShouldProfileBrowser()) {
    SetOnInitAllocatorShimCallbackForTesting(
        base::BindOnce(&base::WaitableEvent::Signal,
                       base::Unretained(&wait_for_ui_thread_)),
        base::ThreadTaskRunnerHandle::Get());
  } else {
    start_callback = base::BindOnce(&base::WaitableEvent::Signal,
                                    base::Unretained(&wait_for_ui_thread_));
  }

  Supervisor::GetInstance()->Start(options_.mode, options_.stack_mode,
                                   kSampleRate, std::move(start_callback));

  return true;
}

bool TestDriver::CheckOrStartProfilingOnUIThreadWithNestedRunLoops() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  if (options_.profiling_already_started) {
    if (!Supervisor::GetInstance()->HasStarted()) {
      LOG(ERROR) << "Profiling should have been started, but wasn't";
      return false;
    }

    // Even if profiling has started, it's possible that the allocator shim
    // has not yet been initialized. Wait for it.
    if (ShouldProfileBrowser()) {
      WaitForProfilingToStartForBrowserUIThread();
    }
    return true;
  }

  // When this is not-null, initialization should wait for the QuitClosure to be
  // called.
  std::unique_ptr<base::RunLoop> run_loop(new base::RunLoop);
  base::OnceClosure start_callback;

  // If we're going to profile the browser, then wait for the allocator shim to
  // start. Otherwise, wait for the Supervisor to start.
  if (ShouldProfileBrowser()) {
    SetOnInitAllocatorShimCallbackForTesting(
        run_loop->QuitClosure(), base::ThreadTaskRunnerHandle::Get());
  } else {
    start_callback = run_loop->QuitClosure();
  }

  Supervisor::GetInstance()->Start(options_.mode, options_.stack_mode,
                                   kSampleRate, std::move(start_callback));
  run_loop->Run();
  if (ShouldProfileBrowser()) {
    WaitForProfilingToStartForBrowserUIThread();
  }

  return true;
}

void TestDriver::MakeTestAllocations() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  base::PlatformThread::SetName(kThreadName);

  // Profiling is turned on by default with a sampling interval of 1MB. We reset
  // the sampling interval, but the change isn't picked by the thread-local
  // cache until we make a sufficient number of allocations. This is an
  // implementation limitation, but it's easy to work around by making a couple
  // of large allocations.
  constexpr int kFourMegsAllocation = 1 << 22;
  for (int i = 0; i < 10; ++i) {
    leaks_.push_back(static_cast<char*>(malloc(kFourMegsAllocation)));
  }

  // We must reserve vector size in advance so that the vector allocation does
  // not get tagged as TRACE_HEAP_PROFILER_API_SCOPED_TASK_EXECUTION.
  leaks_.reserve(leaks_.size() + kPartitionAllocCount);
  {
    TRACE_EVENT0(kTestCategory, kPAEvent);
    for (int i = 0; i < kPartitionAllocCount; ++i) {
      leaks_.push_back(static_cast<char*>(partition_allocator_.root()->Alloc(
          kPartitionAllocSize, kPartitionAllocTypeName)));
    }
  }

  leaks_.reserve(leaks_.size() + kMallocAllocCount);
  {
    TRACE_HEAP_PROFILER_API_SCOPED_TASK_EXECUTION event(kMallocTypeTag);
    TRACE_EVENT0(kTestCategory, kMallocEvent);

    for (int i = 0; i < kMallocAllocCount; ++i) {
      leaks_.push_back(new char[kMallocAllocSize]);
    }
  }

  leaks_.reserve(leaks_.size() + kVariadicAllocCount);
  {
    TRACE_HEAP_PROFILER_API_SCOPED_TASK_EXECUTION event(kMallocVariadicTypeTag);
    TRACE_EVENT0(kTestCategory, kVariadicEvent);

    for (int i = 0; i < kVariadicAllocCount; ++i) {
      leaks_.push_back(new char[i + kMallocAllocSize]);  // Variadic allocation.
      total_variadic_allocations_ += i + kMallocAllocSize;
    }
  }
}

void TestDriver::CollectResults(bool synchronous) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  base::OnceClosure finish_tracing_closure;
  std::unique_ptr<base::RunLoop> run_loop;

  if (synchronous) {
    run_loop.reset(new base::RunLoop);
    finish_tracing_closure = run_loop->QuitClosure();
  } else {
    finish_tracing_closure = base::BindOnce(
        &base::WaitableEvent::Signal, base::Unretained(&wait_for_ui_thread_));
  }

  Supervisor::GetInstance()->RequestTraceWithHeapDump(
      base::BindOnce(&TestDriver::TraceFinished, base::Unretained(this),
                     std::move(finish_tracing_closure)),
      /* anonymize= */ true);

  if (synchronous)
    run_loop->Run();
}

void TestDriver::TraceFinished(base::OnceClosure closure,
                               bool success,
                               std::string trace_json) {
  serialized_trace_.swap(trace_json);
  std::move(closure).Run();
}

bool TestDriver::ValidateBrowserAllocations(base::Value* dump_json) {
  base::Value* heaps_v2 =
      FindArgDump(base::Process::Current().Pid(), dump_json, "heaps_v2");

  if (!ShouldProfileBrowser()) {
    if (heaps_v2) {
      LOG(ERROR) << "There should be no heap dump for the browser.";
      return false;
    }
    return true;
  }

  if (!heaps_v2) {
    LOG(ERROR) << "Browser heap dump missing.";
    return false;
  }

  bool result = false;

  bool should_validate_dumps = true;
#if defined(OS_ANDROID) && !defined(OFFICIAL_BUILD)
  // TODO(ajwong): This step fails on Nexus 5X devices running kit-kat. It works
  // on Nexus 5X devices running oreo. The problem is that all allocations have
  // the same [an effectively empty] backtrace and get glommed together. More
  // investigation is necessary. For now, I'm turning this off for Android.
  // https://crbug.com/786450.
  if (!HasPseudoFrames())
    should_validate_dumps = false;
#endif

  std::string thread_name = ShouldIncludeNativeThreadNames() ? kThreadName : "";

  if (should_validate_dumps) {
    result = ValidateSamplingAllocations(heaps_v2, "malloc",
                                         kMallocAllocSize * kMallocAllocCount,
                                         kMallocAllocCount, kMallocTypeTag);
    if (!result) {
      LOG(ERROR) << "Failed to validate malloc fixed allocations";
      return false;
    }

    result = ValidateSamplingAllocations(
        heaps_v2, "malloc", total_variadic_allocations_, kVariadicAllocCount,
        kMallocVariadicTypeTag);
    if (!result) {
      LOG(ERROR) << "Failed to validate malloc variadic allocations";
      return false;
    }
    result = ValidateSamplingAllocations(
        heaps_v2, "partition_alloc", kPartitionAllocSize * kPartitionAllocCount,
        kPartitionAllocCount, kPartitionAllocTypeName);
    if (!result) {
      LOG(ERROR) << "Failed to validate sampling allocations";
      return false;
    }
  }

  int process_count = NumProcessesWithName(dump_json, "Browser", nullptr);
  if (process_count != 1) {
    LOG(ERROR) << "Found " << process_count
               << " processes with name: Browser. Expected 1.";
    return false;
  }

  base::Value* process_mmaps =
      FindArgDump(base::Process::Current().Pid(), dump_json, "process_mmaps");
  if (!ValidateProcessMmaps(process_mmaps, HasNativeFrames())) {
    LOG(ERROR) << "Failed to validate browser process mmaps.";
    return false;
  }

  return true;
}

bool TestDriver::ValidateRendererAllocations(base::Value* dump_json) {
  // On Android Webview, there is may not be a separate Renderer process. If we
  // are not asked to profile the Renderer, do not perform any Renderer checks.
  if (!ShouldProfileRenderer())
    return true;

  std::vector<int> pids;
  bool result = NumProcessesWithName(dump_json, "Renderer", &pids) >= 1;
  if (!result) {
    LOG(ERROR) << "Failed to find process with name Renderer";
    return false;
  }

  for (int pid : pids) {
    base::ProcessId renderer_pid = static_cast<base::ProcessId>(pid);
    base::Value* heaps_v2 = FindArgDump(renderer_pid, dump_json, "heaps_v2");
    if (!heaps_v2) {
      LOG(ERROR) << "Failed to find heaps v2 for renderer";
      return false;
    }

    base::Value* process_mmaps =
        FindArgDump(renderer_pid, dump_json, "process_mmaps");
    if (!ValidateProcessMmaps(process_mmaps, HasNativeFrames())) {
      LOG(ERROR) << "Failed to validate renderer process mmaps.";
      return false;
    }
  }

  return true;
}

bool TestDriver::ShouldProfileBrowser() {
  return options_.mode == Mode::kAll || options_.mode == Mode::kBrowser ||
         options_.mode == Mode::kMinimal ||
         options_.mode == Mode::kUtilityAndBrowser;
}

bool TestDriver::ShouldProfileRenderer() {
  return options_.mode == Mode::kAll || options_.mode == Mode::kAllRenderers;
}

bool TestDriver::ShouldIncludeNativeThreadNames() {
  return options_.stack_mode == mojom::StackMode::NATIVE_WITH_THREAD_NAMES;
}

bool TestDriver::HasPseudoFrames() {
  return options_.stack_mode == mojom::StackMode::PSEUDO ||
         options_.stack_mode == mojom::StackMode::MIXED;
}

bool TestDriver::HasNativeFrames() {
  return options_.stack_mode == mojom::StackMode::NATIVE_WITH_THREAD_NAMES ||
         options_.stack_mode == mojom::StackMode::NATIVE_WITHOUT_THREAD_NAMES ||
         options_.stack_mode == mojom::StackMode::MIXED;
}

void TestDriver::WaitForProfilingToStartForBrowserUIThread() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  while (true) {
    std::vector<base::ProcessId> profiled_pids;
    base::RunLoop run_loop;
    auto callback = base::BindLambdaForTesting(
        [&profiled_pids, &run_loop](std::vector<base::ProcessId> pids) {
          profiled_pids = std::move(pids);
          run_loop.Quit();
        });
    Supervisor::GetInstance()->GetProfiledPids(std::move(callback));
    run_loop.Run();

    if (std::find(profiled_pids.begin(), profiled_pids.end(),
                  base::GetCurrentProcId()) != profiled_pids.end()) {
      break;
    }
  }
}

void TestDriver::WaitForProfilingToStartForAllRenderersUIThread() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  while (true) {
    std::vector<base::ProcessId> profiled_pids;
    base::RunLoop run_loop;
    auto callback = base::BindOnce(
        [](std::vector<base::ProcessId>* results, base::OnceClosure finished,
           std::vector<base::ProcessId> pids) {
          results->swap(pids);
          std::move(finished).Run();
        },
        &profiled_pids, run_loop.QuitClosure());
    Supervisor::GetInstance()->GetProfiledPids(std::move(callback));
    run_loop.Run();

    if (RenderersAreBeingProfiled(profiled_pids))
      break;
  }
}

void TestDriver::WaitForProfilingToStartForAllRenderersUIThreadAndSignal() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  Supervisor::GetInstance()->GetProfiledPids(base::BindOnce(
      &TestDriver::WaitForProfilingToStartForAllRenderersUIThreadCallback,
      base::Unretained(this)));
}

void TestDriver::WaitForProfilingToStartForAllRenderersUIThreadCallback(
    std::vector<base::ProcessId> results) {
  if (RenderersAreBeingProfiled(results)) {
    wait_for_ui_thread_.Signal();
    return;
  }

  // Brief sleep to prevent spamming the task queue, since this code is called
  // in a tight loop.
  base::PlatformThread::Sleep(base::TimeDelta::FromMicroseconds(100));

  WaitForProfilingToStartForAllRenderersUIThreadAndSignal();
}

}  // namespace heap_profiling
