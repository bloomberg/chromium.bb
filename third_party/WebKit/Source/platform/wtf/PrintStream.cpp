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

#include "platform/wtf/PrintStream.h"

#include "platform/wtf/text/CString.h"
#include "platform/wtf/text/WTFString.h"
#include <stdio.h>

namespace WTF {

PrintStream::PrintStream() = default;
PrintStream::~PrintStream() = default;  // Force the vtable to be in this module

void PrintStream::Printf(const char* format, ...) {
  va_list arg_list;
  va_start(arg_list, format);
  Vprintf(format, arg_list);
  va_end(arg_list);
}

void PrintStream::Flush() {}

void PrintInternal(PrintStream& out, const char* string) {
  out.Printf("%s", string);
}

void PrintInternal(PrintStream& out, const CString& string) {
  out.Print(string.data());
}

void PrintInternal(PrintStream& out, const String& string) {
  out.Print(string.Utf8());
}

void PrintInternal(PrintStream& out, bool value) {
  if (value)
    out.Print("true");
  else
    out.Print("false");
}

void PrintInternal(PrintStream& out, int value) {
  out.Printf("%d", value);
}

void PrintInternal(PrintStream& out, unsigned value) {
  out.Printf("%u", value);
}

void PrintInternal(PrintStream& out, long value) {
  out.Printf("%ld", value);
}

void PrintInternal(PrintStream& out, unsigned long value) {
  out.Printf("%lu", value);
}

void PrintInternal(PrintStream& out, long long value) {
  out.Printf("%lld", value);
}

void PrintInternal(PrintStream& out, unsigned long long value) {
  out.Printf("%llu", value);
}

void PrintInternal(PrintStream& out, float value) {
  out.Print(static_cast<double>(value));
}

void PrintInternal(PrintStream& out, double value) {
  out.Printf("%lf", value);
}

void DumpCharacter(PrintStream& out, char value) {
  out.Printf("%c", value);
}

}  // namespace WTF
