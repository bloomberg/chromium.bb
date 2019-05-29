/*
 * Copyright (C) 2013 Bloomberg Finance L.P.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS," WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef INCLUDED_BLPWTK2_STRING_H
#define INCLUDED_BLPWTK2_STRING_H

#include <blpwtk2_config.h>
#include <blpwtk2_stringref.h>
#include <string>

namespace blpwtk2 {

class BLPWTK2_EXPORT String {
    typedef char* Impl;

    static Impl make(const char* str, size_t length);
    static Impl make(const wchar_t* str, size_t length);
    static Impl make(Impl);
    static void unmake(Impl);
    static size_t length(Impl);

  public:
    String() : d_impl(0) { }
    String(const String& orig) : d_impl(orig.d_impl ? make(orig.d_impl) : 0) {}
    explicit String(const char* str) : d_impl(make(str, strlen(str))) {}
    String(const char* str, size_t length) : d_impl(make(str, length)) {}
    String(const wchar_t* str, size_t length) : d_impl(make(str, length)) {}
    explicit String(const StringRef& str) : d_impl(make(str.data(), str.length())) {}
    explicit String(const std::string& str) : d_impl(make(str.data(), str.length())) {}
    explicit String(const std::wstring& str) : d_impl(make(str.data(), str.length())) {}
    ~String();

    String& operator=(const String& rhs)
    {
        if (&rhs != this) {
            char* impl = rhs.d_impl ? make(rhs.d_impl) : 0;
            if (d_impl) unmake(d_impl);
            d_impl = impl;
        }
        return *this;
    }

    void reset() { if (d_impl) { unmake(d_impl); d_impl = 0; } }
    void assign(const char* str)
    {
        char* impl = make(str, strlen(str));
        if (d_impl) unmake(d_impl);
        d_impl = impl;
    }
    void assign(const char* str, size_t length)
    {
        char* impl = make(str, length);
        if (d_impl) unmake(d_impl);
        d_impl = impl;
    }
    void assign(const StringRef& str)
    {
        char* impl = make(str.data(), str.length());
        if (d_impl) unmake(d_impl);
        d_impl = impl;
    }

    const char* data() const { return d_impl; }
    const char* c_str() const;
    size_t length() const { return d_impl ? length(d_impl) : 0; }
    size_t size() const { return length(); }
    int isEmpty() const { return !d_impl; }
    bool equals(const StringRef& other) const
    {
        return 0 == StringRef::compare(*this, other);
    }

    operator StringRef() const
    {
        return StringRef(d_impl, d_impl ? length(d_impl) : 0);
    }

private:
    Impl d_impl;
};

}  // close namespace blpwtk2

#ifdef BLPWTK2_IMPLEMENTATION
namespace blink {
    class WebString;
}  // close namespace blink
namespace blpwtk2 {
String fromWebString(const blink::WebString&);
}  // close namespace blpwtk2
#endif  // BLPWTK2_IMPLEMENTATION

#endif  // INCLUDED_BLPWTK2_STRING_H

// vim: ts=4 et

