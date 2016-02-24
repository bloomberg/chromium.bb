// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "net/base/io_buffer.h"
#include "net/base/test_completion_callback.h"
#include "net/disk_cache/disk_cache.h"
#include "net/disk_cache/simple/simple_backend_impl.h"
#include "net/http/http_cache.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"

using disk_cache::Backend;
using disk_cache::Entry;
using disk_cache::SimpleBackendImpl;

namespace {

base::MessageLoopForIO* g_message_loop = nullptr;

scoped_ptr<SimpleBackendImpl> CreateSimpleBackend(
    const base::FilePath& cache_path) {
  scoped_ptr<SimpleBackendImpl> cache_backend(new SimpleBackendImpl(
      cache_path, 0, net::DISK_CACHE, g_message_loop->task_runner(), NULL));

  net::TestCompletionCallback cb;
  int rv = cache_backend->Init(cb.callback());
  if (cb.GetResult(rv) != net::OK)
    return nullptr;
  return cache_backend;
}

// Print all of a cache's keys to stdout.
bool ListKeys(Backend* cache_backend) {
  scoped_ptr<Backend::Iterator> entry_iterator =
      cache_backend->CreateIterator();
  Entry* entry = nullptr;
  net::TestCompletionCallback cb;
  int rv = entry_iterator->OpenNextEntry(&entry, cb.callback());
  while (cb.GetResult(rv) == net::OK) {
    std::string url = entry->GetKey();
    std::cout << url << std::endl;
    entry->Close();
    entry = nullptr;
    rv = entry_iterator->OpenNextEntry(&entry, cb.callback());
  }
  return true;
}

// Print a key's stream to stdout.
bool GetKeyStream(Backend* cache_backend, const std::string& key, int index) {
  if (index < 0 || index > 2) {
    std::cerr << "Invalid stream index." << std::endl;
    return false;
  }

  Entry* cache_entry;
  net::TestCompletionCallback cb;
  int rv = cache_backend->OpenEntry(key, &cache_entry, cb.callback());
  if (cb.GetResult(rv) != net::OK) {
    std::cerr << "Couldn't find key's entry." << std::endl;
    return false;
  }

  const int kInitBufferSize = 8192;
  scoped_refptr<net::GrowableIOBuffer> buffer(new net::GrowableIOBuffer());
  buffer->SetCapacity(kInitBufferSize);
  while (true) {
    rv = cache_entry->ReadData(index, buffer->offset(), buffer.get(),
                               buffer->capacity() - buffer->offset(),
                               cb.callback());
    rv = cb.GetResult(rv);
    if (rv < 0) {
      cache_entry->Close();
      std::cerr << "Stream read error." << std::endl;
      return false;
    }
    buffer->set_offset(buffer->offset() + rv);
    if (rv == 0)
      break;
    buffer->SetCapacity(buffer->offset() * 2);
  }
  cache_entry->Close();
  if (index == 0) {
    net::HttpResponseInfo response_info;
    bool truncated_response_info = false;
    net::HttpCache::ParseResponseInfo(buffer->StartOfBuffer(), buffer->offset(),
                                      &response_info, &truncated_response_info);
    if (truncated_response_info) {
      std::cerr << "Truncated HTTP response." << std::endl;
      return false;
    }
    std::cout << net::HttpUtil::ConvertHeadersBackToHTTPResponse(
        response_info.headers->raw_headers());
  } else {
    std::cout.write(buffer->StartOfBuffer(), buffer->offset());
  }
  return true;
}

// Delete a specified key from the cache.
bool DeleteKey(Backend* cache_backend, const std::string& key) {
  net::TestCompletionCallback cb;
  int rv = cache_backend->DoomEntry(key, cb.callback());
  if (cb.GetResult(rv) != net::OK) {
    std::cerr << "Couldn't delete key." << std::endl;
    return false;
  }
  return true;
}

// Print the command line help.
void PrintHelp() {
  std::cout << "cachetool <cache_path> <cache_backend_type> <subcommand> ..."
            << std::endl
            << std::endl;
  std::cout << "Available cache back-end types: simple";
  std::cout << "Available subcommands:" << std::endl;
  std::cout << "  validate: Verify that the cache can be opened and return, "
            << "confirming the cache exists and is of the right type."
            << std::endl;
  std::cout << "  list_keys: List all keys in the cache." << std::endl;
  std::cout << "  get_stream <key> <index>: Print a particular stream for a"
            << " given key." << std::endl;
  std::cout << "  delete_key <key>: Delete key from cache." << std::endl;
  std::cout << "Expected values of <index> are:" << std::endl;
  std::cout << "  0 (HTTP response headers)" << std::endl;
  std::cout << "  1 (transport encoded content)" << std::endl;
  std::cout << "  2 (compiled content)" << std::endl;
}

}  // namespace

int main(int argc, char* argv[]) {
  base::AtExitManager at_exit_manager;
  base::MessageLoopForIO message_loop;
  base::CommandLine::Init(argc, argv);
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

#if defined(OS_WIN)
  std::vector<std::string> args;
  {
    base::CommandLine::StringVector wide_args = command_line.GetArgs();
    for (const auto& arg : wide_args) {
      args.push_back(base::WideToUTF8(arg));
    }
  }
#else
  base::CommandLine::StringVector args = command_line.GetArgs();
#endif
  if (args.size() < 3U) {
    PrintHelp();
    return 1;
  }

  base::FilePath cache_path(args[0]);
  std::string cache_backend_type(args[1]);
  std::string subcommand(args[2]);

  g_message_loop = &message_loop;
  scoped_ptr<Backend> cache_backend;
  if (cache_backend_type == "simple") {
    cache_backend = CreateSimpleBackend(cache_path);
  } else {
    std::cerr << "Unknown cache type." << std::endl;
    PrintHelp();
    return 1;
  }

  if (!cache_backend) {
    std::cerr << "Invalid cache." << std::endl;
    PrintHelp();
    return 1;
  }

  bool successful_command = false;
  if (subcommand == "validate") {
    if (args.size() != 3) {
      PrintHelp();
      return 1;
    }
    successful_command = true;
  } else if (subcommand == "list_keys") {
    if (args.size() != 3) {
      PrintHelp();
      return 1;
    }
    successful_command = ListKeys(cache_backend.get());
  } else if (subcommand == "get_stream") {
    if (args.size() != 5) {
      PrintHelp();
      return 1;
    }
    std::string key(args[3]);
    int index = 0;
    if (!base::StringToInt(args[4], &index)) {
      std::cerr << "<index> must be an integer." << std::endl;
      PrintHelp();
      return 1;
    }
    successful_command = GetKeyStream(cache_backend.get(), key, index);
  } else if (subcommand == "delete_key") {
    if (args.size() != 4) {
      PrintHelp();
      return 1;
    }
    std::string key(args[3]);
    successful_command = DeleteKey(cache_backend.get(), key);
  } else {
    std::cerr << "Unknown subcommand." << std::endl;
    PrintHelp();
  }
  return !successful_command;
}
