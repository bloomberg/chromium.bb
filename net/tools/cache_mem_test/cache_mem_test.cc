// Copyright (c) 2013 The Chromium Authors. All rights reserved.
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

struct CacheSpec {
 public:
  static scoped_ptr<CacheSpec> Parse(const std::string& spec_string) {
    scoped_ptr<CacheSpec> result;
    std::vector<std::string> tokens;
    base::SplitString(spec_string, ':', &tokens);
    if (tokens.size() != 3)
      return result.Pass();
    if (tokens[0] != kBlockFileBackendType && tokens[0] != kSimpleBackendType)
      return result.Pass();
    if (tokens[1] != kDiskCacheType && tokens[1] != kAppCacheType)
      return result.Pass();
    result.reset(
        new CacheSpec(
            tokens[0] == kBlockFileBackendType ?
                net::CACHE_BACKEND_BLOCKFILE : net::CACHE_BACKEND_SIMPLE,
            tokens[1] == kDiskCacheType ?  net::DISK_CACHE : net::APP_CACHE,
            base::FilePath(tokens[2])));
    return result.Pass();
  }

  const net::BackendType backend_type;
  const net::CacheType cache_type;
  const base::FilePath path;

 private:
  CacheSpec(net::BackendType backend_type,
            net::CacheType cache_type,
            const base::FilePath& path)
      : backend_type(backend_type),
        cache_type(cache_type),
        path(path) {
  }
};

void CompletionCallback(const std::string& error_msg,
                        base::RunLoop* run_loop,
                        bool* succeeded,
                        int net_error) {
  if (net_error == net::OK) {
    *succeeded = true;
  } else {
    std::cerr << error_msg << std::endl;
    *succeeded = false;
  }
  run_loop->Quit();
}

scoped_ptr<Backend> CreateBackendFromSpec(const CacheSpec& spec) {
  scoped_ptr<Backend> result;
  scoped_ptr<Backend> backend;
  bool succeeded = false;
  base::RunLoop run_loop;
  const net::CompletionCallback callback = base::Bind(
      &CompletionCallback,
      base::StringPrintf(
          "Could not initialize backend in %s", spec.path.value().c_str()),
      base::Unretained(&run_loop),
      base::Unretained(&succeeded));
  const int net_error = CreateCacheBackend(
      spec.cache_type, spec.backend_type, spec.path, 0, false,
      base::MessageLoopProxy::current(), NULL, &backend, callback);
  if (net_error == net::OK)
    callback.Run(net::OK);
  else
    run_loop.Run();
  // For the simple cache, the index may not be initialized yet.
  if (succeeded && spec.backend_type == net::CACHE_BACKEND_SIMPLE) {
    base::RunLoop index_run_loop;
    const net::CompletionCallback index_callback = base::Bind(
        &CompletionCallback,
        base::StringPrintf(
            "Could not initialize simple cache index in %s",
            spec.path.value().c_str()),
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
  }
  if (succeeded) {
    DCHECK(backend);
    result.swap(backend);
  }
  return result.Pass();
}

// Parses lines from /proc/<PID>/smaps.
// Sets |is_anonymous_read_write| when encountering a new section, e.g.
// 7f819d88b000-7f819d890000 rw-p 00000000 00:00 0 (anonymous read write)
// If |is_anonymous_read_write| is true, returns the amount of private
// dirty memory when parsing the corresponding line, e.g.:
// Private_Dirty:        16 kB
uint64 ParseMemoryMapLine(const std::string& line,
                          std::vector<std::string>* tokens,
                          bool* is_anonymous_read_write) {
  tokens->clear();
  base::SplitStringAlongWhitespace(line, tokens);
  uint64 map_size = 0;
  if (tokens->size() == 5) {
    const std::string& mode = tokens->at(1);
    if (!mode.compare(0, 3, kReadWrite)) {
      *is_anonymous_read_write = true;
      return map_size;
    }
    *is_anonymous_read_write = false;
  }
  if (tokens->size() != 3 || !*is_anonymous_read_write)
    return map_size;
  const std::string& type = tokens->at(0);
  if (!type.compare(kPrivateDirty)) {
    const std::string& size = tokens->at(1);
    if (base::StringToUint64(
            base::StringPiece(size),
            &map_size)) {
      *is_anonymous_read_write = false;
      return map_size;
    }
    map_size = 0;
  }
  return map_size;
}

uint64 getMemoryConsumption() {
  std::ifstream maps_file(
      base::StringPrintf("/proc/%d/smaps", getpid()).c_str());
  if (!maps_file.good()) {
    std::cerr << "Could not open smaps file.";
    return false;
  }
  std::string line;
  std::vector<std::string> tokens;
  bool need_size = false;
  uint64 total_size = 0;
  while (std::getline(maps_file, line) && !line.empty())
    total_size += ParseMemoryMapLine(line, &tokens, &need_size);
  return total_size;
}

bool CacheMemTest(const CacheSpec& spec) {
  const scoped_ptr<Backend> backend = CreateBackendFromSpec(spec);
  if (!backend)
    return false;
  std::cout << "Number of entries: "
            << backend->GetEntryCount()
            << std::endl;
  const uint64 memory_consumption = getMemoryConsumption();
  std::cout << "Private dirty memory: "
            << memory_consumption
            << " kB" << std::endl;
  return true;
}

void PrintUsage(std::ostream* stream) {
  *stream << "Usage: cache_mem_test "
          << "--spec=<cache_spec>"
          << std::endl
          << "  with <cache_spec>=<backend_type>:<cache_type>:<cache_path>"
          << std::endl
          << "       <backend_type>='block_file'|'simple'" << std::endl
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
  if (command_line.GetSwitches().size() != 1||
      !command_line.HasSwitch("spec")) {
    PrintUsage(&std::cerr);
    return false;
  }
  const std::string spec_str = command_line.GetSwitchValueASCII("spec");
  const scoped_ptr<CacheSpec> spec = CacheSpec::Parse(spec_str);
  if (!spec) {
    PrintUsage(&std::cerr);
    return false;
  }
  std::cout << spec->path.value() << std::endl;
  return CacheMemTest(*spec);
}

}  // namespace
}  // namespace disk_cache

int main(int argc, char** argv) {
  return !disk_cache::Main(argc, argv);
}
