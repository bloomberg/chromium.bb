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

#ifndef INCLUDED_BLPWTK2_STRINGREF_H
#define INCLUDED_BLPWTK2_STRINGREF_H

#include <blpwtk2_config.h>
#include <string>

namespace blpwtk2 {

class StringRef {
  public:
    static int compare(const StringRef& lhs, const StringRef& rhs)
    {
        if (lhs.d_length == rhs.d_length) return strncmp(lhs.d_data, rhs.d_data, lhs.d_length);
        if (lhs.d_length < rhs.d_length) {
            int cmp = strncmp(lhs.d_data, rhs.d_data, lhs.d_length);
            return cmp ? cmp : -1;
        }
        int cmp = strncmp(lhs.d_data, rhs.d_data, rhs.d_length);
        return cmp ? cmp : 1;
    }

    StringRef() : d_data(0), d_length(0) {}
    StringRef(const char* str) : d_data(str), d_length(strlen(str)) {}
    StringRef(const char* str, size_t len) : d_data(str), d_length(len) {}
    StringRef(const std::string& str) : d_data(str.data()), d_length(str.length()) {}

    void clear() { d_data = 0; d_length = 0; }
    void assign(const char* str) { d_data = str; d_length = strlen(str); }
    void assign(const char* str, size_t len) { d_data = str; d_length = len; }

    const char* data() const { return d_data; }
    size_t length() const { return d_length; }
    size_t size() const { return length(); }
    bool isEmpty() const { return 0 == d_length; }
    bool equals(const StringRef& other) const { return 0 == compare(*this, other); }
    std::string toStdString() const { return std::string(d_data, d_length); }

  private:
    const char* d_data;
    size_t d_length;
};

}  // close namespace blpwtk2

#ifdef BLPWTK2_IMPLEMENTATION
namespace blink {
    class WebString;
}  // close namespace blink
namespace blpwtk2 {
blink::WebString toWebString(const StringRef&);
}  // close namespace blpwtk2
#endif  // BLPWTK2_IMPLEMENTATION

#endif  // INCLUDED_BLPWTK2_STRINGREF_H

// vim: ts=4 et

