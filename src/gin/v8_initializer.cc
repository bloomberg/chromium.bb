// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/v8_initializer.h"

#include <stddef.h>
#include <stdint.h>

#include <cstdint>
#include <memory>

#include "base/allocator/partition_allocator/page_allocator.h"
#include "base/allocator/partition_allocator/partition_address_space.h"
#include "base/check.h"
#include "base/debug/alias.h"
#include "base/debug/crash_logging.h"
#include "base/feature_list.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/memory_mapped_file.h"
#include "base/lazy_instance.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/win/windows_version.h"
#include "build/build_config.h"
#include "gin/gin_features.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "v8/include/v8-initialization.h"
#include "v8/include/v8-snapshot.h"

#if defined(V8_USE_EXTERNAL_STARTUP_DATA)
#if BUILDFLAG(IS_ANDROID)
#include "base/android/apk_assets.h"
#elif BUILDFLAG(IS_MAC)
#include "base/mac/foundation_util.h"
#endif
#endif  // V8_USE_EXTERNAL_STARTUP_DATA

namespace gin {

namespace {

// This global is never freed nor closed.
base::MemoryMappedFile* g_mapped_snapshot = nullptr;

#if defined(V8_USE_EXTERNAL_STARTUP_DATA)
absl::optional<gin::V8SnapshotFileType> g_snapshot_file_type;
#endif

bool GenerateEntropy(unsigned char* buffer, size_t amount) {
  base::RandBytes(buffer, amount);
  return true;
}

void GetMappedFileData(base::MemoryMappedFile* mapped_file,
                       v8::StartupData* data) {
  if (mapped_file) {
    data->data = reinterpret_cast<const char*>(mapped_file->data());
    data->raw_size = static_cast<int>(mapped_file->length());
  } else {
    data->data = nullptr;
    data->raw_size = 0;
  }
}

#if defined(V8_USE_EXTERNAL_STARTUP_DATA)

#if BUILDFLAG(IS_ANDROID)
const char kV8ContextSnapshotFileName64[] = "v8_context_snapshot_64.bin";
const char kV8ContextSnapshotFileName32[] = "v8_context_snapshot_32.bin";
const char kSnapshotFileName64[] = "snapshot_blob_64.bin";
const char kSnapshotFileName32[] = "snapshot_blob_32.bin";

#if defined(__LP64__)
#define kV8ContextSnapshotFileName kV8ContextSnapshotFileName64
#define kSnapshotFileName kSnapshotFileName64
#else
#define kV8ContextSnapshotFileName kV8ContextSnapshotFileName32
#define kSnapshotFileName kSnapshotFileName32
#endif

#else  // BUILDFLAG(IS_ANDROID)
#if defined(USE_V8_CONTEXT_SNAPSHOT)
const char kV8ContextSnapshotFileName[] = V8_CONTEXT_SNAPSHOT_FILENAME;
#endif
const char kSnapshotFileName[] = "snapshot_blob.bin";
#endif  // BUILDFLAG(IS_ANDROID)

const char* GetSnapshotFileName(const V8SnapshotFileType file_type) {
  switch (file_type) {
    case V8SnapshotFileType::kDefault:
      return kSnapshotFileName;
    case V8SnapshotFileType::kWithAdditionalContext:
#if defined(USE_V8_CONTEXT_SNAPSHOT)
      return kV8ContextSnapshotFileName;
#else
      NOTREACHED();
      return nullptr;
#endif
  }
  NOTREACHED();
  return nullptr;
}

void GetV8FilePath(const char* file_name, base::FilePath* path_out) {
#if BUILDFLAG(IS_ANDROID)
  // This is the path within the .apk.
  *path_out =
      base::FilePath(FILE_PATH_LITERAL("assets")).AppendASCII(file_name);
#elif BUILDFLAG(IS_MAC)
  base::ScopedCFTypeRef<CFStringRef> bundle_resource(
      base::SysUTF8ToCFStringRef(file_name));
  *path_out = base::mac::PathForFrameworkBundleResource(bundle_resource);
#else
  base::FilePath data_path;
  bool r = base::PathService::Get(base::DIR_ASSETS, &data_path);
  DCHECK(r);
  *path_out = data_path.AppendASCII(file_name);
#endif
}

bool MapV8File(base::File file,
               base::MemoryMappedFile::Region region,
               base::MemoryMappedFile** mmapped_file_out) {
  DCHECK(*mmapped_file_out == NULL);
  std::unique_ptr<base::MemoryMappedFile> mmapped_file(
      new base::MemoryMappedFile());
  if (mmapped_file->Initialize(std::move(file), region)) {
    *mmapped_file_out = mmapped_file.release();
    return true;
  }
  return false;
}

base::File OpenV8File(const char* file_name,
                      base::MemoryMappedFile::Region* region_out) {
  // Re-try logic here is motivated by http://crbug.com/479537
  // for A/V on Windows (https://support.microsoft.com/en-us/kb/316609).

  // These match tools/metrics/histograms.xml
  enum OpenV8FileResult {
    OPENED = 0,
    OPENED_RETRY,
    FAILED_IN_USE,
    FAILED_OTHER,
    MAX_VALUE
  };
  base::FilePath path;
  GetV8FilePath(file_name, &path);

#if BUILDFLAG(IS_ANDROID)
  base::File file(base::android::OpenApkAsset(path.value(), region_out));
  OpenV8FileResult result = file.IsValid() ? OpenV8FileResult::OPENED
                                           : OpenV8FileResult::FAILED_OTHER;
#else
  // Re-try logic here is motivated by http://crbug.com/479537
  // for A/V on Windows (https://support.microsoft.com/en-us/kb/316609).
  const int kMaxOpenAttempts = 5;
  const int kOpenRetryDelayMillis = 250;

  OpenV8FileResult result = OpenV8FileResult::FAILED_IN_USE;
  int flags = base::File::FLAG_OPEN | base::File::FLAG_READ;
  base::File file;
  for (int attempt = 0; attempt < kMaxOpenAttempts; attempt++) {
    file.Initialize(path, flags);
    if (file.IsValid()) {
      *region_out = base::MemoryMappedFile::Region::kWholeFile;
      if (attempt == 0) {
        result = OpenV8FileResult::OPENED;
        break;
      } else {
        result = OpenV8FileResult::OPENED_RETRY;
        break;
      }
    } else if (file.error_details() != base::File::FILE_ERROR_IN_USE) {
      result = OpenV8FileResult::FAILED_OTHER;
      break;
    } else if (kMaxOpenAttempts - 1 != attempt) {
      base::PlatformThread::Sleep(base::Milliseconds(kOpenRetryDelayMillis));
    }
  }
#endif  // BUILDFLAG(IS_ANDROID)

  UMA_HISTOGRAM_ENUMERATION("V8.Initializer.OpenV8File.Result", result,
                            OpenV8FileResult::MAX_VALUE);
  return file;
}

#endif  // defined(V8_USE_EXTERNAL_STARTUP_DATA)

template <int LENGTH>
void SetV8Flags(const char (&flag)[LENGTH]) {
  v8::V8::SetFlagsFromString(flag, LENGTH - 1);
}

void SetV8FlagsFormatted(const char* format, ...) {
  char buffer[128];
  va_list args;
  va_start(args, format);
  int length = base::vsnprintf(buffer, sizeof(buffer), format, args);
  if (length <= 0 || sizeof(buffer) <= static_cast<unsigned>(length)) {
    PLOG(ERROR) << "Invalid formatted V8 flag: " << format;
    return;
  }
  v8::V8::SetFlagsFromString(buffer, length - 1);
}

template <size_t N, size_t M>
void SetV8FlagsIfOverridden(const base::Feature& feature,
                            const char (&enabling_flag)[N],
                            const char (&disabling_flag)[M]) {
  auto overridden_state = base::FeatureList::GetStateIfOverridden(feature);
  if (!overridden_state.has_value()) {
    return;
  }
  if (overridden_state.value()) {
    SetV8Flags(enabling_flag);
  } else {
    SetV8Flags(disabling_flag);
  }
}

void SetFlags(IsolateHolder::ScriptMode mode,
              const std::string js_command_line_flags) {
  // We assume that all feature flag defaults correspond to the default
  // values of the corresponding V8 flags.
  SetV8FlagsIfOverridden(features::kV8CompactCodeSpaceWithStack,
                         "--compact-code-space-with-stack",
                         "--no-compact-code-space-with-stack");
  SetV8FlagsIfOverridden(features::kV8CompactWithStack, "--compact-with-stack",
                         "--no-compact-with-stack");
  SetV8FlagsIfOverridden(features::kV8OptimizeJavascript, "--opt", "--no-opt");
  SetV8FlagsIfOverridden(features::kV8FlushBytecode, "--flush-bytecode",
                         "--no-flush-bytecode");
  SetV8FlagsIfOverridden(features::kV8FlushBaselineCode,
                         "--flush-baseline-code", "--no-flush-baseline-code");
  SetV8FlagsIfOverridden(features::kV8OffThreadFinalization,
                         "--finalize-streaming-on-background",
                         "--no-finalize-streaming-on-background");
  SetV8FlagsIfOverridden(features::kV8LazyFeedbackAllocation,
                         "--lazy-feedback-allocation",
                         "--no-lazy-feedback-allocation");
  SetV8FlagsIfOverridden(features::kV8ConcurrentInlining,
                         "--concurrent_inlining", "--no-concurrent_inlining");
  SetV8FlagsIfOverridden(features::kV8PerContextMarkingWorklist,
                         "--stress-per-context-marking-worklist",
                         "--no-stress-per-context-marking-worklist");
  SetV8FlagsIfOverridden(features::kV8FlushEmbeddedBlobICache,
                         "--experimental-flush-embedded-blob-icache",
                         "--no-experimental-flush-embedded-blob-icache");
  SetV8FlagsIfOverridden(features::kV8ReduceConcurrentMarkingTasks,
                         "--gc-experiment-reduce-concurrent-marking-tasks",
                         "--no-gc-experiment-reduce-concurrent-marking-tasks");
  SetV8FlagsIfOverridden(features::kV8NoReclaimUnmodifiedWrappers,
                         "--no-reclaim-unmodified-wrappers",
                         "--reclaim-unmodified-wrappers");
  SetV8FlagsIfOverridden(
      features::kV8ExperimentalRegexpEngine,
      "--enable-experimental-regexp-engine-on-excessive-backtracks",
      "--no-enable-experimental-regexp-engine-on-excessive-backtracks");
  SetV8FlagsIfOverridden(features::kV8TurboFastApiCalls,
                         "--turbo-fast-api-calls", "--no-turbo-fast-api-calls");
  SetV8FlagsIfOverridden(features::kV8Turboprop, "--turboprop",
                         "--no-turboprop");
  SetV8FlagsIfOverridden(features::kV8Sparkplug, "--sparkplug",
                         "--no-sparkplug");
  SetV8FlagsIfOverridden(features::kV8ConcurrentSparkplug,
                         "--concurrent-sparkplug", "--no-concurrent-sparkplug");
  SetV8FlagsIfOverridden(features::kV8SparkplugNeedsShortBuiltinCalls,
                         "--sparkplug-needs-short-builtins",
                         "--no-sparkplug-needs-short-builtins");
  SetV8FlagsIfOverridden(features::kV8ShortBuiltinCalls,
                         "--short-builtin-calls", "--no-short-builtin-calls");
  SetV8FlagsIfOverridden(features::kV8CodeMemoryWriteProtection,
                         "--write-protect-code-memory",
                         "--no-write-protect-code-memory");
  SetV8FlagsIfOverridden(features::kV8SlowHistograms, "--slow-histograms",
                         "--no-slow-histograms");

  if (base::FeatureList::IsEnabled(features::kV8ConcurrentSparkplug)) {
    if (int max_threads = features::kV8ConcurrentSparkplugMaxThreads.Get()) {
      SetV8FlagsFormatted("--concurrent-sparkplug-max-threads=%i", max_threads);
    }
  }

  if (base::FeatureList::IsEnabled(features::kV8ScriptAblation)) {
    if (int delay = features::kV8ScriptDelayMs.Get()) {
      SetV8FlagsFormatted("--script-delay=%i", delay);
    }
    if (int delay = features::kV8ScriptDelayOnceMs.Get()) {
      SetV8FlagsFormatted("--script-delay-once=%i", delay);
    }
    if (double fraction = features::kV8ScriptDelayFraction.Get()) {
      SetV8FlagsFormatted("--script-delay-fraction=%f", fraction);
    }
  }

  // Make sure aliases of kV8SlowHistograms only enable the feature to
  // avoid contradicting settings between multiple finch experiments.
  bool any_slow_histograms_alias =
      base::FeatureList::IsEnabled(
          features::kV8SlowHistogramsCodeMemoryWriteProtection) ||
      base::FeatureList::IsEnabled(features::kV8SlowHistogramsSparkplug) ||
      base::FeatureList::IsEnabled(
          features::kV8SlowHistogramsSparkplugAndroid) ||
      base::FeatureList::IsEnabled(features::kV8SlowHistogramsScriptAblation);
  if (any_slow_histograms_alias) {
    SetV8Flags("--slow-histograms");
  } else {
    SetV8FlagsIfOverridden(features::kV8SlowHistograms, "--slow-histograms",
                           "--no-slow-histograms");
  }

  if (IsolateHolder::kStrictMode == mode) {
    SetV8Flags("--use_strict");
  }

  if (js_command_line_flags.empty())
    return;

  // Allow the --js-flags switch to override existing flags:
  std::vector<base::StringPiece> flag_list =
      base::SplitStringPiece(js_command_line_flags, ",", base::TRIM_WHITESPACE,
                             base::SPLIT_WANT_NONEMPTY);
  for (const auto& flag : flag_list) {
    v8::V8::SetFlagsFromString(std::string(flag).c_str(), flag.size());
  }
}

}  // namespace

// static
void V8Initializer::Initialize(IsolateHolder::ScriptMode mode,
                               const std::string js_command_line_flags) {
  static bool v8_is_initialized = false;
  if (v8_is_initialized)
    return;

  v8::V8::InitializePlatform(V8Platform::Get());

  // Set this early on as some initialization steps, such as the initialization
  // of the virtual memory cage, already use V8's random number generator.
  v8::V8::SetEntropySource(&GenerateEntropy);

#if defined(V8_SANDBOX)
  static_assert(ARCH_CPU_64_BITS, "V8 sandbox can only work in 64-bit builds");
  // For now, initializing the sandbox is optional, and we only do it if the
  // correpsonding feature is enabled. In the future, it will be mandatory when
  // compiling with V8_SANDBOX.
  // However, if V8 uses sandboxed pointers, then the sandbox must be
  // initialized as sandboxed pointers are simply offsets inside the sandbox.
#if defined(V8_SANDBOXED_POINTERS)
  bool must_initialize_sandbox = true;
#else
  bool must_initialize_sandbox = false;
#endif

  bool v8_sandbox_is_initialized = false;
  if (must_initialize_sandbox ||
      base::FeatureList::IsEnabled(features::kV8VirtualMemoryCage)) {
    v8_sandbox_is_initialized = v8::V8::InitializeSandbox();
    CHECK(!must_initialize_sandbox || v8_sandbox_is_initialized);

    // Record the size of the sandbox, in GB. The size will always be a power
    // of two, so we use a sparse histogram to capture it. If the
    // initialization failed, this API will return zero. The main reason for
    // capturing this histogram here instead of having V8 do it is that there
    // are no Isolates available yet, which are required for recording
    // histograms in V8.
    size_t size = v8::V8::GetSandboxSizeInBytes();
    int sizeInGB = size >> 30;
    DCHECK(base::bits::IsPowerOfTwo(size));
    DCHECK(size == 0 || sizeInGB > 0);
    // This uses the term "cage" instead of "sandbox" for historical reasons.
    // TODO(1218005) remove this once the finch trial has ended.
    base::UmaHistogramSparse("V8.VirtualMemoryCageSizeGB", sizeInGB);
  }
#endif  // V8_SANDBOX

  SetFlags(mode, js_command_line_flags);

#if defined(V8_USE_EXTERNAL_STARTUP_DATA)
  if (g_mapped_snapshot) {
    v8::StartupData snapshot;
    GetMappedFileData(g_mapped_snapshot, &snapshot);
    v8::V8::SetSnapshotDataBlob(&snapshot);
  }
#endif  // V8_USE_EXTERNAL_STARTUP_DATA

  v8::V8::Initialize();

  v8_is_initialized = true;

#if defined(V8_SANDBOX)
  if (v8_sandbox_is_initialized) {
    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused. This should match enum
    // V8VirtualMemoryCageMode in \tools\metrics\histograms\enums.xml
    // This uses the term "cage" instead of "sandbox" for historical reasons.
    // TODO(1218005) remove this once the finch trial has ended.
    enum class VirtualMemoryCageMode {
      kSecure = 0,
      kInsecure = 1,
      kMaxValue = kInsecure,
    };
    base::UmaHistogramEnumeration("V8.VirtualMemoryCageMode",
                                  v8::V8::IsSandboxConfiguredSecurely()
                                      ? VirtualMemoryCageMode::kSecure
                                      : VirtualMemoryCageMode::kInsecure);

    // When the sandbox is enabled, ArrayBuffers must be allocated inside of
    // it. To achieve that, PA's ConfigurablePool is created inside the sandbox
    // and Blink then creates the ArrayBuffer partition in that Pool.
    v8::VirtualAddressSpace* sandbox_address_space =
        v8::V8::GetSandboxAddressSpace();
    const size_t max_pool_size =
        base::internal::PartitionAddressSpace::ConfigurablePoolMaxSize();
    const size_t min_pool_size =
        base::internal::PartitionAddressSpace::ConfigurablePoolMinSize();
    size_t pool_size = max_pool_size;
#if BUILDFLAG(IS_WIN)
    // On Windows prior to 8.1 we allocate a smaller Pool since reserving
    // virtual memory is expensive on these OSes.
    if (base::win::GetVersion() < base::win::Version::WIN8_1) {
      // The size chosen here should be synchronized with the size of the
      // virtual memory reservation for the V8 sandbox on these platforms.
      // Currently, that is 8GB, of which 4GB are used for V8's pointer
      // compression region.
      // TODO(saelo) give this constant a proper name and maybe move it
      // somewhere else.
      constexpr size_t kGB = 1ULL << 30;
      pool_size = 4ULL * kGB;
      DCHECK_LE(pool_size, max_pool_size);
      DCHECK_GE(pool_size, min_pool_size);
    }
#endif
    // Try to reserve the maximum size of the pool at first, then keep halving
    // the size on failure until it succeeds.
    uintptr_t pool_base = 0;
    while (!pool_base && pool_size >= min_pool_size) {
      pool_base = sandbox_address_space->AllocatePages(
          0, pool_size, pool_size, v8::PagePermissions::kNoAccess);
      if (!pool_base) {
        pool_size /= 2;
      }
    }
    // The V8 sandbox is guaranteed to be large enough to host the pool.
    CHECK(pool_base);
    base::internal::PartitionAddressSpace::InitConfigurablePool(pool_base,
                                                                pool_size);
    // TODO(saelo) maybe record the size of the Pool into UMA.
  }
#endif  // V8_SANDBOX
}

// static
void V8Initializer::GetV8ExternalSnapshotData(v8::StartupData* snapshot) {
  GetMappedFileData(g_mapped_snapshot, snapshot);
}

// static
void V8Initializer::GetV8ExternalSnapshotData(const char** snapshot_data_out,
                                              int* snapshot_size_out) {
  v8::StartupData snapshot;
  GetV8ExternalSnapshotData(&snapshot);
  *snapshot_data_out = snapshot.data;
  *snapshot_size_out = snapshot.raw_size;
}

#if defined(V8_USE_EXTERNAL_STARTUP_DATA)

// static
void V8Initializer::LoadV8Snapshot(V8SnapshotFileType snapshot_file_type) {
  if (g_mapped_snapshot) {
    // TODO(crbug.com/802962): Confirm not loading different type of snapshot
    // files in a process.
    return;
  }

  base::MemoryMappedFile::Region file_region;
  base::File file =
      OpenV8File(GetSnapshotFileName(snapshot_file_type), &file_region);
  LoadV8SnapshotFromFile(std::move(file), &file_region, snapshot_file_type);
}

// static
void V8Initializer::LoadV8SnapshotFromFile(
    base::File snapshot_file,
    base::MemoryMappedFile::Region* snapshot_file_region,
    V8SnapshotFileType snapshot_file_type) {
  if (g_mapped_snapshot)
    return;

  if (!snapshot_file.IsValid()) {
    LOG(FATAL) << "Error loading V8 startup snapshot file";
    return;
  }

  g_snapshot_file_type = snapshot_file_type;
  base::MemoryMappedFile::Region region =
      base::MemoryMappedFile::Region::kWholeFile;
  if (snapshot_file_region) {
    region = *snapshot_file_region;
  }

  if (!MapV8File(std::move(snapshot_file), region, &g_mapped_snapshot)) {
    LOG(FATAL) << "Error mapping V8 startup snapshot file";
    return;
  }
}

#if BUILDFLAG(IS_ANDROID)
// static
base::FilePath V8Initializer::GetSnapshotFilePath(
    bool abi_32_bit,
    V8SnapshotFileType snapshot_file_type) {
  base::FilePath path;
  const char* filename = nullptr;
  switch (snapshot_file_type) {
    case V8SnapshotFileType::kDefault:
      filename = abi_32_bit ? kSnapshotFileName32 : kSnapshotFileName64;
      break;
    case V8SnapshotFileType::kWithAdditionalContext:
      filename = abi_32_bit ? kV8ContextSnapshotFileName32
                            : kV8ContextSnapshotFileName64;
      break;
  }
  CHECK(filename);

  GetV8FilePath(filename, &path);
  return path;
}
#endif  // BUILDFLAG(IS_ANDROID)

V8SnapshotFileType GetLoadedSnapshotFileType() {
  DCHECK(g_snapshot_file_type.has_value());
  return *g_snapshot_file_type;
}

#endif  // defined(V8_USE_EXTERNAL_STARTUP_DATA)

}  // namespace gin
