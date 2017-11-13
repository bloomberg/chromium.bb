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

WebString::~WebString() = default;
WebString::WebString() = default;
WebString::WebString(const WebString&) = default;
WebString::WebString(WebString&&) = default;
WebString& WebString::operator=(const WebString&) = default;
WebString& WebString::operator=(WebString&&) = default;

WebString::WebString(const WebUChar* data, size_t len)
    : impl_(StringImpl::Create8BitIfPossible(data, len)) {}

void WebString::Reset() {
  impl_ = nullptr;
}

size_t WebString::length() const {
  return impl_ ? impl_->length() : 0;
}

bool WebString::Is8Bit() const {
  return impl_->Is8Bit();
}

const WebLChar* WebString::Data8() const {
  return impl_ && Is8Bit() ? impl_->Characters8() : nullptr;
}

const WebUChar* WebString::Data16() const {
  return impl_ && !Is8Bit() ? impl_->Characters16() : nullptr;
}

std::string WebString::Utf8(UTF8ConversionMode mode) const {
  StringUTF8Adaptor utf8(impl_.get(),
                         static_cast<WTF::UTF8ConversionMode>(mode));
  return std::string(utf8.Data(), utf8.length());
}

WebString WebString::FromUTF8(const char* data, size_t length) {
  return String::FromUTF8(data, length);
}

WebString WebString::FromUTF16(const base::string16& s) {
  return WebString(s.data(), s.length());
}

WebString WebString::FromUTF16(const base::NullableString16& s) {
  if (s.is_null())
    return WebString();
  return WebString(s.string().data(), s.string().length());
}

WebString WebString::FromUTF16(const base::Optional<base::string16>& s) {
  if (!s.has_value())
    return WebString();
  return WebString(s->data(), s->length());
}

std::string WebString::Latin1() const {
  String string(impl_);

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

  if (impl_->Is8Bit()) {
    return std::string(reinterpret_cast<const char*>(impl_->Characters8()),
                       impl_->length());
  }

  return std::string(impl_->Characters16(),
                     impl_->Characters16() + impl_->length());
}

bool WebString::ContainsOnlyASCII() const {
  return String(impl_).ContainsOnlyASCII();
}

WebString WebString::FromASCII(const std::string& s) {
  DCHECK(base::IsStringASCII(s));
  return FromLatin1(s);
}

bool WebString::Equals(const WebString& s) const {
  return Equal(impl_.get(), s.impl_.get());
}

bool WebString::Equals(const char* characters, size_t length) const {
  return Equal(impl_.get(), characters, length);
}

WebString::WebString(const WTF::String& s) : impl_(s.Impl()) {}

WebString& WebString::operator=(const WTF::String& s) {
  impl_ = s.Impl();
  return *this;
}

WebString::operator WTF::String() const {
  return impl_.get();
}

WebString::operator WTF::StringView() const {
  return StringView(impl_.get());
}

WebString::WebString(const WTF::AtomicString& s) {
  impl_ = s.Impl();
}

WebString& WebString::operator=(const WTF::AtomicString& s) {
  impl_ = s.Impl();
  return *this;
}

WebString::operator WTF::AtomicString() const {
  return WTF::AtomicString(impl_);
}

}  // namespace blink
