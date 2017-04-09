/*
 * Copyright (C) 2005, 2006, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/text/LineEnding.h"

#include "platform/wtf/Allocator.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/text/CString.h"
#include "platform/wtf/text/WTFString.h"

namespace {

class OutputBuffer {
  STACK_ALLOCATED();
  WTF_MAKE_NONCOPYABLE(OutputBuffer);

 public:
  OutputBuffer() {}
  virtual char* allocate(size_t) = 0;
  virtual void copy(const CString&) = 0;
  virtual ~OutputBuffer() {}
};

class CStringBuffer final : public OutputBuffer {
 public:
  CStringBuffer(CString& buffer) : m_buffer(buffer) {}
  ~CStringBuffer() override {}

  char* allocate(size_t size) override {
    char* ptr;
    m_buffer = CString::CreateUninitialized(size, ptr);
    return ptr;
  }

  void copy(const CString& source) override { m_buffer = source; }

  const CString& buffer() const { return m_buffer; }

 private:
  CString m_buffer;
};

class VectorCharAppendBuffer final : public OutputBuffer {
 public:
  VectorCharAppendBuffer(Vector<char>& buffer) : m_buffer(buffer) {}
  ~VectorCharAppendBuffer() override {}

  char* allocate(size_t size) override {
    size_t oldSize = m_buffer.size();
    m_buffer.Grow(oldSize + size);
    return m_buffer.Data() + oldSize;
  }

  void copy(const CString& source) override {
    m_buffer.Append(source.Data(), source.length());
  }

 private:
  Vector<char>& m_buffer;
};

void internalNormalizeLineEndingsToCRLF(const CString& from,
                                        OutputBuffer& buffer) {
  // Compute the new length.
  size_t newLen = 0;
  const char* p = from.Data();
  while (p < from.Data() + from.length()) {
    char c = *p++;
    if (c == '\r') {
      // Safe to look ahead because of trailing '\0'.
      if (*p != '\n') {
        // Turn CR into CRLF.
        newLen += 2;
      }
    } else if (c == '\n') {
      // Turn LF into CRLF.
      newLen += 2;
    } else {
      // Leave other characters alone.
      newLen += 1;
    }
  }
  if (newLen < from.length())
    return;

  if (newLen == from.length()) {
    buffer.copy(from);
    return;
  }

  p = from.Data();
  char* q = buffer.allocate(newLen);

  // Make a copy of the string.
  while (p < from.Data() + from.length()) {
    char c = *p++;
    if (c == '\r') {
      // Safe to look ahead because of trailing '\0'.
      if (*p != '\n') {
        // Turn CR into CRLF.
        *q++ = '\r';
        *q++ = '\n';
      }
    } else if (c == '\n') {
      // Turn LF into CRLF.
      *q++ = '\r';
      *q++ = '\n';
    } else {
      // Leave other characters alone.
      *q++ = c;
    }
  }
}

}  // namespace;

namespace blink {

void NormalizeLineEndingsToLF(const CString& from, Vector<char>& result) {
  // Compute the new length.
  size_t new_len = 0;
  bool need_fix = false;
  const char* p = from.Data();
  char from_ending_char = '\r';
  char to_ending_char = '\n';
  while (p < from.Data() + from.length()) {
    char c = *p++;
    if (c == '\r' && *p == '\n') {
      // Turn CRLF into CR or LF.
      p++;
      need_fix = true;
    } else if (c == from_ending_char) {
      // Turn CR/LF into LF/CR.
      need_fix = true;
    }
    new_len += 1;
  }

  // Grow the result buffer.
  p = from.Data();
  size_t old_result_size = result.size();
  result.Grow(old_result_size + new_len);
  char* q = result.Data() + old_result_size;

  // If no need to fix the string, just copy the string over.
  if (!need_fix) {
    memcpy(q, p, from.length());
    return;
  }

  // Make a copy of the string.
  while (p < from.Data() + from.length()) {
    char c = *p++;
    if (c == '\r' && *p == '\n') {
      // Turn CRLF or CR into CR or LF.
      p++;
      *q++ = to_ending_char;
    } else if (c == from_ending_char) {
      // Turn CR/LF into LF/CR.
      *q++ = to_ending_char;
    } else {
      // Leave other characters alone.
      *q++ = c;
    }
  }
}

CString NormalizeLineEndingsToCRLF(const CString& from) {
  if (!from.length())
    return from;
  CString result;
  CStringBuffer buffer(result);
  internalNormalizeLineEndingsToCRLF(from, buffer);
  return buffer.buffer();
}

void NormalizeLineEndingsToNative(const CString& from, Vector<char>& result) {
#if OS(WIN)
  VectorCharAppendBuffer buffer(result);
  internalNormalizeLineEndingsToCRLF(from, buffer);
#else
  NormalizeLineEndingsToLF(from, result);
#endif
}

}  // namespace blink
