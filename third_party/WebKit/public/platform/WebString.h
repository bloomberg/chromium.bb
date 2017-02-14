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

#ifndef WebString_h
#define WebString_h

#include "WebCommon.h"
#include "WebPrivatePtr.h"
#include "base/strings/latin1_string_conversions.h"
#include "base/strings/nullable_string16.h"
#include "base/strings/string16.h"
#include <string>

#if INSIDE_BLINK
#include "wtf/Forward.h"
#endif

namespace WTF {
class StringImpl;
}

namespace blink {

// Use either one of static methods to convert ASCII, Latin1, UTF-8 or
// UTF-16 string into WebString:
//
// * WebString::fromASCII(const std::string& ascii)
// * WebString::fromLatin1(const std::string& latin1)
// * WebString::fromUTF8(const std::string& utf8)
// * WebString::fromUTF16(const base::string16& utf16)
// * WebString::fromUTF16(const base::NullableString16& utf16)
//
// Similarly, use either of following methods to convert WebString to
// ASCII, Latin1, UTF-8 or UTF-16:
//
// * webstring.ascii()
// * webstring.latin1()
// * webstring.utf8()
// * webstring.utf16()
// * WebString::toNullableString16(webstring)
//
// Note that if you need to convert the UTF8 string converted from WebString
// back to WebString with fromUTF8() you may want to specify Strict
// UTF8ConversionMode when you call utf8(), as fromUTF8 rejects strings
// with invalid UTF8 characters.
//
// Some types like GURL and base::FilePath can directly take either utf-8 or
// utf-16 strings. Use following methods to convert WebString to/from GURL or
// FilePath rather than going through intermediate string types:
//
// * GURL WebStringToGURL(const WebString&)
// * base::FilePath WebStringToFilePath(const WebString&)
// * WebString FilePathToWebString(const base::FilePath&);
//
// It is inexpensive to copy a WebString object.
// WARNING: It is not safe to pass a WebString across threads!!!
//
class WebString {
 public:
  enum class UTF8ConversionMode {
    // Ignores errors for invalid characters.
    kLenient,
    // Errors out on invalid characters, returns null string.
    kStrict,
    // Replace invalid characters with 0xFFFD.
    // (This is the same conversion mode as base::UTF16ToUTF8)
    kStrictReplacingErrorsWithFFFD,
  };

  ~WebString() { reset(); }

  WebString() {}

  WebString(const WebUChar* data, size_t len) { assign(data, len); }

  WebString(const WebString& s) { assign(s); }

  WebString& operator=(const WebString& s) {
    assign(s);
    return *this;
  }

  BLINK_PLATFORM_EXPORT void reset();
  BLINK_PLATFORM_EXPORT void assign(const WebString&);
  BLINK_PLATFORM_EXPORT void assign(const WebUChar* data, size_t len);

  BLINK_PLATFORM_EXPORT bool equals(const WebString&) const;
  BLINK_PLATFORM_EXPORT bool equals(const char* characters, size_t len) const;
  bool equals(const char* characters) const {
    return equals(characters, characters ? strlen(characters) : 0);
  }

  BLINK_PLATFORM_EXPORT size_t length() const;

  bool isEmpty() const { return !length(); }
  bool isNull() const { return m_private.isNull(); }

  BLINK_PLATFORM_EXPORT std::string utf8(
      UTF8ConversionMode = UTF8ConversionMode::kLenient) const;

  BLINK_PLATFORM_EXPORT static WebString fromUTF8(const char* data,
                                                  size_t length);
  static WebString fromUTF8(const std::string& s) {
    return fromUTF8(s.data(), s.length());
  }

  base::string16 utf16() const {
    return base::Latin1OrUTF16ToUTF16(length(), data8(), data16());
  }

  BLINK_PLATFORM_EXPORT static WebString fromUTF16(const base::string16&);

  BLINK_PLATFORM_EXPORT static WebString fromUTF16(
      const base::NullableString16&);

  static base::NullableString16 toNullableString16(const WebString& s) {
    return base::NullableString16(s.utf16(), s.isNull());
  }

  BLINK_PLATFORM_EXPORT std::string latin1() const;

  BLINK_PLATFORM_EXPORT static WebString fromLatin1(const WebLChar* data,
                                                    size_t length);

  static WebString fromLatin1(const std::string& s) {
    return fromLatin1(reinterpret_cast<const WebLChar*>(s.data()), s.length());
  }

  // This asserts if the string contains non-ascii characters.
  // Use this rather than calling base::UTF16ToASCII() which always incurs
  // (likely unnecessary) string16 conversion.
  BLINK_PLATFORM_EXPORT std::string ascii() const;

  // Use this rather than calling base::IsStringASCII().
  BLINK_PLATFORM_EXPORT bool containsOnlyASCII() const;

  // Does same as fromLatin1 but asserts if the given string has non-ascii char.
  BLINK_PLATFORM_EXPORT static WebString fromASCII(const std::string&);

  template <int N>
  WebString(const char (&data)[N]) {
    assign(fromUTF8(data, N - 1));
  }

  template <int N>
  WebString& operator=(const char (&data)[N]) {
    assign(fromUTF8(data, N - 1));
    return *this;
  }

#if INSIDE_BLINK
  BLINK_PLATFORM_EXPORT WebString(const WTF::String&);
  BLINK_PLATFORM_EXPORT WebString& operator=(const WTF::String&);
  BLINK_PLATFORM_EXPORT operator WTF::String() const;

  BLINK_PLATFORM_EXPORT operator WTF::StringView() const;

  BLINK_PLATFORM_EXPORT WebString(const WTF::AtomicString&);
  BLINK_PLATFORM_EXPORT WebString& operator=(const WTF::AtomicString&);
  BLINK_PLATFORM_EXPORT operator WTF::AtomicString() const;
#endif

 private:
  BLINK_PLATFORM_EXPORT bool is8Bit() const;
  BLINK_PLATFORM_EXPORT const WebLChar* data8() const;
  BLINK_PLATFORM_EXPORT const WebUChar* data16() const;

  BLINK_PLATFORM_EXPORT void assign(WTF::StringImpl*);

  WebPrivatePtr<WTF::StringImpl> m_private;
};

inline bool operator==(const WebString& a, const char* b) {
  return a.equals(b);
}

inline bool operator!=(const WebString& a, const char* b) {
  return !(a == b);
}

inline bool operator==(const char* a, const WebString& b) {
  return b == a;
}

inline bool operator!=(const char* a, const WebString& b) {
  return !(b == a);
}

inline bool operator==(const WebString& a, const WebString& b) {
  return a.equals(b);
}

inline bool operator!=(const WebString& a, const WebString& b) {
  return !(a == b);
}

}  // namespace blink

#endif
