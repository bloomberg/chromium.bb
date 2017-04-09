/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#include "public/platform/WebCString.h"

#include <string.h>
#include "platform/wtf/text/CString.h"
#include "public/platform/WebString.h"

namespace blink {

int WebCString::Compare(const WebCString& other) const {
  // A null string is always less than a non null one.
  if (IsNull() != other.IsNull())
    return IsNull() ? -1 : 1;

  if (IsNull())
    return 0;  // Both WebStrings are null.

  return strcmp(private_->Data(), other.private_->Data());
}

void WebCString::Reset() {
  private_.Reset();
}

void WebCString::Assign(const WebCString& other) {
  Assign(other.private_.Get());
}

void WebCString::Assign(const char* data, size_t length) {
  Assign(WTF::CString(data, length).Impl());
}

size_t WebCString::length() const {
  return private_.IsNull() ? 0 : private_->length();
}

const char* WebCString::Data() const {
  return private_.IsNull() ? 0 : private_->Data();
}

WebString WebCString::Utf16() const {
  return WebString::FromUTF8(Data(), length());
}

WebCString::WebCString(const WTF::CString& s) {
  Assign(s.Impl());
}

WebCString& WebCString::operator=(const WTF::CString& s) {
  Assign(s.Impl());
  return *this;
}

WebCString::operator WTF::CString() const {
  return private_.Get();
}

void WebCString::Assign(WTF::CStringImpl* p) {
  private_ = p;
}

}  // namespace blink
