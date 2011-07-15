/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// this file provides a bunch of glue code to make it possible to link in some
// of the files from native_client/command_buffer/
// Most of the code is stubbed out but the SharedMemory class is reimplemented
// using NaCl primitives

#include <sys/mman.h>

#include <stdio.h>
#include <unistd.h>
#include <iostream>

#include "base/logging.h"
#include "base/process_util.h"
#include "base/shared_memory.h"
#include "base/debug/trace_event.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "native_client/src/trusted/desc/nacl_desc_imc.h"

// ======================================================================
//
// ======================================================================
namespace logging {

const char* const log_severity_names[LOG_NUM_SEVERITIES ] = {
  "INFO", "WARNING", "ERROR", "ERROR_REPORT", "FATAL" };

void LogMessage::Init(const char* file, int line) {
  if (severity_ >= 0)
    stream_ << log_severity_names[severity_];
  else
    stream_ << "VERBOSE" << -severity_;
  stream_ << " ";
}

LogMessage::LogMessage(const char* file,
                       int line,
                       LogSeverity severity,
                       int ctr)
  : severity_(severity), line_(line) {
  Init(file, line);
}

LogMessage::LogMessage(const char* file,
                       int line) : line_(line) {
  Init(file, line);
}

LogMessage::LogMessage(const char* file,
                       int line,
                       LogSeverity severity)
  : severity_(severity), line_(line) {
  Init(file, line);
}

LogMessage::LogMessage(const char* file,
                       int line,
                       LogSeverity severity,
                       std::string* result)
    : severity_(severity), file_(file), line_(line) {
  Init(file, line);
  stream_ << "Check failed: " << *result;
  delete result;
}

LogMessage::~LogMessage() {
  stream_ << std::endl;
  std::string str_newline(stream_.str());
  // problems with unit tests, especially on the buildbots.
  fprintf(stdout, "%s", str_newline.c_str());
  fflush(stdout);
  if (severity_ ==  LOG_FATAL) {
    exit(1);
  }
}


int GetMinLogLevel() {
  return -1;
}


DcheckState g_dcheck_state;
}


// ======================================================================
//
// ======================================================================
namespace base {

ProcessHandle GetCurrentProcessHandle() {
  return getpid();
}

}


// ======================================================================
//
// ======================================================================
namespace base {

SharedMemory::SharedMemory() {
}

SharedMemory::~SharedMemory() {
  // CHECK(false) << "unimplemented " << __func__;
}


SharedMemory::SharedMemory(base::FileDescriptor handle, bool read_only)   :
  mapped_file_(handle.fd),
  mapped_size_(0),
  inode_(0),
  memory_(NULL),
  read_only_(read_only),
  created_size_(0) {
}


bool SharedMemory::CreateAnonymous(unsigned int size) {
  DCHECK_EQ(-1, mapped_file_);
  if (size == 0) return false;

  nacl::DescWrapperFactory factory;
  nacl::DescWrapper* dw = factory.MakeShm(size);

  created_size_ = size;
  mapped_file_ = ((NaClDescImcConnectedDesc*)dw->desc())->h;

  return true;
}


bool SharedMemory::CreateAndMapAnonymous(uint32 size) {
  CHECK(false) << "unimplemented";
  return false;
}


bool SharedMemory::Map(unsigned int bytes) {
  LOG(INFO) << "about to map " << bytes << " bytes";
  if (mapped_file_ == -1)
    return false;

  memory_ = mmap(NULL, bytes, PROT_READ | (read_only_ ? 0 : PROT_WRITE),
                 MAP_SHARED, mapped_file_, 0);
  if (memory_)
    mapped_size_ = bytes;

  LOG(INFO) << "map address is " << memory_;
  bool mmap_succeeded = (memory_ != (void*)-1);
  DCHECK(mmap_succeeded) << "Call to mmap failed";
  return mmap_succeeded;
}

bool SharedMemory::ShareToProcessCommon(int process,
                                        base::FileDescriptor* new_handle,
                                        bool close_self) {
  const int new_fd = dup(mapped_file_);
  LOG(INFO) << "ShareToProcessCommon dup:" << mapped_file_ << " -> " << new_fd;

  DCHECK_GE(new_fd, 0);
  new_handle->fd = new_fd;
  new_handle->auto_close = true;
  if (close_self) {
    CHECK(false) << "unreachable";
  }

  return true;
}

SharedMemoryHandle SharedMemory::handle() const {
  return FileDescriptor(mapped_file_, false);
}

}

// ======================================================================
//
// ======================================================================
namespace base {

namespace debug {

const TraceCategory* TraceLog::GetCategory(const char* name) {
  CHECK(false) << "unimplemented";
  return 0;
}

TraceLog* TraceLog::GetInstance() {
  CHECK(false) << "unimplemented";
  return 0;
}

int TraceLog::AddTraceEvent(TraceEventPhase phase,
                            const TraceCategory* category,
                            const char* name,
                            const char* arg1_name, TraceValue arg1_val,
                            const char* arg2_name, TraceValue arg2_val,
                            int threshold_begin_id,
                            int64 threshold) {
  CHECK(false) << "unimplemented";
  return 0;
}

void TraceValue::Destroy() {
  CHECK(false) << "unimplemented";
}

namespace internal {

void TraceEndOnScopeClose::Initialize(
  base::debug::TraceCategory const*, char const*) {
  CHECK(false) << "unimplemented";
}

void TraceEndOnScopeClose::AddEventIfEnabled() {
  CHECK(false) << "unimplemented";
}

void TraceEndOnScopeCloseThreshold::Initialize(
  base::debug::TraceCategory const*,
  char const*,
  int threshold_begin_id,
  int64 threshold) {
  CHECK(false) << "unimplemented";
}

void TraceEndOnScopeCloseThreshold::AddEventIfEnabled() {
  CHECK(false) << "unimplemented";
}


}  // namespace internal

}  // namespace debug

}  // namespace base
