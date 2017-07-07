/*
 * Copyright (C) 2003, 2006, 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2007-2009 Torch Mobile, Inc.
 * Copyright (C) 2011 University of Szeged. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// The vprintf_stderr_common function triggers this error in the Mac build.
// Feel free to remove this pragma if this file builds on Mac.
// According to
// http://gcc.gnu.org/onlinedocs/gcc-4.2.1/gcc/Diagnostic-Pragmas.html#Diagnostic-Pragmas
// we need to place this directive before any data or functions are defined.
#pragma GCC diagnostic ignored "-Wmissing-format-attribute"

#include "platform/wtf/Assertions.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory>
#include "build/build_config.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/ThreadSpecific.h"
#include "platform/wtf/Threading.h"

#if defined(OS_MACOSX)
#include <AvailabilityMacros.h>
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 1080
#define WTF_USE_APPLE_SYSTEM_LOG 1
#include <asl.h>
#endif
#endif  // defined(OS_MACOSX)

#if defined(COMPILER_MSVC)
#include <crtdbg.h>
#endif

#if defined(OS_WIN)
#include <windows.h>
#endif

#if defined(OS_MACOSX) || (defined(OS_LINUX) && !defined(__UCLIBC__))
#include <cxxabi.h>
#include <dlfcn.h>
#include <execinfo.h>
#endif

#if defined(OS_ANDROID)
#include <android/log.h>
#endif

PRINTF_FORMAT(1, 0)
static void vprintf_stderr_common(const char* format, va_list args) {
#if defined(OS_MACOSX) && USE(APPLE_SYSTEM_LOG)
  va_list copyOfArgs;
  va_copy(copyOfArgs, args);
  asl_vlog(0, 0, ASL_LEVEL_NOTICE, format, copyOfArgs);
  va_end(copyOfArgs);
#elif defined(OS_ANDROID)
  __android_log_vprint(ANDROID_LOG_WARN, "WebKit", format, args);
#elif defined(OS_WIN)
  if (IsDebuggerPresent()) {
    size_t size = 1024;

    do {
      char* buffer = (char*)malloc(size);
      if (!buffer)
        break;

      if (_vsnprintf(buffer, size, format, args) != -1) {
        OutputDebugStringA(buffer);
        free(buffer);
        break;
      }

      free(buffer);
      size *= 2;
    } while (size > 1024);
  }
#endif
  vfprintf(stderr, format, args);
}

#if defined(COMPILER_GCC)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif

static void vprintf_stderr_with_trailing_newline(const char* format,
                                                 va_list args) {
  size_t formatLength = strlen(format);
  if (formatLength && format[formatLength - 1] == '\n') {
    vprintf_stderr_common(format, args);
    return;
  }

  std::unique_ptr<char[]> formatWithNewline =
      WrapArrayUnique(new char[formatLength + 2]);
  memcpy(formatWithNewline.get(), format, formatLength);
  formatWithNewline[formatLength] = '\n';
  formatWithNewline[formatLength + 1] = 0;

  vprintf_stderr_common(formatWithNewline.get(), args);
}

#if defined(COMPILER_GCC)
#pragma GCC diagnostic pop
#endif

#if DCHECK_IS_ON()
namespace WTF {

ScopedLogger::ScopedLogger(bool condition, const char* format, ...)
    : parent_(condition ? Current() : 0), multiline_(false) {
  if (!condition)
    return;

  va_list args;
  va_start(args, format);
  Init(format, args);
  va_end(args);
}

ScopedLogger::~ScopedLogger() {
  if (Current() == this) {
    if (multiline_)
      Indent();
    else
      Print(" ");
    Print(")\n");
    Current() = parent_;
  }
}

void ScopedLogger::SetPrintFuncForTests(PrintFunctionPtr ptr) {
  print_func_ = ptr;
};

void ScopedLogger::Init(const char* format, va_list args) {
  Current() = this;
  if (parent_)
    parent_->WriteNewlineIfNeeded();
  Indent();
  Print("( ");
  print_func_(format, args);
}

void ScopedLogger::WriteNewlineIfNeeded() {
  if (!multiline_) {
    Print("\n");
    multiline_ = true;
  }
}

void ScopedLogger::Indent() {
  if (parent_) {
    parent_->Indent();
    PrintIndent();
  }
}

void ScopedLogger::Log(const char* format, ...) {
  if (Current() != this)
    return;

  va_list args;
  va_start(args, format);

  WriteNewlineIfNeeded();
  Indent();
  PrintIndent();
  print_func_(format, args);
  Print("\n");

  va_end(args);
}

void ScopedLogger::Print(const char* format, ...) {
  va_list args;
  va_start(args, format);
  print_func_(format, args);
  va_end(args);
}

void ScopedLogger::PrintIndent() {
  Print("  ");
}

ScopedLogger*& ScopedLogger::Current() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(ThreadSpecific<ScopedLogger*>, ref, ());
  return *ref;
}

ScopedLogger::PrintFunctionPtr ScopedLogger::print_func_ =
    vprintf_stderr_common;

}  // namespace WTF
#endif  // DCHECK_IS_ON

void WTFLogAlways(const char* format, ...) {
  va_list args;
  va_start(args, format);
  vprintf_stderr_with_trailing_newline(format, args);
  va_end(args);
}
