/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#include "public/platform/WebString.h"

#include "base/strings/string_util.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/text/ASCIIFastPath.h"
#include "platform/wtf/text/AtomicString.h"
#include "platform/wtf/text/CString.h"
#include "platform/wtf/text/StringUTF8Adaptor.h"
#include "platform/wtf/text/StringView.h"
#include "platform/wtf/text/WTFString.h"


STATIC_ASSERT_ENUM(WTF::kLenientUTF8Conversion,
                   blink::WebString::UTF8ConversionMode::kLenient);
STATIC_ASSERT_ENUM(WTF::kStrictUTF8Conversion,
                   blink::WebString::UTF8ConversionMode::kStrict);
STATIC_ASSERT_ENUM(
    WTF::kStrictUTF8ConversionReplacingUnpairedSurrogatesWithFFFD,
    blink::WebString::UTF8ConversionMode::kStrictReplacingErrorsWithFFFD);

namespace blink {

void WebString::Reset() {
  private_.Reset();
}

void WebString::Assign(const WebString& other) {
  Assign(other.private_.Get());
}

void WebString::Assign(const WebUChar* data, size_t length) {
  Assign(StringImpl::Create8BitIfPossible(data, length).get());
}

size_t WebString::length() const {
  return private_.IsNull() ? 0 : private_->length();
}

bool WebString::Is8Bit() const {
  return private_->Is8Bit();
}

const WebLChar* WebString::Data8() const {
  return !private_.IsNull() && Is8Bit() ? private_->Characters8() : nullptr;
}

const WebUChar* WebString::Data16() const {
  return !private_.IsNull() && !Is8Bit() ? private_->Characters16() : nullptr;
}

std::string WebString::Utf8(UTF8ConversionMode mode) const {
  StringUTF8Adaptor utf8(private_.Get(),
                         static_cast<WTF::UTF8ConversionMode>(mode));
  return std::string(utf8.Data(), utf8.length());
}

WebString WebString::FromUTF8(const char* data, size_t length) {
  return String::FromUTF8(data, length);
}

WebString WebString::FromUTF16(const base::string16& s) {
  WebString string;
  string.Assign(s.data(), s.length());
  return string;
}

WebString WebString::FromUTF16(const base::NullableString16& s) {
  WebString string;
  if (s.is_null())
    string.Reset();
  else
    string.Assign(s.string().data(), s.string().length());
  return string;
}

WebString WebString::FromUTF16(const base::Optional<base::string16>& s) {
  WebString string;
  if (!s)
    string.Reset();
  else
    string.Assign(s->data(), s->length());
  return string;
}

std::string WebString::Latin1() const {
  String string(private_.Get());

  if (string.IsEmpty())
    return std::string();

  if (string.Is8Bit())
    return std::string(reinterpret_cast<const char*>(string.Characters8()),
                       string.length());

  CString latin1 = string.Latin1();
  return std::string(latin1.data(), latin1.length());
}

WebString WebString::FromLatin1(const WebLChar* data, size_t length) {
  return String(data, length);
}

std::string WebString::Ascii() const {
  DCHECK(ContainsOnlyASCII());

  if (IsEmpty())
    return std::string();

  if (private_->Is8Bit()) {
    return std::string(reinterpret_cast<const char*>(private_->Characters8()),
                       private_->length());
  }

  return std::string(private_->Characters16(),
                     private_->Characters16() + private_->length());
}

bool WebString::ContainsOnlyASCII() const {
  return String(private_.Get()).ContainsOnlyASCII();
}

WebString WebString::FromASCII(const std::string& s) {
  DCHECK(base::IsStringASCII(s));
  return FromLatin1(s);
}

bool WebString::Equals(const WebString& s) const {
  return Equal(private_.Get(), s.private_.Get());
}

bool WebString::Equals(const char* characters, size_t length) const {
  return Equal(private_.Get(), characters, length);
}

WebString::WebString(const WTF::String& s) : private_(s.Impl()) {}

WebString& WebString::operator=(const WTF::String& s) {
  Assign(s.Impl());
  return *this;
}

WebString::operator WTF::String() const {
  return private_.Get();
}

WebString::operator WTF::StringView() const {
  return StringView(private_.Get());
}

WebString::WebString(const WTF::AtomicString& s) {
  Assign(s.GetString());
}

WebString& WebString::operator=(const WTF::AtomicString& s) {
  Assign(s.GetString());
  return *this;
}

WebString::operator WTF::AtomicString() const {
  return WTF::AtomicString(private_.Get());
}

void WebString::Assign(WTF::StringImpl* p) {
  private_ = p;
}

}  // namespace blink
