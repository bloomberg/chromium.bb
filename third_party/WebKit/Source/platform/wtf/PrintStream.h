/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef PrintStream_h
#define PrintStream_h

#include "platform/wtf/Allocator.h"
#include "platform/wtf/Compiler.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/StdLibExtras.h"
#include "platform/wtf/WTFExport.h"
#include <stdarg.h>

namespace WTF {

class CString;
class String;

class WTF_EXPORT PrintStream {
  USING_FAST_MALLOC(PrintStream);
  WTF_MAKE_NONCOPYABLE(PrintStream);

 public:
  PrintStream();
  virtual ~PrintStream();

  PRINTF_FORMAT(2, 3) void Printf(const char* format, ...);
  PRINTF_FORMAT(2, 0) virtual void Vprintf(const char* format, va_list) = 0;

  // Typically a no-op for many subclasses of PrintStream, this is a hint that
  // the implementation should flush its buffers if it had not done so already.
  virtual void Flush();

  template <typename T>
  void Print(const T& value) {
    PrintInternal(*this, value);
  }

  template <typename T1, typename... RemainingTypes>
  void Print(const T1& value1, const RemainingTypes&... values) {
    Print(value1);
    Print(values...);
  }
};

WTF_EXPORT void PrintInternal(PrintStream&, const char*);
WTF_EXPORT void PrintInternal(PrintStream&, const CString&);
WTF_EXPORT void PrintInternal(PrintStream&, const String&);
inline void PrintInternal(PrintStream& out, char* value) {
  PrintInternal(out, static_cast<const char*>(value));
}
inline void PrintInternal(PrintStream& out, CString& value) {
  PrintInternal(out, static_cast<const CString&>(value));
}
inline void PrintInternal(PrintStream& out, String& value) {
  PrintInternal(out, static_cast<const String&>(value));
}
WTF_EXPORT void PrintInternal(PrintStream&, bool);
WTF_EXPORT void PrintInternal(PrintStream&, int);
WTF_EXPORT void PrintInternal(PrintStream&, unsigned);
WTF_EXPORT void PrintInternal(PrintStream&, long);
WTF_EXPORT void PrintInternal(PrintStream&, unsigned long);
WTF_EXPORT void PrintInternal(PrintStream&, long long);
WTF_EXPORT void PrintInternal(PrintStream&, unsigned long long);
WTF_EXPORT void PrintInternal(PrintStream&, float);
WTF_EXPORT void PrintInternal(PrintStream&, double);

template <typename T>
void PrintInternal(PrintStream& out, const T& value) {
  value.Dump(out);
}

}  // namespace WTF

using WTF::PrintStream;

#endif  // PrintStream_h
