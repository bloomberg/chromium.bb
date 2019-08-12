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

#include <blpwtk2_string.h>

#include <base/logging.h>  // for DCHECK
#include <base/strings/utf_string_conversions.h>
#include <third_party/blink/public/platform/web_string.h>

namespace blpwtk2 {
String::~String()
{
    if (d_impl) unmake(d_impl);
}

String::Impl String::make(const char* str, size_t length)
{
    if (0 == length) return 0;
    DCHECK(str);

    size_t* lenPtr = (size_t*)malloc(sizeof(size_t) + length + 1);
    *lenPtr = length;

    char* ret = {reinterpret_cast<char*>(lenPtr + 1)};
    memcpy(ret, str, length);
    ret[length] = 0;
    return ret;
}

String::Impl String::make(const wchar_t* str, size_t length)
{
    // TODO: There is an extra copy going from:
    // TODO:     wchar_t* -> std::string -> blpwtk2::String
    // TODO: We should optimize this to do:
    // TODO:     wchar_t* -> blpwtk2::String
    std::string tmp;
    base::WideToUTF8(str, length, &tmp);
    return make(tmp.data(), tmp.length());
}

String::Impl String::make(Impl impl)
{
    DCHECK(impl);

    const char* str = impl;
    size_t length = *(reinterpret_cast<size_t*>(impl) - 1);
    DCHECK(0 < length);

    size_t* lenPtr = (size_t*)malloc(sizeof(size_t) + length + 1);
    *lenPtr = length;

    char* ret = {reinterpret_cast<char*>(lenPtr + 1)};
    memcpy(ret, str, length);
    ret[length] = 0;
    return ret;
}

void String::unmake(Impl impl)
{
    DCHECK(impl);
    size_t* realPtr = reinterpret_cast<size_t*>(impl) - 1;
    free(realPtr);
}

size_t String::length(Impl impl)
{
    DCHECK(impl);
    return *(reinterpret_cast<size_t*>(impl) - 1);
}

const char* String::c_str() const
{
    return d_impl ? d_impl : "";
}

String fromWebString(const blink::WebString& other)
{
    std::string cstr = other.Utf8();
    // TODO: see if we can "steal" this data from WebCString instead of
    // TODO: copying it
    return String(cstr.data(), cstr.length());
}

}  // close namespace blpwtk2

// vim: ts=4 et

