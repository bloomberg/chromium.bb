// Copyright (c) 2006, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <ObjBase.h>

#include <cassert>
#include <cstdio>

#include "common/windows/string_utils-inl.h"

#include "client/windows/handler/exception_handler.h"
#include "common/windows/guid_string.h"
#include "google_airbag/common/minidump_format.h"

namespace google_airbag {

static const int kExceptionHandlerThreadInitialStackSize = 64 * 1024;

vector<ExceptionHandler *> *ExceptionHandler::handler_stack_ = NULL;
LONG ExceptionHandler::handler_stack_index_ = 0;
CRITICAL_SECTION ExceptionHandler::handler_stack_critical_section_;
bool ExceptionHandler::handler_stack_critical_section_initialized_ = false;

ExceptionHandler::ExceptionHandler(const wstring &dump_path,
                                   FilterCallback filter,
                                   MinidumpCallback callback,
                                   void *callback_context,
                                   bool install_handler)
    : filter_(filter),
      callback_(callback),
      callback_context_(callback_context),
      dump_path_(),
      next_minidump_id_(),
      next_minidump_path_(),
      dump_path_c_(),
      next_minidump_id_c_(NULL),
      next_minidump_path_c_(NULL),
      dbghelp_module_(NULL),
      minidump_write_dump_(NULL),
      installed_handler_(install_handler),
      previous_filter_(NULL),
      handler_thread_(0),
      handler_critical_section_(),
      handler_start_semaphore_(NULL),
      handler_finish_semaphore_(NULL),
      requesting_thread_id_(0),
      exception_info_(NULL),
      handler_return_value_(false) {
  // set_dump_path calls UpdateNextID.  This sets up all of the path and id
  // strings, and their equivalent c_str pointers.
  set_dump_path(dump_path);

  // Set synchronization primitives and the handler thread.  Each
  // ExceptionHandler object gets its own handler thread, even if
  // install_handler is false, because that's the only way to reliably
  // guarantee sufficient stack space in an exception, and the only way to
  // get a snapshot of the requesting thread's context outside of an
  // exception.
  InitializeCriticalSection(&handler_critical_section_);
  handler_start_semaphore_ = CreateSemaphore(NULL, 0, 1, NULL);
  handler_finish_semaphore_ = CreateSemaphore(NULL, 0, 1, NULL);

  DWORD thread_id;
  handler_thread_ = CreateThread(NULL,         // lpThreadAttributes
                                 kExceptionHandlerThreadInitialStackSize,
                                 ExceptionHandlerThreadMain,
                                 this,         // lpParameter
                                 0,            // dwCreationFlags
                                 &thread_id);

  dbghelp_module_ = LoadLibrary(L"dbghelp.dll");
  if (dbghelp_module_) {
    minidump_write_dump_ = reinterpret_cast<MiniDumpWriteDump_type>(
        GetProcAddress(dbghelp_module_, "MiniDumpWriteDump"));
  }

  if (install_handler) {
    if (!handler_stack_critical_section_initialized_) {
      InitializeCriticalSection(&handler_stack_critical_section_);
      handler_stack_critical_section_initialized_ = true;
    }

    EnterCriticalSection(&handler_stack_critical_section_);

    // The first time an ExceptionHandler that installs a handler is
    // created, set up the handler stack.
    if (!handler_stack_) {
      handler_stack_ = new vector<ExceptionHandler *>();
    }
    handler_stack_->push_back(this);
    previous_filter_ = SetUnhandledExceptionFilter(HandleException);

    LeaveCriticalSection(&handler_stack_critical_section_);
  }
}

ExceptionHandler::~ExceptionHandler() {
  if (dbghelp_module_) {
    FreeLibrary(dbghelp_module_);
  }

  if (installed_handler_) {
    EnterCriticalSection(&handler_stack_critical_section_);

    SetUnhandledExceptionFilter(previous_filter_);
    if (handler_stack_->back() == this) {
      handler_stack_->pop_back();
    } else {
      // TODO(mmentovai): use advapi32!ReportEvent to log the warning to the
      // system's application event log.
      fprintf(stderr, "warning: removing Airbag handler out of order\n");
      for (vector<ExceptionHandler *>::iterator iterator =
               handler_stack_->begin();
           iterator != handler_stack_->end();
           ++iterator) {
        if (*iterator == this) {
          handler_stack_->erase(iterator);
        }
      }
    }

    if (handler_stack_->empty()) {
      // When destroying the last ExceptionHandler that installed a handler,
      // clean up the handler stack.
      delete handler_stack_;
      handler_stack_ = NULL;
    }

    LeaveCriticalSection(&handler_stack_critical_section_);
  }

  // Clean up the handler thread and synchronization primitives.
  TerminateThread(handler_thread_, 1);
  DeleteCriticalSection(&handler_critical_section_);
  CloseHandle(handler_start_semaphore_);
  CloseHandle(handler_finish_semaphore_);
}

// static
DWORD ExceptionHandler::ExceptionHandlerThreadMain(void *lpParameter) {
  ExceptionHandler *self = reinterpret_cast<ExceptionHandler *>(lpParameter);
  assert(self);

  while (true) {
    if (WaitForSingleObject(self->handler_start_semaphore_, INFINITE) ==
        WAIT_OBJECT_0) {
      // Perform the requested action.
      self->handler_return_value_ = self->WriteMinidumpWithException(
          self->requesting_thread_id_, self->exception_info_);

      // Allow the requesting thread to proceed.
      ReleaseSemaphore(self->handler_finish_semaphore_, 1, NULL);
    }
  }

  // Not reached.  This thread will be terminated by ExceptionHandler's
  // destructor.
  return 0;
}

// static
LONG ExceptionHandler::HandleException(EXCEPTION_POINTERS *exinfo) {
  // Increment handler_stack_index_ so that if another Airbag handler is
  // registered using this same HandleException function, and it needs to be
  // called while this handler is running (either becaause this handler
  // declines to handle the exception, or an exception occurs during
  // handling), HandleException will find the appropriate ExceptionHandler
  // object in handler_stack_ to deliver the exception to.
  //
  // Because handler_stack_ is addressed in reverse (as |size - index|),
  // preincrementing handler_stack_index_ avoids needing to subtract 1 from
  // the argument to |at|.
  //
  // The index is maintained instead of popping elements off of the handler
  // stack and pushing them at the end of this method.  This avoids ruining
  // the order of elements in the stack in the event that some other thread
  // decides to manipulate the handler stack (such as creating a new
  // ExceptionHandler object) while an exception is being handled.
  EnterCriticalSection(&handler_stack_critical_section_);
  ExceptionHandler *current_handler =
      handler_stack_->at(handler_stack_->size() - ++handler_stack_index_);
  LeaveCriticalSection(&handler_stack_critical_section_);

  // In case another exception occurs while this handler is doing its thing,
  // it should be delivered to the previous filter.
  LPTOP_LEVEL_EXCEPTION_FILTER previous = current_handler->previous_filter_;
  LPTOP_LEVEL_EXCEPTION_FILTER restore = SetUnhandledExceptionFilter(previous);

  LONG action;
  if (current_handler->WriteMinidumpOnHandlerThread(exinfo)) {
    // The handler fully handled the exception.  Returning
    // EXCEPTION_EXECUTE_HANDLER indicates this to the system, and usually
    // results in the applicaiton being terminated.
    //
    // Note: If the application was launched from within the Cygwin
    // environment, returning EXCEPTION_EXECUTE_HANDLER seems to cause the
    // application to be restarted.
    action = EXCEPTION_EXECUTE_HANDLER;
  } else {
    // There was an exception, but the handler decided not to handle it.
    // This could be because the filter callback didn't want it, because
    // minidump writing failed for some reason, or because the post-minidump
    // callback function indicated failure.  Give the previous handler a
    // chance to do something with the exception.  If there is no previous
    // handler, return EXCEPTION_CONTINUE_SEARCH, which will allow a debugger
    // or native "crashed" dialog to handle the exception.
    action = previous ? previous(exinfo) : EXCEPTION_CONTINUE_SEARCH;
  }

  // Put things back the way they were before entering this handler.
  SetUnhandledExceptionFilter(restore);
  EnterCriticalSection(&handler_stack_critical_section_);
  --handler_stack_index_;
  LeaveCriticalSection(&handler_stack_critical_section_);

  return action;
}

bool ExceptionHandler::WriteMinidumpOnHandlerThread(EXCEPTION_POINTERS *exinfo) {
  EnterCriticalSection(&handler_critical_section_);

  // Set up data to be passed in to the handler thread.
  requesting_thread_id_ = GetCurrentThreadId();
  exception_info_ = exinfo;

  // This causes the handler thread to call WriteMinidumpWithException.
  ReleaseSemaphore(handler_start_semaphore_, 1, NULL);

  // Wait until WriteMinidumpWithException is done and collect its return value.
  WaitForSingleObject(handler_finish_semaphore_, INFINITE);
  bool status = handler_return_value_;

  // Clean up.
  requesting_thread_id_ = 0;
  exception_info_ = NULL;

  LeaveCriticalSection(&handler_critical_section_);

  return status;
}

bool ExceptionHandler::WriteMinidump() {
  bool success = WriteMinidumpOnHandlerThread(NULL);
  UpdateNextID();
  return success;
}

// static
bool ExceptionHandler::WriteMinidump(const wstring &dump_path,
                                     MinidumpCallback callback,
                                     void *callback_context) {
  ExceptionHandler handler(dump_path, NULL, callback, callback_context, false);
  return handler.WriteMinidump();
}

bool ExceptionHandler::WriteMinidumpWithException(DWORD requesting_thread_id,
                                                  EXCEPTION_POINTERS *exinfo) {
  // Give user code a chance to approve or prevent writing a minidump.  If the
  // filter returns false, don't handle the exception at all.  If this method
  // was called as a result of an exception, returning false will cause
  // HandleException to call any previous handler or return
  // EXCEPTION_CONTINUE_SEARCH on the exception thread, allowing it to appear
  // as though this handler were not present at all.
  if (filter_&& !filter_(callback_context_)) {
    return false;
  }

  bool success = false;
  if (minidump_write_dump_) {
    HANDLE dump_file = CreateFile(next_minidump_path_c_,
                                  GENERIC_WRITE,
                                  FILE_SHARE_WRITE,
                                  NULL,
                                  CREATE_ALWAYS,
                                  FILE_ATTRIBUTE_NORMAL,
                                  NULL);
    if (dump_file != INVALID_HANDLE_VALUE) {
      MINIDUMP_EXCEPTION_INFORMATION except_info;
      except_info.ThreadId = requesting_thread_id;
      except_info.ExceptionPointers = exinfo;
      except_info.ClientPointers = FALSE;

      // Add an MDRawAirbagInfo stream to the minidump, to provide additional
      // information about the exception handler to the Airbag processor.  The
      // information will help the processor determine which threads are
      // relevant.  The Airbag processor does not require this information but
      // can function better with Airbag-generated dumps when it is present.
      // The native debugger is not harmed by the presence of this information.
      MDRawAirbagInfo airbag_info;
      airbag_info.validity = MD_AIRBAG_INFO_VALID_DUMP_THREAD_ID |
                             MD_AIRBAG_INFO_VALID_REQUESTING_THREAD_ID;
      airbag_info.dump_thread_id = GetCurrentThreadId();
      airbag_info.requesting_thread_id = requesting_thread_id;

      MINIDUMP_USER_STREAM airbag_info_stream;
      airbag_info_stream.Type = MD_AIRBAG_INFO_STREAM;
      airbag_info_stream.BufferSize = sizeof(airbag_info);
      airbag_info_stream.Buffer = &airbag_info;

      MINIDUMP_USER_STREAM_INFORMATION user_streams;
      user_streams.UserStreamCount = 1;
      user_streams.UserStreamArray = &airbag_info_stream;

      // The explicit comparison to TRUE avoids a warning (C4800).
      success = (minidump_write_dump_(GetCurrentProcess(),
                                      GetCurrentProcessId(),
                                      dump_file,
                                      MiniDumpNormal,
                                      exinfo ? &except_info : NULL,
                                      &user_streams,
                                      NULL) == TRUE);

      CloseHandle(dump_file);
    }
  }

  if (callback_) {
    success = callback_(dump_path_c_, next_minidump_id_c_, callback_context_,
                        success);
  }

  return success;
}

void ExceptionHandler::UpdateNextID() {
  GUID id;
  CoCreateGuid(&id);
  next_minidump_id_ = GUIDString::GUIDToWString(&id);
  next_minidump_id_c_ = next_minidump_id_.c_str();

  wchar_t minidump_path[MAX_PATH];
  WindowsStringUtils::safe_swprintf(minidump_path, MAX_PATH, L"%s\\%s.dmp",
                                    dump_path_c_,
                                    next_minidump_id_c_);
  next_minidump_path_ = minidump_path;
  next_minidump_path_c_ = next_minidump_path_.c_str();
}

}  // namespace google_airbag
