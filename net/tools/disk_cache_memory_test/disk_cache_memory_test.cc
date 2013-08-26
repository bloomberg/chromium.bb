// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "net/base/cache_type.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/disk_cache.h"
#include "net/disk_cache/simple/simple_backend_impl.h"
#include "net/disk_cache/simple/simple_index.h"

namespace disk_cache {
namespace {

const char kBlockFileBackendType[] = "block_file";
const char kSimpleBackendType[] = "simple";

const char kDiskCacheType[] = "disk_cache";
const char kAppCacheType[] = "app_cache";

const char kPrivateDirty[] = "Private_Dirty:";
const char kReadWrite[] = "rw-";
const char kHeap[] = "[heap]";
const char kKb[] = "kB";

void SetSuccessCodeOnCompletion(base::RunLoop* run_loop,
                                bool* succeeded,
                                int net_error) {
  if (net_error == net::OK) {
    *succeeded = true;
  } else {
    *succeeded = false;
  }
  run_loop->Quit();
}

scoped_ptr<Backend> CreateAndInitBackend(
    const net::BackendType& backend_type,
    const net::CacheType& cache_type,
    const base::FilePath& cache_path) {
  scoped_ptr<Backend> result;
  scoped_ptr<Backend> backend;
  bool succeeded = false;
  base::RunLoop run_loop;
  const net::CompletionCallback callback = base::Bind(
      &SetSuccessCodeOnCompletion,
      base::Unretained(&run_loop),
      base::Unretained(&succeeded));
  const int net_error = CreateCacheBackend(
      cache_type, backend_type, cache_path, 0, false,
      base::MessageLoopProxy::current(), NULL, &backend, callback);
  if (net_error == net::OK)
    callback.Run(net::OK);
  else
    run_loop.Run();
  if (!succeeded) {
    LOG(ERROR) << "Could not initialize backend in "
               << cache_path.LossyDisplayName();
    return result.Pass();
  }
  // For the simple cache, the index may not be initialized yet.
  if (backend_type == net::CACHE_BACKEND_SIMPLE) {
    base::RunLoop index_run_loop;
    const net::CompletionCallback index_callback = base::Bind(
        &SetSuccessCodeOnCompletion,
        base::Unretained(&index_run_loop),
        base::Unretained(&succeeded));
    SimpleBackendImpl* simple_backend =
        static_cast<SimpleBackendImpl*>(backend.get());
    const int index_net_error =
        simple_backend->index()->ExecuteWhenReady(index_callback);
    if (index_net_error == net::OK)
      index_callback.Run(net::OK);
    else
      index_run_loop.Run();
    if (!succeeded) {
      LOG(ERROR) << "Could not initialize Simple Cache in "
                 << cache_path.LossyDisplayName();
      return result.Pass();
    }
  }
  DCHECK(backend);
  result.swap(backend);
  return result.Pass();
}

// Parses range lines from /proc/<PID>/smaps, e.g. (anonymous read write):
// 7f819d88b000-7f819d890000 rw-p 00000000 00:00 0
bool ParseRangeLine(const std::string& line,
                    std::vector<std::string>* tokens,
                    bool* is_anonymous_read_write) {
  tokens->clear();
  base::SplitStringAlongWhitespace(line, tokens);
  if (tokens->size() == 5) {
    const std::string& mode = (*tokens)[1];
    *is_anonymous_read_write = !mode.compare(0, 3, kReadWrite);
    return true;
  }
  // On Android, most of the memory is allocated in the heap, instead of being
  // mapped.
  if (tokens->size() == 6) {
    const std::string& type = (*tokens)[5];
    *is_anonymous_read_write = (type == kHeap);
    return true;
  }
  return false;
}

// Parses range property lines from /proc/<PID>/smaps, e.g.:
// Private_Dirty:        16 kB
bool ParseRangeProperty(const std::string& line,
                        std::vector<std::string>* tokens,
                        uint64* size,
                        bool* is_private_dirty) {
  tokens->clear();
  base::SplitStringAlongWhitespace(line, tokens);
  if (tokens->size() != 3)
    return false;
  const std::string& type = (*tokens)[0];
  if (type != kPrivateDirty)
    return true;
  const std::string& unit = (*tokens)[2];
  if (unit != kKb) {
    LOG(WARNING) << "Discarding value not in kB: " << line;
    return true;
  }
  const std::string& size_str = (*tokens)[1];
  uint64 map_size = 0;
  if (!base::StringToUint64(size_str, &map_size))
    return false;
  *is_private_dirty = true;
  *size = map_size;
  return true;
}

uint64 GetMemoryConsumption() {
  std::ifstream maps_file(
      base::StringPrintf("/proc/%d/smaps", getpid()).c_str());
  if (!maps_file.good()) {
    LOG(ERROR) << "Could not open smaps file.";
    return false;
  }
  std::string line;
  std::vector<std::string> tokens;
  uint64 total_size = 0;
  if (!std::getline(maps_file, line) || line.empty())
    return total_size;
  while (true) {
    bool is_anonymous_read_write = false;
    if (!ParseRangeLine(line, &tokens, &is_anonymous_read_write)) {
      LOG(WARNING) << "Parsing smaps - did not expect line: " << line;
    }
    if (!std::getline(maps_file, line) || line.empty())
      return total_size;
    bool is_private_dirty = false;
    uint64 size = 0;
    while (ParseRangeProperty(line, &tokens, &size, &is_private_dirty)) {
      if (is_anonymous_read_write && is_private_dirty) {
        total_size += size;
        is_private_dirty = false;
      }
      if (!std::getline(maps_file, line) || line.empty())
        return total_size;
    }
  }
  return total_size;
}

bool CacheMemTest(const net::BackendType& backend_type,
                  const net::CacheType& cache_type,
                  const base::FilePath& file_path) {
  const scoped_ptr<Backend> backend =
      CreateAndInitBackend(backend_type, cache_type, file_path);
  if (!backend)
    return false;
  std::cout << "Number of entries: " << backend->GetEntryCount() << std::endl;
  const uint64 memory_consumption = GetMemoryConsumption();
  std::cout << "Private dirty memory: " << memory_consumption << " kB"
            << std::endl;
  return true;
}

void PrintUsage(std::ostream* stream) {
  *stream << "Usage: cache_mem_test "
          << "--backend-type=<backend_type> "
          << "--cache-type=<cache_type> "
          << "--cache-path=<cache_path>"
          << std::endl
          << "  with <backend_type>='block_file'|'simple'" << std::endl
          << "       <cache_type>='disk_cache'|'app_cache'" << std::endl
          << "       <cache_path>=file system path" << std::endl;
}

bool Main(int argc, char** argv) {
  base::AtExitManager at_exit_manager;
  base::MessageLoopForIO message_loop;
  CommandLine::Init(argc, argv);
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch("help")) {
    PrintUsage(&std::cout);
    return true;
  }
  if (command_line.GetSwitches().size() != 3 ||
      !command_line.HasSwitch("backend-type") ||
      !command_line.HasSwitch("cache-type") ||
      !command_line.HasSwitch("cache-path")) {
    PrintUsage(&std::cerr);
    return false;
  }
  const std::string backend_type_str =
      command_line.GetSwitchValueASCII("backend-type");
  const std::string cache_type_str =
      command_line.GetSwitchValueASCII("cache-type");
  const base::FilePath cache_path =
      command_line.GetSwitchValuePath("cache-path");
  if (backend_type_str != kBlockFileBackendType &&
      backend_type_str != kSimpleBackendType) {
    PrintUsage(&std::cerr);
    return false;
  }
  if (cache_type_str != kDiskCacheType && cache_type_str != kAppCacheType) {
    PrintUsage(&std::cerr);
    return false;
  }
  const net::BackendType backend_type =
      (backend_type_str == kBlockFileBackendType) ?
      net::CACHE_BACKEND_BLOCKFILE : net::CACHE_BACKEND_SIMPLE;
  const net::CacheType cache_type = (cache_type_str == kDiskCacheType) ?
      net::DISK_CACHE : net::APP_CACHE;
  return CacheMemTest(backend_type, cache_type, cache_path);
}

}  // namespace
}  // namespace disk_cache

int main(int argc, char** argv) {
  return !disk_cache::Main(argc, argv);
}
